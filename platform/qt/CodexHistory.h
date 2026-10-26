#pragma once
#include <QJsonArray>

// Bounded, newest-first previews of the normalized read-only history response.
QJsonArray codexHistoryRows(const QJsonArray& turns);
