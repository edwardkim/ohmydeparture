#pragma once
#include <QDateTime>
#include <QList>
#include <QString>

struct IcnDeparture {
    enum Tone { Normal, Boarding, Attention, Muted, Error };
    QString flightId, airportCode, airlineCode, airlineName, gate;
    QString destinationEnglish, destinationKorean, remarkEnglish, remarkKorean;
    QDateTime scheduled, estimated;
    Tone tone = Normal;
};

// Reads a complete provider snapshot. This class never makes network requests.
class IcnDepartures {
public:
    bool load(const QString& path = QStringLiteral(":/data/icn-departures.json"));
    bool loadData(const QByteArray& data);
    QList<IcnDeparture> visibleRows(const QDateTime& now, int limit) const;
    QString notice(const QDateTime& now) const;
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
};
