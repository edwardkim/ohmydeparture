#include "IcnDepartures.h"
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QTimeZone>
#include <algorithm>

namespace {
const QTimeZone seoul("Asia/Seoul");

QDateTime timestamp(const QString& value) {
    const QDate date = QDate::fromString(value.left(8), "yyyyMMdd");
    const QTime time = QTime::fromString(value.mid(8), "HHmm");
    const QDateTime result(date, time, seoul);
    return value.size() == 12 && result.isValid() && result.toString("yyyyMMddHHmm") == value
        ? result : QDateTime();
}

QJsonObject resource(const char* path) {
    QFile file(QString::fromLatin1(path));
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
}

struct Names {
    QHash<QString, QJsonObject> cities, airports;
    Names() {
        for (const auto& value : resource(":/data/airport-codes.json")["items"].toArray()) {
            const auto item = value.toObject(); cities.insert(item["cityCode"].toString(), item);
        }
        QSet<QString> ambiguous;
        for (const auto& value : resource(":/data/airport-details.json")["items"].toArray()) {
            const auto item = value.toObject(); const auto code = item["iataCode"].toString();
            if (code.isEmpty()) continue;
            if (airports.contains(code)) ambiguous.insert(code);
            airports.insert(code, item);
        }
        for (const auto& code : ambiguous) airports.remove(code);
    }
};

void describeRemark(IcnDeparture& flight, const QString& remark) {
    flight.remarkKorean = remark.isEmpty() ? QStringLiteral("예정") : remark;
    flight.remarkEnglish = "STATUS UNKNOWN";
    if (remark.isEmpty()) flight.remarkEnglish = "SCHEDULED";
    else if (remark == "출발") { flight.remarkEnglish = "DEPARTED"; flight.tone = IcnDeparture::Muted; }
    else if (remark == "탑승중") { flight.remarkEnglish = "BOARDING"; flight.tone = IcnDeparture::Boarding; }
    else if (remark == "탑승준비") flight.remarkEnglish = "PREPARING";
    else if (remark == "탑승마감") { flight.remarkEnglish = "GATE CLOSED"; flight.tone = IcnDeparture::Muted; }
    else if (remark == "마감예정") { flight.remarkEnglish = "CLOSING SOON"; flight.tone = IcnDeparture::Attention; }
    else if (remark == "지연") { flight.remarkEnglish = "DELAYED"; flight.tone = IcnDeparture::Attention; }
    else if (remark == "결항") { flight.remarkEnglish = "CANCELLED"; flight.tone = IcnDeparture::Error; }
    else if (remark == "회항") { flight.remarkEnglish = "RETURNED"; flight.tone = IcnDeparture::Error; }
    else if (remark == "수속중") flight.remarkEnglish = "CHECK-IN";
}
}

bool IcnDepartures::load(const QString& path) {
    QFile file(path);
    if (file.open(QIODevice::ReadOnly) && file.size() <= 16 * 1024 * 1024) return loadData(file.readAll());
    flights_.clear(); date_ = QDate(); fetched_ = QDateTime(); recordCount_ = 0;
    error_ = "ICN snapshot could not be read";
    return false;
}

bool IcnDepartures::loadData(const QByteArray& data) {
    flights_.clear(); date_ = QDate(); fetched_ = QDateTime(); recordCount_ = 0;
    error_ = "Invalid or incomplete ICN snapshot";
    const auto document = QJsonDocument::fromJson(data);
    const auto root = document.object();
    const auto date = QDate::fromString(root["date"].toString(), Qt::ISODate);
    const auto fetched = QDateTime::fromString(root["fetched_at"].toString(), Qt::ISODate);
    const auto items = root["items"].toArray();
    if (document.isNull() || root["schema_version"].toInt() != 1 || root["airport"] != "ICN"
        || root["direction"] != "departures" || root["timezone"] != "Asia/Seoul"
        || !date.isValid() || !fetched.isValid() || !root["items"].isArray()
        || root["record_count"].toInt(-1) != items.size()) return false;
    static const Names names;
    static const QRegularExpression flightNumber("^([A-Z0-9]{2}|[A-Z]{3})\\s*[0-9]+[A-Z]?$");
    QSet<QString> identifiers;
    QList<IcnDeparture> flights;
    for (const auto& value : items) {
        const auto item = value.toObject(); const auto id = item["fid"].toString();
        const auto scheduled = timestamp(item["scheduleDatetime"].toString());
        if (id.isEmpty() || identifiers.contains(id) || !scheduled.isValid() || scheduled.date() != date) return false;
        identifiers.insert(id);
        if (item["codeshare"] != "Master" || item["passengerOrCargo"] != "Passenger") continue;
        IcnDeparture flight;
        flight.flightId = item["flightId"].toString().trimmed();
        flight.airportCode = item["airportCode"].toString().trimmed();
        if (flight.flightId.isEmpty() || flight.airportCode.isEmpty()) return false;
        const auto match = flightNumber.match(flight.flightId);
        flight.airlineCode = match.hasMatch() ? match.captured(1) : QString();
        flight.airlineName = item["airline"].toString();
        flight.scheduled = scheduled;
        const auto estimate = item["estimatedDatetime"].toString();
        flight.estimated = timestamp(estimate);
        if (!estimate.isEmpty() && !flight.estimated.isValid()) return false;
        flight.gate = item["gateNumber"].toString();
        const auto city = names.cities.value(flight.airportCode);
        const auto airport = names.airports.value(flight.airportCode);
        flight.destinationEnglish = city["cityEnglish"].toString();
        if (flight.destinationEnglish.isEmpty()) flight.destinationEnglish = airport["nameEnglish"].toString();
        if (flight.destinationEnglish.isEmpty()) flight.destinationEnglish = flight.airportCode;
        flight.destinationKorean = city["cityKorean"].toString();
        if (flight.destinationKorean.isEmpty()) flight.destinationKorean = airport["nameKorean"].toString();
        if (flight.destinationKorean.isEmpty()) flight.destinationKorean = item["airport"].toString();
        if (flight.destinationKorean.isEmpty()) flight.destinationKorean = flight.airportCode;
        describeRemark(flight, item["remark"].toString());
        flights.append(flight);
    }
    std::sort(flights.begin(), flights.end(), [](const auto& a, const auto& b) {
        return a.scheduled == b.scheduled ? a.flightId < b.flightId : a.scheduled < b.scheduled;
    });
    flights_ = flights; date_ = date; fetched_ = fetched; recordCount_ = items.size(); error_.clear();
    return true;
}

QList<IcnDeparture> IcnDepartures::visibleRows(const QDateTime& now, int limit) const {
    QList<IcnDeparture> result;
    if (!error_.isEmpty() || now.toTimeZone(seoul).date() != date_ || limit <= 0) return result;
    const auto cutoff = now.addSecs(-30 * 60);
    for (const auto& flight : flights_) {
        const auto effective = flight.estimated.isValid() ? flight.estimated : flight.scheduled;
        if (effective < cutoff) continue;
        result.append(flight);
        if (result.size() == limit) break;
    }
    return result;
}

QString IcnDepartures::notice(const QDateTime& now) const {
    if (!error_.isEmpty() || !date_.isValid()) return "ICN DATA UNAVAILABLE";
    if (now.toTimeZone(seoul).date() != date_)
        return "ICN DATA " + date_.toString(Qt::ISODate) + " / REFRESH REQUIRED";
    return "ICN SNAPSHOT " + fetched_.toTimeZone(seoul).toString("yyyy-MM-dd HH:mm") + " KST";
}
