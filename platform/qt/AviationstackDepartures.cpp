#include "AviationstackDepartures.h"
#include "DepartureSupport.h"
#include <QFile>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStandardPaths>
#include <QTimeZone>

namespace {
const QTimeZone tokyo("Asia/Tokyo");

QDateTime reportedTime(const QString& value) {
    const auto supplied = QDateTime::fromString(value, Qt::ISODate);
    // Preview the provider's clock fields verbatim, rather than silently adding
    // nine hours to its +00:00 strings. The board labels these REPORTED TIMES.
    return supplied.isValid() ? QDateTime(supplied.date(), supplied.time(), tokyo) : QDateTime();
}

void describeStatus(Departure& flight, const QString& status, int delay) {
    flight.remarkEnglish = "STATUS UNKNOWN"; flight.remarkKorean = "상태 미상"; flight.remarkJapanese = "不明";
    if (status == "scheduled") {
        flight.remarkEnglish = "SCHEDULED"; flight.remarkKorean = "예정"; flight.remarkJapanese = "予定";
        if (delay > 0) {
            flight.remarkEnglish = "DELAYED"; flight.remarkKorean = "지연"; flight.remarkJapanese = "遅延";
            flight.tone = Departure::Attention;
        }
    } else if (status == "active") {
        flight.remarkEnglish = "ACTIVE"; flight.remarkKorean = "운항중"; flight.remarkJapanese = "飛行中";
        flight.tone = Departure::Muted;
    } else if (status == "landed") {
        flight.remarkEnglish = "LANDED"; flight.remarkKorean = "도착"; flight.remarkJapanese = "到着";
        flight.tone = Departure::Muted;
    } else if (status == "cancelled") {
        flight.remarkEnglish = "CANCELLED"; flight.remarkKorean = "결항"; flight.remarkJapanese = "欠航";
        flight.tone = Departure::Error;
    } else if (status == "diverted" || status == "incident") {
        flight.remarkEnglish = status.toUpper();
        flight.remarkKorean = status == "diverted" ? "목적지 변경" : "운항 이상";
        flight.remarkJapanese = status == "diverted" ? "目的地変更" : "異常";
        flight.tone = Departure::Error;
    }
}
}

QString AviationstackDepartures::cachePath() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + "/ohmydeparture/aviationstack/nrt.json";
}

bool AviationstackDepartures::load(const QString& path) {
    QFile file(path);
    hasCache_ = file.exists();
    if (file.open(QIODevice::ReadOnly) && file.size() <= 4 * 1024 * 1024) return loadData(file.readAll());
    sha256_.clear(); flights_.clear(); date_ = QDate(); fetched_ = QDateTime(); recordCount_ = totalCount_ = offset_ = 0;
    error_ = "NRT cache could not be read";
    return false;
}

