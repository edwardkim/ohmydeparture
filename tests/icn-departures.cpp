#include "IcnDepartures.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimeZone>
#include <cstdio>
#include <stdexcept>

static void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
static QDateTime at(const char* value) { return QDateTime::fromString(QString::fromLatin1(value), Qt::ISODate); }
static QJsonObject flight(const QString& id, const QString& code, const QString& time,
                          const QString& remark = QString()) {
    return {{"fid", id}, {"flightId", id}, {"airportCode", code}, {"airport", "원본 목적지"},
        {"airline", "Test airline"}, {"scheduleDatetime", time}, {"gateNumber", "123"},
        {"remark", remark}, {"codeshare", "Master"}, {"passengerOrCargo", "Passenger"}};
}
static QJsonObject snapshot(const QJsonArray& items) {
    return {{"schema_version", 1}, {"airport", "ICN"}, {"direction", "departures"},
        {"timezone", "Asia/Seoul"}, {"date", "2026-10-26"}, {"fetched_at", "2026-10-26T09:00:00+09:00"},
        {"record_count", items.size()}, {"items", items}};
}
static bool load(IcnDepartures& model, const QJsonObject& root) { return model.loadData(QJsonDocument(root).toJson()); }

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    try {
        auto early = flight("KE101", "NRT", "202610261000", "탑승중");
        early["estimatedDatetime"] = "202610260955";
        auto shared = early; shared["fid"] = "shared"; shared["flightId"] = "ZZ101"; shared["codeshare"] = "Slave";
        auto cargo = flight("XX100", "SFO", "202610261000"); cargo["passengerOrCargo"] = "Cargo";
        auto delayed = flight("ZZ901", "QQQ", "202610260930", "지연");
        auto midnight = flight("7C101", "DAD", "202610262355"); midnight["estimatedDatetime"] = "202610270005";
        IcnDepartures model;
        check(load(model, snapshot({early, shared, cargo, delayed, midnight})), "load complete provider response");
        check(model.recordCount() == 5 && model.operatingCount() == 3, "exclude codeshares and cargo without discarding raw count");
        auto rows = model.visibleRows(at("2026-10-26T09:50:00+09:00"), 20);
        check(rows.size() == 3 && rows[0].flightId == "ZZ901", "include delayed flights inside the thirty-minute window");
        check(rows[0].destinationEnglish == "QQQ" && rows[0].destinationKorean == "원본 목적지", "unknown destination keeps code/provider name");
        check(rows[0].airlineCode == "ZZ" && rows[0].remarkEnglish == "DELAYED", "unknown airline retains its own code");
        check(rows[1].destinationEnglish == "TOKYO/NARITA" && rows[1].destinationKorean == "도쿄/나리타", "resolve airport code using static city reference");
        check(rows[1].estimated < rows[1].scheduled && rows[1].estimated.toString("HH:mm") == "09:55", "preserve earlier changed time");
        check(rows[1].remarkEnglish == "BOARDING" && rows[1].gate == "123", "provider status and gate are used");
        check(rows[2].estimated.date() == QDate(2026, 10, 27) && rows[2].airlineCode == "7C", "changed time crosses midnight; alphanumeric airline code");
        check(rows[2].remarkEnglish == "SCHEDULED", "missing status is scheduled rather than confirmed on time");
        check(model.visibleRows(at("2026-10-26T00:50:00Z"), 20).size() == 3, "selection is independent of host timezone");
        check(model.visibleRows(at("2026-10-26T09:50:00+09:00"), 1).size() == 1, "row limit respected");
        check(model.notice(at("2026-10-26T10:00:00+09:00")).contains("09:00 KST"), "show actual fetch time");
        check(model.visibleRows(at("2026-10-27T00:00:00+09:00"), 20).isEmpty()
              && model.notice(at("2026-10-27T00:00:00+09:00")).contains("REFRESH REQUIRED"), "stale day is explicit and never fabricated");
        auto departed = flight("KE202", "NRT", "202610260800", "출발");
        auto cancelled = flight("KE303", "NRT", "202610261100", "결항");
        check(load(model, snapshot({departed, cancelled})), "departure/cancellation fixture");
        rows = model.visibleRows(at("2026-10-26T10:00:00+09:00"), 20);
        check(rows.size() == 1 && rows[0].remarkEnglish == "CANCELLED" && rows[0].tone == IcnDeparture::Error, "old departures removed; cancellations remain visible");
        const auto boundary = flight("KE401", "NRT", "202610260930");
        const auto beforeBoundary = flight("KE402", "NRT", "202610260929");
        const auto oldDelayed = flight("KE403", "NRT", "202610260900", "지연");
        auto changedIntoWindow = flight("KE404", "NRT", "202610260800", "지연");
        changedIntoWindow["estimatedDatetime"] = "202610261020";
        auto changedOutsideWindow = flight("KE405", "NRT", "202610261005");
        changedOutsideWindow["estimatedDatetime"] = "202610260929";
        const auto future = flight("KE406", "NRT", "202610262300");
        check(load(model, snapshot({boundary, beforeBoundary, oldDelayed, changedIntoWindow, changedOutsideWindow, future})),
              "thirty-minute boundary fixture");
        rows = model.visibleRows(at("2026-10-26T10:00:00+09:00"), 20);
        check(rows.size() == 3 && rows[0].flightId == "KE404" && rows[1].flightId == "KE401"
              && rows[2].flightId == "KE406", "include exactly thirty minutes ago and all future flights; no old-delay exception");
        rows = model.visibleRows(at("2026-10-26T10:00:01+09:00"), 20);
        check(rows.size() == 2 && rows[0].flightId == "KE404" && rows[1].flightId == "KE406",
              "advancing system time moves the window without downloading data");
        check(model.visibleRows(at("2026-10-26T01:00:00Z"), 20).size() == 3,
              "thirty-minute cutoff uses the same instant in another timezone");
        auto malformed = snapshot({early}); malformed["record_count"] = 2;
        check(!load(model, malformed) && model.operatingCount() == 0, "reject partial snapshot and clear old data");
        check(!load(model, snapshot({early, early})), "reject duplicate flight identities");
        early["estimatedDatetime"] = "202610262560";
        check(!load(model, snapshot({early})), "reject invalid changed timestamp");
        check(!model.load("/missing/icn-snapshot.json") && model.notice(QDateTime::currentDateTime()).contains("UNAVAILABLE"), "missing snapshot never selects generated data");
        check(model.load() && model.recordCount() >= model.operatingCount() && model.operatingCount() > 0, "bundled real snapshot is complete and usable offline");
        const auto today = QDateTime(model.date(), QTime(0,0), QTimeZone("Asia/Seoul"));
        check(!model.visibleRows(today, 20).isEmpty(), "bundled ICN day supplies actual departure rows");
        std::puts("PASS: ICN snapshot, rolling thirty-minute window, codeshares, static names, real statuses, earlier/midnight changes, stale dates, malformed data");
        return 0;
    } catch (const std::exception& error) { std::fprintf(stderr, "FAIL: %s\n", error.what()); return 1; }
}
