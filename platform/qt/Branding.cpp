#include "Branding.h"
#include <QSettings>
#include <QStringList>

void importLegacyPreferences()
{
    QSettings current;
    if (current.value("migration/rdeparturesImported",false).toBool()) return;
    QSettings legacy("rainygirl","RDepartures");
    bool found=false;
    for (const auto& key:QStringList{"airport","codex/flipIntervalSeconds","codex/flipDurationMs"}) {
        if (!legacy.contains(key)) continue;
        found=true;
        if (!current.contains(key)) current.setValue(key,legacy.value(key));
    }
    if (found) {
        current.setValue("migration/rdeparturesImported",true);
        current.sync();
    }
}