bool AviationstackDepartures::loadData(const QByteArray& data) {
    hasCache_ = true; sha256_.clear(); flights_.clear(); date_ = QDate(); fetched_ = QDateTime();
    recordCount_ = totalCount_ = offset_ = 0; error_ = "Invalid NRT cache";
    const auto document = QJsonDocument::fromJson(data);
    const auto root = document.object(), query = root["query"].toObject(), response = root["response"].toObject();
    const auto pagination = response["pagination"].toObject();
    const auto fetched = QDateTime::fromString(root["fetched_at"].toString(), Qt::ISODate);
    const auto items = response["data"].toArray();
    const int count = pagination["count"].toInt(-1), total = pagination["total"].toInt(-1);
    const int offset = pagination["offset"].toInt(-1), limit = pagination["limit"].toInt(-1);
    if (document.isNull() || root["schema_version"].toInt() != 1 || root["http_status"].toInt() != 200
        || root["provider"] != "aviationstack" || root["display_time_basis"] != "reported_wall_time"
        || root["endpoint"] != "https://api.aviationstack.com/v1/flights" || query["dep_iata"] != "NRT"
        || !fetched.isValid() || !response["data"].isArray() || response.contains("error")
        || count != items.size() || offset < 0 || offset != query["offset"].toInt(-1)
        || limit < 1 || limit > 100 || limit > query["limit"].toInt(-1)
        || count > limit || qint64(total) < qint64(offset) + count) return false;
    const auto date = fetched.toTimeZone(tokyo).date();
    QSet<QString> seen;
    QList<Departure> flights;
    for (const auto& value : items) {
        if (!value.isObject()) return false;
        const auto item = value.toObject(), departure = item["departure"].toObject();
        if (departure["iata"] != "NRT" || departure["timezone"] != "Asia/Tokyo") return false;
        const auto scheduled = reportedTime(departure["scheduled"].toString());
        const auto flightDate = QDate::fromString(item["flight_date"].toString(), Qt::ISODate);
        if (!scheduled.isValid() || !flightDate.isValid() || scheduled.date() != flightDate) return false;
        const auto identifier = item["flight"].toObject();
        if (!identifier["codeshared"].isNull() && !identifier["codeshared"].isUndefined()) continue;
        if (flightDate != date) continue;
        const auto arrival = item["arrival"].toObject(), airline = item["airline"].toObject();
        Departure flight;
        flight.flightId = identifier["iata"].toString().trimmed();
        if (flight.flightId.isEmpty()) flight.flightId = identifier["icao"].toString().trimmed();
        flight.airportCode = arrival["iata"].toString().trimmed();
        if (flight.airportCode.isEmpty()) flight.airportCode = arrival["icao"].toString().trimmed();
        if (flight.flightId.isEmpty() || flight.airportCode.isEmpty()) return false;
        const auto identity = flight.flightId + '|' + scheduled.toString(Qt::ISODate);
        if (seen.contains(identity)) continue;
        seen.insert(identity);
        flight.scheduled = scheduled;
        const auto estimate = departure["estimated"].toString();
        flight.estimated = reportedTime(estimate);
        if (!estimate.isEmpty() && !flight.estimated.isValid()) return false;
        flight.airlineCode = airline["iata"].toString();
        if (flight.airlineCode.isEmpty()) flight.airlineCode = airline["icao"].toString();
        flight.airlineName = airline["name"].toString();
        flight.gate = departure["gate"].toString();
        const auto names = airportNames(flight.airportCode);
        flight.destinationEnglish = names.english;
        if (flight.destinationEnglish.isEmpty()) flight.destinationEnglish = arrival["airport"].toString();
        if (flight.destinationEnglish.isEmpty()) flight.destinationEnglish = flight.airportCode;
        flight.destinationKorean = names.korean.isEmpty() ? flight.destinationEnglish : names.korean;
        flight.destinationJapanese = names.japanese.isEmpty() ? flight.destinationEnglish : names.japanese;
        describeStatus(flight, item["flight_status"].toString(), departure["delay"].toInt());
        flights.append(flight);
    }
    sortDepartures(flights);
    flights_ = flights; date_ = date; fetched_ = fetched; recordCount_ = count; totalCount_ = total; offset_ = offset;
    sha256_=QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex());
    error_.clear();
    return true;
}

QList<Departure> AviationstackDepartures::visibleRows(const QDateTime& now, int limit) const {
    return error_.isEmpty() ? departureWindow(flights_,date_,tokyo,now,limit) : QList<Departure>();
}

QString AviationstackDepartures::notice(const QDateTime& now, bool empty) const {
    if (!error_.isEmpty()) return "NRT AVIATIONSTACK DATA UNAVAILABLE";
    if (now.toTimeZone(tokyo).date() != date_)
        return "NRT CACHE " + date_.toString(Qt::ISODate) + " / REFRESH REQUIRED";
    if (empty) return "NRT CACHE / NO UPCOMING CACHED FLIGHTS";
    return QString("NRT AVIATIONSTACK %1/%2 %3 | %4 JST | REPORTED TIMES")
        .arg(recordCount_).arg(totalCount_).arg(offset_ == 0 && recordCount_ == totalCount_ ? "CACHED" : "PARTIAL")
        .arg(fetched_.toTimeZone(tokyo).toString("ddMMM HH:mm").toUpper());
}

DepartureProviderInfo AviationstackDepartures::info() const {
    DepartureProviderInfo result;
    result.provider="aviationstack";result.airport="NRT";result.storage="private_cache";
    result.path=cachePath();result.timeBasis="reported_wall_time";result.timezone=tokyo;
    result.languages={"en","ja","ko"};result.date=date_;result.fetched=fetched_;
    result.records=recordCount_;result.total=totalCount_;result.offset=offset_;result.displayed=flights_.size();
    result.configured=hasCache_;result.valid=error_.isEmpty() && date_.isValid();result.error=error_;
    result.sha256=sha256_;
    return result;
}
