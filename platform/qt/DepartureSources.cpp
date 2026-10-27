#include "DepartureSources.h"
#include "AviationstackDepartures.h"
#include "IcnDepartures.h"
#include <QJsonArray>

DepartureSources::DepartureSources() {
    add("ICN",std::make_unique<IcnDepartures>());
    add("NRT",std::make_unique<AviationstackDepartures>());
}
void DepartureSources::add(const QString& airport,std::unique_ptr<DepartureProvider> provider) {
    sources_.insert_or_assign(airport,Entry{std::move(provider),false});
}
DepartureProvider* DepartureSources::source(const QString& airport) {
    auto found=sources_.find(airport);
    if (found==sources_.end()) return nullptr;
    auto& entry=found->second;
    if (!entry.loaded) {entry.provider->refresh();entry.loaded=true;}
    return entry.provider->configured()?entry.provider.get():nullptr;
}
const DepartureProvider* DepartureSources::current(const QString& airport) const {
    auto found=sources_.find(airport);
    return found!=sources_.end() && found->second.loaded && found->second.provider->configured()
        ? found->second.provider.get():nullptr;
}
DepartureProvider* DepartureSources::inspect(const QString& airport,const QString& path) {
    auto found=sources_.find(airport);
    if (found==sources_.end()) return nullptr;
    auto& entry=found->second;
    if (path.isEmpty()) entry.provider->refresh();else entry.provider->load(path);
    entry.loaded=true;
    return entry.provider.get();
}
QJsonArray DepartureSources::descriptions(const QDateTime& now) const {
    QJsonArray result;
    for (const auto& pair:sources_) {
        auto item=pair.second.provider->info().toJson(now);
        for (const auto* key:{"date","fetched_at","record_count","total_count","display_count",
                "offset","partial","configured","valid","state","error","sha256"}) item.remove(key);
        result.append(item);
    }
    return result;
}
