#include "DepartureProvider.h"
#include <QJsonArray>

QJsonObject DepartureProviderInfo::toJson(const QDateTime& now) const {
    const bool stale = valid && date != now.toTimeZone(timezone).date();
    const QString state = !configured ? "missing" : !valid ? "invalid" : stale ? "stale" : "ready";
    return {{"provider",provider},{"airport",airport},{"storage",storage},{"path",path},
        {"timezone",QString::fromUtf8(timezone.id())},{"time_basis",timeBasis},
        {"languages",QJsonArray::fromStringList(languages)},
        {"date",date.toString(Qt::ISODate)},{"fetched_at",fetched.toString(Qt::ISODate)},
        {"record_count",records},{"total_count",total},{"display_count",displayed},{"offset",offset},
        {"partial",offset != 0 || records != total},{"configured",configured},{"valid",valid},
        {"state",state},{"error",error},{"sha256",sha256}};
}
