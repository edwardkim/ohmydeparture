import QtQuick
import QtQuick.Window
import Quickshell

ShellRoot {
  id: preview

  property QtObject barFacade: QtObject {
    property bool vertical: false
    property int barSize: 40
    property color barForeground: "#eeeeee"
    property color urgent: "#e5b566"
    property string fontFamily: "JetBrainsMono Nerd Font"
    property bool foregroundAnimationEnabled: false
    function showTooltip(target, text) {}
    function hideTooltip(target) {}
    function registerClickTarget(target) {}
    function unregisterClickTarget(target) {}
  }

  Window {
    visible: true
    width: 240
    height: 48
    title: "Oh My Departure plugin preview"
    color: "#181c24"

    Loader {
      id: widget
      x: 16
      y: 4
      source: "file://" + Quickshell.env("RDEPARTURES_PLUGIN_TEST_PATH") + "/BarWidget.qml"
      onStatusChanged: {
        if (status === Loader.Error) Qt.exit(1)
      }
      onLoaded: {
        item.bar = preview.barFacade
        item.settings = {view: "departures", windowed: true, airport: " seoul "}
        var command = item.launchCommand()
        if (JSON.stringify(command.slice(1)) !== JSON.stringify(["--windowed", "--view", "departures", "--airport", "SEOUL"])) {
          console.error("FAIL: widget settings/arguments", JSON.stringify(command))
          Qt.exit(1)
          return
        }
        item.settings = {view: "departures", windowed: false, airport: "ICN; echo unexpected"}
        if (JSON.stringify(item.launchCommand().slice(1)) !== JSON.stringify(["--view", "departures"])) {
          console.error("FAIL: invalid airport setting")
          Qt.exit(1)
          return
        }
        item.settings = {windowed: true, airport: "SEOUL", view: "codex; echo unexpected"}
        if (JSON.stringify(item.launchCommand().slice(1)) !== JSON.stringify(["--windowed", "--view", "codex"])) {
          console.error("FAIL: default/whitelisted Codex view")
          Qt.exit(1)
          return
        }
        item.settings = {windowed: true, view: "codex"}
        console.log("PASS: native Omarchy widget loaded; settings and argv validated")
      }
    }
    Text {
      anchors.verticalCenter: parent.verticalCenter
      x: 66
      text: "Oh My Departure"
      color: "#eeeeee"
      font.pixelSize: 16
    }
  }
}
