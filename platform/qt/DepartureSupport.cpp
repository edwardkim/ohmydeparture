#include "DepartureSupport.h"
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <algorithm>

namespace {
QJsonArray items(const char* path) {
    QFile file(QString::fromLatin1(path));
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object()["items"].toArray() : QJsonArray();
}
struct Reference {
    QHash<QString,QJsonObject> cities, airports;
    Reference() {
        for (const auto& value : items(":/data/airport-codes.json")) {
            const auto item=value.toObject();cities.insert(item["cityCode"].toString(),item);
        }
        QSet<QString> ambiguous;
        for (const auto& value : items(":/data/airport-details.json")) {
            const auto item=value.toObject();const auto code=item["iataCode"].toString();
            if (code.isEmpty()) continue;
            if (airports.contains(code)) ambiguous.insert(code);
            airports.insert(code,item);
        }
        for (const auto& code : ambiguous) airports.remove(code);
    }
};
}

AirportNames airportNames(const QString& code) {
    static const Reference reference;
    const auto city=reference.cities.value(code), airport=reference.airports.value(code);
    AirportNames names{city["cityEnglish"].toString(),city["cityKorean"].toString(),city["cityJapan"].toString()};
    if (names.english.isEmpty()) names.english=airport["nameEnglish"].toString();
    if (names.korean.isEmpty()) names.korean=airport["nameKorean"].toString();
    return names;
}

void sortDepartures(QList<Departure>& flights) {
    std::sort(flights.begin(),flights.end(),[](const auto& a,const auto& b) {
        return a.scheduled==b.scheduled ? a.flightId<b.flightId : a.scheduled<b.scheduled;
    });
}

QList<Departure> departureWindow(const QList<Departure>& flights,const QDate& date,
    const QTimeZone& timezone,const QDateTime& now,int limit) {
    QList<Departure> rows;
    if (!now.isValid() || now.toTimeZone(timezone).date()!=date || limit<=0) return rows;
    const auto cutoff=now.addSecs(-30*60);
    for (const auto& flight : flights) {
        if ((flight.estimated.isValid()?flight.estimated:flight.scheduled)<cutoff) continue;
        rows.append(flight);
        if (rows.size()==limit) break;
    }
    return rows;
}
