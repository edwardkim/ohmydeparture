/* Linux entry point. ICN uses a provider snapshot; other travel boards share Haiku's generated data. */
#include "Board.h"
#include "Branding.h"
#include "Station.h"
#include "CodexBoard.h"
#include "DesktopBoards.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFontDatabase>
#include <QIcon>
#include <QScreen>
#include <QSettings>
#include <QThread>
#include <QEventLoop>
#include <QTimer>

#include <cstdio>
#include <limits>

int64 gTimeOffset = 0;

void save_airport_setting(const char* code)
{
    QSettings settings;
    settings.setValue("airport", QString::fromLatin1(code));
    settings.sync();
    if (settings.status() != QSettings::NoError)
        std::fprintf(stderr, "Could not save airport selection to %s\n", qPrintable(settings.fileName()));
}

#ifndef RDEPARTURES_NO_MAIN
static int airport_index(const QString& code)
{
    for (int i = 0; i < kAirportCount; ++i)
        if (code.compare(QLatin1String(kAirports[i].code), Qt::CaseInsensitive) == 0) return i;
    for (int i = 0; i < kStationCount; ++i)
        if (code.compare(QLatin1String(kStations[i].code), Qt::CaseInsensitive) == 0) return kAirportCount + i;
    return -1;
}

static int fail(const QString& message)
{
    std::fprintf(stderr, "%s\n", qPrintable(message));
    return 2;
}

