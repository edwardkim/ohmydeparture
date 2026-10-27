#include "AviationstackDepartures.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <cstdio>
#include <stdexcept>

static void check(bool condition,const char* message) {if (!condition) throw std::runtime_error(message);}
static QDateTime at(const char* value) {return QDateTime::fromString(QString::fromLatin1(value),Qt::ISODate);}
static QJsonObject flight(const QString& id,const QString& time,const QString& destination="ICN") {
    return {{"flight_date","2026-10-27"},{"flight_status","scheduled"},
        {"departure",QJsonObject{{"iata","NRT"},{"timezone","Asia/Tokyo"},{"scheduled",time},
            {"estimated",QJsonValue::Null},{"gate",QJsonValue::Null},{"delay",0}}},
        {"arrival",QJsonObject{{"iata",destination},{"airport","Provider destination"}}},
        {"airline",QJsonObject{{"iata","ZZ"},{"name","Unknown airline"}}},
        {"flight",QJsonObject{{"iata",id},{"codeshared",QJsonValue::Null}}}};
}
static QJsonObject report(const QJsonArray& rows) {
    return {{"schema_version",1},{"provider","aviationstack"},{"http_status",200},
        {"display_time_basis","reported_wall_time"},
        {"endpoint","https://api.aviationstack.com/v1/flights"},{"fetched_at","2026-10-27T00:00:00Z"},
        {"query",QJsonObject{{"dep_iata","NRT"},{"limit",100},{"offset",0}}},
        {"response",QJsonObject{{"pagination",QJsonObject{{"count",rows.size()},{"total",1418},{"limit",100},{"offset",0}}},
                              {"data",rows}}}};
}
static bool load(AviationstackDepartures& model,const QJsonObject& root) {return model.loadData(QJsonDocument(root).toJson());}

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        auto changed=flight("ZZ101","2026-10-27T10:30:00+00:00");
        auto departure=changed["departure"].toObject();departure["estimated"]="2026-10-27T10:10:00+00:00";changed["departure"]=departure;
        auto shared=changed;auto number=shared["flight"].toObject();number["iata"]="XX1";
        number["codeshared"]=QJsonObject{{"flight_iata","ZZ101"}};shared["flight"]=number;
        auto cargo=flight("KZ4","2026-10-27T11:00:00+00:00");
        cargo["airline"]=QJsonObject{{"iata","KZ"},{"name","Nippon Cargo Airlines"}};
        const auto boundary=flight("ZZ102","2026-10-27T09:30:00+00:00");
        const auto old=flight("ZZ103","2026-10-27T09:29:00+00:00");
        auto unknown=flight("ZZ104","2026-10-27T12:00:00+00:00","QQQ");unknown["flight_status"]="cancelled";
        AviationstackDepartures model;
        check(load(model,report({changed,shared,cargo,boundary,old,unknown,changed})),"load partial local provider page");
        check(model.recordCount()==7 && model.totalCount()==1418 && model.displayCount()==5,"keep partial raw count; omit codeshares and duplicate rows; include cargo");
        auto rows=model.visibleRows(at("2026-10-27T10:00:00+09:00"),20);
        check(rows.size()==4 && rows[0].flightId=="ZZ102","include exactly thirty minutes ago; sort by reported departure time");
        check(rows[0].scheduled.toString("HH:mm")=="09:30" && rows[0].scheduled.offsetFromUtc()==9*3600,"preview reported wall time without a hidden nine-hour shift");
        check(rows[1].estimated.toString("HH:mm")=="10:10" && rows[1].gate.isEmpty(),"retain changed time and blank missing gate");
        check(rows[1].destinationKorean.contains("인천") && !rows[1].destinationJapanese.isEmpty(),"use static translated destination reference");
        check(rows[2].flightId=="KZ4","do not pretend the provider has a passenger-only flag");
        check(rows[3].destinationEnglish=="Provider destination" && rows[3].airlineCode=="ZZ"
              && rows[3].remarkEnglish=="CANCELLED" && rows[3].tone==Departure::Error,"keep unknown identity and real status");
        check(model.visibleRows(at("2026-10-27T10:00:01+09:00"),20).size()==3,"rolling cutoff advances without a download");
        check(model.visibleRows(at("2026-10-27T01:00:00Z"),1).size()==1,"filter uses the same instant independently of host timezone");
        const auto notice=model.notice(at("2026-10-27T10:00:00+09:00"));
        check(notice.contains("7/1418 PARTIAL") && notice.contains("09:00 JST") && notice.contains("REPORTED TIMES"),"show partial coverage, fetch time and preview time basis");
        check(model.visibleRows(at("2026-10-28T00:00:00+09:00"),20).isEmpty()
              && model.notice(at("2026-10-28T00:00:00+09:00")).contains("REFRESH REQUIRED"),"stale cache is explicit");
        auto malformed=report({changed});malformed.remove("display_time_basis");
        check(!load(model,malformed) && model.displayCount()==0,"reject ambiguous display-time basis and clear stale rows");
        auto wrong=changed;departure=wrong["departure"].toObject();departure["iata"]="SFO";wrong["departure"]=departure;
        check(!load(model,report({wrong})),"reject mismatched departure airport");
        malformed=report({changed});auto response=malformed["response"].toObject();auto page=response["pagination"].toObject();
        page["count"]=2;response["pagination"]=page;malformed["response"]=response;
        check(!load(model,malformed),"reject inconsistent cached record count");
        check(!model.load("/missing/nrt.json") && !model.hasCache(),"absent cache permits generated preview");
        QTemporaryDir directory;QFile invalid(directory.filePath("nrt.json"));check(invalid.open(QIODevice::WriteOnly),"temporary cache");
        invalid.write("invalid");invalid.close();
        check(!model.load(invalid.fileName()) && model.hasCache() && model.notice(QDateTime::currentDateTime()).contains("UNAVAILABLE"),"present invalid cache never silently selects fabricated data");
        std::puts("PASS: NRT local cache, partial coverage, reported times, codeshares, translations, rolling window and invalid/stale data");
        return 0;
    } catch (const std::exception& error) {std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;}
}
