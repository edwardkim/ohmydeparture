#pragma once
#include "Departure.h"
#include "DepartureProvider.h"
#include <QDateTime>
#include <QList>
#include <QString>

using IcnDeparture = Departure;

// Reads a complete provider snapshot. This class never makes network requests.
class IcnDepartures : public DepartureProvider {
public:
    bool load(const QString& path = QStringLiteral(":/data/icn-departures.json")) override;
    bool refresh() override { return load(); }
    bool configured() const override { return true; }
    bool loadData(const QByteArray& data);
    QList<IcnDeparture> visibleRows(const QDateTime& now, int limit) const override;
    QString notice(const QDateTime& now, bool empty = false) const override;
    DepartureProviderInfo info() const override;
    int operatingCount() const { return flights_.size(); }
    int recordCount() const { return recordCount_; }
    QDate date() const { return date_; }
    QString error() const { return error_; }
private:
    QList<IcnDeparture> flights_;
    QDate date_;
    QDateTime fetched_;
    int recordCount_ = 0;
    QString error_;
    QString sha256_;
};
