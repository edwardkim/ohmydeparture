#pragma once
#include "Departure.h"
#include <QJsonObject>
#include <QList>
#include <QStringList>
#include <QTimeZone>

struct DepartureProviderInfo {
    QString provider, airport, storage, path, timeBasis, sha256;
    QTimeZone timezone;
    QStringList languages;
    QDate date;
    QDateTime fetched;
    int records = 0, total = 0, displayed = 0, offset = 0;
    bool configured = true, valid = false;
    QString error;
    QJsonObject toJson(const QDateTime& now) const;
};

// Rendering depends only on normalized rows and metadata. Adapters own the
// provider format, validation, time basis and coverage semantics; never HTTP.
class DepartureProvider {
public:
    virtual ~DepartureProvider() = default;
    virtual bool refresh() = 0;
    virtual bool load(const QString& path) = 0;
    virtual bool configured() const = 0;
    virtual QList<Departure> visibleRows(const QDateTime& now, int limit) const = 0;
    virtual QString notice(const QDateTime& now, bool empty = false) const = 0;
    virtual DepartureProviderInfo info() const = 0;
};
