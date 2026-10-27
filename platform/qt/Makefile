# Linux/Omarchy backend. No network downloads are performed by the build.
.DEFAULT_GOAL := all
CXX ?= g++
CXXFLAGS ?= -O2 -g
CPPFLAGS += -DRDEPARTURES_QT -DQT_NO_KEYWORDS -Isrc -Iplatform/qt $(shell pkg-config --cflags Qt6Widgets Qt6Network)
CXXFLAGS += -std=c++17 -fPIC -Wall -Wextra -Wno-unused-parameter -Wno-multichar
LDLIBS += $(shell pkg-config --libs Qt6Widgets Qt6Network)
RCC := $(shell pkg-config --variable=libexecdir Qt6Core)/rcc
VERSION := $(strip $(shell cat VERSION))
APP := ohmydeparture
CPPFLAGS += -DOHMYDEPARTURE_VERSION=\"$(VERSION)\"
PREFIX ?= $(HOME)/.local
BUILD ?= .build/linux

SOURCES := platform/qt/LinuxApp.cpp platform/qt/Branding.cpp platform/qt/HaikuCompat.cpp platform/qt/CodexBoard.cpp platform/qt/CodexHistory.cpp platform/qt/DesktopBoards.cpp platform/qt/IcnDepartures.cpp src/Board.cpp src/Data.cpp src/Station.cpp
OBJECTS := $(SOURCES:%.cpp=$(BUILD)/%.o) $(BUILD)/qrc_assets.o

all: $(APP) RDepartures

RDepartures: $(APP)
	ln -sf $(APP) $@

check-deps:
	@pkg-config --exists Qt6Widgets Qt6Network || { echo "Install g++, make, pkgconf and qt6-base (Omarchy: omarchy pkg add qt6-base)."; exit 1; }
	@test -x "$(RCC)" || { echo "Qt 6 resource compiler not found: $(RCC)"; exit 1; }

$(APP): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

$(BUILD)/%.o: %.cpp | check-deps
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/qrc_assets.cpp: platform/qt/assets.qrc fonts/D-DINCondensed-Bold.otf fonts/texgyreheroscn-bold.otf docs/icon.png data/airport-codes.json data/airport-details.json data/icn-departures.json | check-deps
	@mkdir -p $(dir $@)
	$(RCC) $< -o $@

$(BUILD)/qrc_assets.o: $(BUILD)/qrc_assets.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

install: $(APP)
	install -Dm755 $(APP) "$(DESTDIR)$(PREFIX)/bin/ohmydeparture"
	install -Dm644 docs/icon.png "$(DESTDIR)$(PREFIX)/share/icons/hicolor/128x128/apps/ohmydeparture.png"
	install -Dm644 LICENSE "$(DESTDIR)$(PREFIX)/share/licenses/ohmydeparture/LICENSE"
	install -Dm644 THIRD_PARTY_NOTICES.md "$(DESTDIR)$(PREFIX)/share/licenses/ohmydeparture/THIRD_PARTY_NOTICES.md"
	install -Dm644 fonts/GUST-FONT-LICENSE.txt "$(DESTDIR)$(PREFIX)/share/licenses/ohmydeparture/GUST-FONT-LICENSE.txt"
	install -Dm644 fonts/D-DIN-OFL-1.1.txt "$(DESTDIR)$(PREFIX)/share/licenses/ohmydeparture/D-DIN-OFL-1.1.txt"
	install -Dm644 fonts/D-DIN-FONTLOG.txt "$(DESTDIR)$(PREFIX)/share/licenses/ohmydeparture/D-DIN-FONTLOG.txt"
	install -Dm644 platform/codex/reader.py "$(DESTDIR)$(PREFIX)/share/ohmydeparture/codex-reader.py"
	@mkdir -p "$(DESTDIR)$(PREFIX)/share/applications"
	sed 's|@EXEC@|$(PREFIX)/bin/ohmydeparture|g' platform/qt/ohmydeparture.desktop.in > "$(DESTDIR)$(PREFIX)/share/applications/edwardkim.ohmydeparture.desktop"

clean:
	rm -rf $(BUILD)
	rm -f $(APP) RDepartures

$(BUILD)/platform/qt/LinuxApp.o: VERSION

$(BUILD)/LinuxAppTest.o: platform/qt/LinuxApp.cpp VERSION
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DRDEPARTURES_NO_MAIN -MMD -MP -c $< -o $@

$(BUILD)/linux-smoke.o: tests/linux-smoke.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(shell pkg-config --cflags Qt6Test) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/linux-smoke: $(BUILD)/linux-smoke.o $(BUILD)/LinuxAppTest.o $(filter-out $(BUILD)/platform/qt/LinuxApp.o,$(OBJECTS))
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(shell pkg-config --libs Qt6Test)

$(BUILD)/codex-smoke.o: tests/codex-smoke.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(shell pkg-config --cflags Qt6Test) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/codex-smoke: $(BUILD)/codex-smoke.o $(BUILD)/LinuxAppTest.o $(filter-out $(BUILD)/platform/qt/LinuxApp.o,$(OBJECTS))
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(shell pkg-config --libs Qt6Test)

$(BUILD)/codex-history-test.o: tests/codex-history.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/codex-history-test: $(BUILD)/codex-history-test.o $(BUILD)/platform/qt/CodexHistory.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/icn-departures-test.o: tests/icn-departures.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/icn-departures-test: $(BUILD)/icn-departures-test.o $(BUILD)/platform/qt/IcnDepartures.o $(BUILD)/qrc_assets.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test-icn: $(BUILD)/icn-departures-test
	python3 tests/icn-download.py
	$(BUILD)/icn-departures-test

$(BUILD)/../libexec/codex-reader.py: platform/codex/reader.py
	install -Dm644 $< $@

test-codex: $(APP) $(BUILD)/codex-smoke $(BUILD)/codex-history-test $(BUILD)/../libexec/codex-reader.py
	$(BUILD)/codex-history-test
	python3 tests/codex-reader.py
	env QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME= QT_IM_MODULE= $(BUILD)/codex-smoke

test: $(APP) $(BUILD)/linux-smoke test-codex test-icn
	python3 tests/airport-data.py
	env QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME= QT_IM_MODULE= $(BUILD)/linux-smoke
	python3 tests/linux-cli.py

package-omarchy: $(APP)
	python3 tools/package-omarchy.py

test-omarchy: package-omarchy
	python3 tests/omarchy-package.py

test-desktop: $(APP)
	xvfb-run -a -s '-screen 0 1920x1080x24' /usr/bin/python3 tests/desktop-views.py

-include $(OBJECTS:.o=.d)
-include $(BUILD)/linux-smoke.d $(BUILD)/LinuxAppTest.d $(BUILD)/codex-smoke.d
-include $(BUILD)/icn-departures-test.d
-include $(BUILD)/codex-history-test.d
.PHONY: all check-deps install clean test test-codex test-icn test-desktop package-omarchy test-omarchy
