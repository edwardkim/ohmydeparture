import QtQuick
import Quickshell
import qs.Ui

BarWidget {
  id: root
  objectName: "ohmydepartureBarWidget"
  moduleName: "edwardkim.ohmydeparture"

  implicitWidth: button.implicitWidth
  implicitHeight: button.implicitHeight

  function launchCommand() {
    var path = decodeURIComponent(Qt.resolvedUrl("launch").toString().replace(/^file:\/\//, ""))
    var command = [path]
    if (setting("windowed", true) === true) command.push("--windowed")
    var view = setting("view", "codex") === "departures" ? "departures" : "codex"
    command.push("--view", view)
    var code = String(setting("airport", "")).trim().toUpperCase()
    var codes = ["ICN", "NRT", "HKG", "LHR", "CDG", "MXP", "JFK", "SFO",
                 "TOKYO", "SEOUL", "VICTORIA", "SYDNEY"]
    if (view === "departures" && codes.indexOf(code) >= 0) command.push("--airport", code)
    return command
  }

  function openBoard() {
    Quickshell.execDetached(launchCommand())
  }

  BarIconButton {
    id: button
    objectName: "ohmydepartureBarButton"
    anchors.fill: parent
    bar: root.bar
    text: "\uf072"
    tooltipText: "Oh My Departure — " + (root.setting("view", "codex") === "departures" ? "Departures" : "Codex sessions (read-only)")
                 + "\nClick to open or focus · F11: fullscreen · Esc: quit"
    onPressed: function(mouseButton) {
      if (mouseButton === Qt.LeftButton) root.openBoard()
    }
  }
}
