#pragma once
#include "Departure.h"
#include <QList>
#include <QTimeZone>

struct AirportNames { QString english, korean, japanese; };
AirportNames airportNames(const QString& code);
void sortDepartures(QList<Departure>& flights);
QList<Departure> departureWindow(const QList<Departure>& flights, const QDate& date,
    const QTimeZone& timezone, const QDateTime& now, int limit);