int main(int argc, char** argv)
{
    // Snapshots/help work over SSH, even when a desktop session exports a
    // Wayland platform preference or a GTK theme that requires a display.
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "--snapshot" || arg.startsWith("--snapshot=")
            || arg == "--help" || arg == "-h" || arg == "--version" || arg == "-v") {
            qputenv("QT_QPA_PLATFORM", "offscreen");
            qputenv("QT_QPA_PLATFORMTHEME", "");
            qputenv("QT_IM_MODULE", "");
        }
    }
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("OhMyDeparture");
    QCoreApplication::setOrganizationName("edwardkim");
    QCoreApplication::setApplicationVersion(OHMYDEPARTURE_VERSION);
    QApplication::setApplicationDisplayName("Oh My Departure");
    QGuiApplication::setDesktopFileName("edwardkim.ohmydeparture");
    QApplication::setWindowIcon(QIcon(":/docs/icon.png"));
    if (QFontDatabase::addApplicationFont(":/fonts/D-DINCondensed-Bold.otf") < 0
        || QFontDatabase::addApplicationFont(":/fonts/texgyreheroscn-bold.otf") < 0)
        return fail("Could not load the bundled board font.");

    QCommandLineParser parser;
    parser.setApplicationDescription("Departure boards and read-only Codex session monitoring for Linux / Omarchy.\n"
        "ICN uses a dated provider snapshot; other travel timetables are generated.\n"
        "Departures: Esc/Q quits, arrows/Space changes board. Codex: Esc quits.\n"
        "F11: toggle full screen.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"view", "Board: departures or codex (read-only).", "name", "codex"});
    parser.addOption({"codex-fixture", "Show explicitly labelled sample Codex JSON; no connection is made.", "file"});
    parser.addOption({"airport", "ICN, NRT, HKG, LHR, CDG, MXP, JFK, SFO, TOKYO, SEOUL, VICTORIA or SYDNEY.", "code"});
    parser.addOption({"windowed", "Open in a resizable window instead of full screen."});
    parser.addOption({"size", "Window or snapshot dimensions (480x240 to 8192x8192).", "WIDTHxHEIGHT", "1600x900"});
    parser.addOption({"snapshot", "Render off-screen and save a PNG, then exit.", "file"});
    parser.addOption({"phase", "Pin the language/station-board phase (zero based).", "index"});
    parser.addOption({"wait", "Snapshot animation time in seconds, between 0 and 60.", "seconds", "4"});
    parser.addOption({"offset", "Shift the timetable clock by this many minutes.", "minutes", "0"});
    parser.process(app);
    if (!parser.positionalArguments().isEmpty()) return fail("Unexpected positional argument. See --help.");
    const auto view = !parser.isSet("view") && parser.isSet("airport") ? QString("departures") : parser.value("view");
    if (view != "departures" && view != "codex") return fail("Invalid --view. Use departures or codex.");
    if (parser.isSet("codex-fixture") && view != "codex") return fail("--codex-fixture requires --view codex.");

    const QStringList size = parser.value("size").toLower().split('x');
    bool widthOK = false, heightOK = false;
    const int width = size.value(0).toInt(&widthOK), height = size.value(1).toInt(&heightOK);
    if (size.size() != 2 || !widthOK || !heightOK || width < 480 || height < 240
        || width > 8192 || height > 8192 || qint64(width) * height > 33554432)
        return fail("Invalid --size. Use WIDTHxHEIGHT (minimum 480x240, at most 32 megapixels).");
    bool waitOK = false, offsetOK = false, phaseOK = true;
    const double wait = parser.value("wait").toDouble(&waitOK);
    const qint64 offset = parser.value("offset").toLongLong(&offsetOK);
    const int phase = parser.isSet("phase") ? parser.value("phase").toInt(&phaseOK) : -1;
    if (!waitOK || !std::isfinite(wait) || wait < 0 || wait > 60)
        return fail("Invalid --wait. Use a number between 0 and 60 seconds.");
    if (!offsetOK || offset < -52560000 || offset > 52560000)
        return fail("Invalid --offset. Use an integer number of minutes within 100 years.");
    if (!phaseOK || (parser.isSet("phase") && phase < 0)) return fail("Invalid --phase. Use a nonnegative integer.");
    gTimeOffset = offset * 60;

    if (view == "codex" && (width < 720 || height < 600)) return fail("Codex board requires --size at least 720x600.");
    if (view == "codex" && (parser.isSet("airport") || parser.isSet("phase") || parser.isSet("offset")))
        return fail("--airport, --phase and --offset apply to the departures view.");
    if (!parser.isSet("snapshot") && !parser.isSet("codex-fixture")) importLegacyPreferences();
    int airport = airport_index(parser.isSet("airport") ? parser.value("airport")
        : QSettings().value("airport", "ICN").toString());
    if (airport < 0 && parser.isSet("airport")) return fail("Unknown --airport. See --help for supported codes.");
    if (airport < 0) airport = 0;
    BApplication bridge;
    be_app = &bridge;
    if (!parser.isSet("snapshot") && !parser.isSet("codex-fixture")) {
        DesktopBoards desktop(airport,phase,QSize(width,height),parser.isSet("windowed"));
        QString error;
        const auto started=desktop.start(view,&error);
        if (started==DesktopBoards::Forwarded) return 0;
        if (started==DesktopBoards::Failed) return fail(error);
        desktop.showView(view);
        return app.exec();
    }

    if (view == "codex") {
        if (width < 720 || height < 600) return fail("Codex board requires --size at least 720x600.");
        if (parser.isSet("airport") || parser.isSet("phase") || parser.isSet("offset"))
            return fail("--airport, --phase and --offset apply to the departures view.");
        CodexBoard board(parser.value("codex-fixture"));
        if (parser.isSet("codex-fixture")) {
            QString error;
            if (!board.loadFixture(parser.value("codex-fixture"), &error)) return fail(error);
        }
        board.resize(width, height);
        if (parser.isSet("snapshot")) {
            if (parser.value("snapshot").isEmpty()) return fail("--snapshot requires an output filename.");
            board.show();
            QEventLoop loop;
            QTimer::singleShot(int(wait * 1000), &loop, &QEventLoop::quit);
            loop.exec();
            if (!board.grab().save(parser.value("snapshot"), "PNG"))
                return fail("Could not save snapshot: " + parser.value("snapshot"));
            std::printf("%s: %dx%d\n", qPrintable(parser.value("snapshot")), width, height);
            return 0;
        }
        if (parser.isSet("windowed")) board.show(); else board.showFullScreen();
        return app.exec();
    }

    BoardView board(BRect(0, 0, width - 1, height - 1), airport);
    board.setWindowTitle("Oh My Departure");
    board.setMinimumSize(480, 240);
    if (phase >= 0) board.SetFixedPhase(phase);

    if (parser.isSet("snapshot")) {
        if (parser.value("snapshot").isEmpty()) return fail("--snapshot requires an output filename.");
        board.LayoutBoard(board.Bounds());
        const bigtime_t end = system_time() + bigtime_t(wait * 1000000);
        do {
            board.Tick();
            if (system_time() >= end) break;
            QThread::msleep(16);
        } while (true);
        if (!board.Bitmap()->Image().save(parser.value("snapshot"), "PNG"))
            return fail("Could not save snapshot: " + parser.value("snapshot"));
        std::printf("%s: %dx%d\n", qPrintable(parser.value("snapshot")), width, height);
        return 0;
    }

    if (parser.isSet("windowed")) board.show(); else board.showFullScreen();
    return app.exec();
}
#endif
