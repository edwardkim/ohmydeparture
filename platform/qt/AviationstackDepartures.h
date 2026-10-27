#pragma once
#include "Departure.h"
#include "DepartureProvider.h"
#include <QList>

// Local NRT preview only. No network requests or credentials in the application.
class AviationstackDepartures : public DepartureProvider {
public:
    static QString cachePath();
    bool load(const QString& path = cachePath()) override;
    bool refresh() override { return load(); }
    bool configured() const override { return hasCache_; }
    bool loadData(const QByteArray& data);
    bool hasCache() const { return hasCache_; }
    QList<Departure> visibleRows(const QDateTime& now, int limit) const override;
    QString notice(const QDateTime& now, bool empty = false) const override;
    DepartureProviderInfo info() const override;
    int recordCount() const { return recordCount_; }
    int totalCount() const { return totalCount_; }
    int displayCount() const { return flights_.size(); }
    QDate date() const { return date_; }
    QString error() const { return error_; }
private:
    bool hasCache_ = false;
    QList<Departure> flights_;
    int recordCount_ = 0, totalCount_ = 0, offset_ = 0;
    QDate date_;
    QDateTime fetched_;
    QString error_;
    QString sha256_;
};
