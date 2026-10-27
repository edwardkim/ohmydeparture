#include "DepartureSources.h"
#include "AviationstackDepartures.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QTemporaryDir>
#include <cstdio>
#include <stdexcept>

static void check(bool condition,const char* message) {if (!condition) throw std::runtime_error(message);}
class TestProvider : public DepartureProvider {
public:
    int reads=0;
    bool refresh() override {++reads;return true;}
    bool load(const QString&) override {++reads;return true;}
    bool configured() const override {return true;}
    QList<Departure> visibleRows(const QDateTime&,int) const override {Departure row;row.flightId="TEST1";return {row};}
    QString notice(const QDateTime&,bool empty=false) const override {return empty?"EMPTY":"TEST DATA";}
    DepartureProviderInfo info() const override {
        DepartureProviderInfo result;result.airport="SFO";result.provider="test";result.valid=true;
        result.timezone=QTimeZone("America/Los_Angeles");result.languages={"en"};return result;
    }
};
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QTemporaryDir directory;qputenv("XDG_DATA_HOME",directory.path().toUtf8());
    try {
        DepartureSources sources;
        check(sources.descriptions(QDateTime::currentDateTime()).size()==2,"ICN and NRT adapters are discoverable");
        check(!sources.current("ICN"),"registry loads providers lazily");
        auto* icn=sources.source("ICN");
        check(icn && icn->info().valid && icn->info().sha256.size()==64,"registered ICN loads its real embedded snapshot and fingerprint");
        check(!sources.source("NRT"),"absent optional cache allows generated board");
        auto* nrt=sources.inspect("NRT");
        check(nrt && !nrt->info().configured && nrt->info().toJson(QDateTime::currentDateTime())["state"]=="missing","missing optional source is inspectable");
        check(!sources.source("XYZ"),"unregistered airport has no provider");
        auto provider=std::make_unique<TestProvider>();auto* test=provider.get();
        sources.add("SFO",std::move(provider));
        check(sources.source("SFO")==test && test->reads==1,"new adapter registers without a renderer branch");
        check(sources.source("SFO")==test && test->reads==1,"normal rendering reuses the loaded source");
        check(test->visibleRows(QDateTime::currentDateTime(),1)[0].flightId=="TEST1","new provider supplies the common display row");
        check(sources.inspect("SFO","candidate.json")==test && test->reads==2,"offline validation can explicitly reload a candidate");
        std::puts("PASS: provider registry, discovery, lazy loading, missing-cache policy and adapter extension");return 0;
    } catch (const std::exception& error) {std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;}
}
