#pragma once
#include "DepartureProvider.h"
#include <map>
#include <memory>

// Add an adapter here once; BoardView has no airport/provider switch.
class DepartureSources {
public:
    DepartureSources();
    void add(const QString& airport,std::unique_ptr<DepartureProvider> provider);
    DepartureProvider* source(const QString& airport);
    const DepartureProvider* current(const QString& airport) const;
    DepartureProvider* inspect(const QString& airport,const QString& path = {});
    QJsonArray descriptions(const QDateTime& now) const;
private:
    struct Entry {std::unique_ptr<DepartureProvider> provider;bool loaded=false;};
    std::map<QString,Entry> sources_;
};
