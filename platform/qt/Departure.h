#pragma once
#include <QDateTime>
#include <QString>

// Provider-neutral row consumed by the split-flap departure board.
struct Departure {
    enum Tone { Normal, Boarding, Attention, Muted, Error };
    QString flightId, airportCode, airlineCode, airlineName, gate;
    QString destinationEnglish, destinationKorean, destinationJapanese;
    QString remarkEnglish, remarkKorean, remarkJapanese;
    QDateTime scheduled, estimated;
    Tone tone = Normal;
};
