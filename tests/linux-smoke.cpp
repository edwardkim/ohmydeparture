#include "Board.h"
#include "Station.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include <cstdio>
#include <stdexcept>

static void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

static QByteArray digest(const BoardView& board)
{
    const QImage& image = board.Bitmap()->Image();
    return QCryptographicHash::hash(QByteArrayView(
        reinterpret_cast<const char*>(image.constBits()), image.sizeInBytes()), QCryptographicHash::Sha256);
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setOrganizationName("edwardkim");
    app.setApplicationName("OhMyDeparture");
    QTemporaryDir settingsDir;
    check(settingsDir.isValid(), "temporary settings directory");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
    check(QFontDatabase::addApplicationFont(":/fonts/texgyreheroscn-bold.otf") >= 0, "bundled font");
    BApplication bridge;
    be_app = &bridge;

    try {
        QFile airportCodes(":/data/airport-codes.json");
        check(airportCodes.open(QIODevice::ReadOnly), "airport code reference bundled for offline use");
        const auto reference = QJsonDocument::fromJson(airportCodes.readAll()).object();
        check(reference.value("schema_version").toInt() == 1
            && reference.value("record_count").toInt() > 0
            && reference.value("items").toArray().size() == reference.value("record_count").toInt(),
            "bundled airport code reference is complete JSON");
        QFile airportDetails(":/data/airport-details.json");
        check(airportDetails.open(QIODevice::ReadOnly), "airport details bundled for offline use");
        const auto details = QJsonDocument::fromJson(airportDetails.readAll()).object();
        check(details.value("schema_version").toInt() == 1
            && details.value("record_count").toInt() > 0
            && details.value("items").toArray().size() == details.value("record_count").toInt(),
            "bundled airport details are complete JSON");
        BoardView board(BRect(0, 0, 1599, 899), 0);
        board.show();
        QTest::qWait(100);
        check(board.Bitmap() != nullptr, "window initial rendering");
        check(board.Animating(), "initial split-flap animation starts");
        const QByteArray initial = digest(board);
        QTest::qWait(400);
        check(digest(board) != initial, "timer advances the rendered animation");

        QTest::keyClick(&board, Qt::Key_Right);
        check(board.AirportIndex() == 1, "right arrow selects Narita");
        check(QSettings().value("airport").toString() == "NRT", "airport selection is persisted");
        QTest::keyClick(&board, Qt::Key_Left);
        check(board.AirportIndex() == 0, "left arrow selects Incheon");
        QTest::keyClick(&board, Qt::Key_Left);
        check(board.AirportIndex() == kAirportCount + kStationCount - 1, "left arrow wraps to Sydney");
        QTest::keyClick(&board, Qt::Key_Space);
        check(board.AirportIndex() == 0, "space wraps to Incheon");

        for (const QSize size : {QSize(480, 240), QSize(1920, 1080), QSize(900, 1500), QSize(1600, 900)}) {
            board.resize(size);
            QTest::qWait(30);
            check(board.Bitmap()->Image().size() == size, "resize rebuilds bitmap to window dimensions");
        }
        for (int i = 0; i < kAirportCount + kStationCount; ++i) {
            board.SetAirport(i);
            QTest::qWait(20);
            check(board.AirportIndex() == i, "all airport and station selections");
        }

        board.SetAirport(0);
        bool menuOpened = false, selectionCorrect = false, selectionExclusive = false, quitIsCommand = false;
        QTimer::singleShot(20, [&] {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu) return;
            menuOpened = true;
            const auto actions = menu->actions();
            int checked = 0;
            for (const auto* action : actions) checked += action->isChecked();
            selectionCorrect = checked == 1 && actions.first()->isChecked();
            quitIsCommand = !actions.last()->isCheckable();
            auto* station = actions.at(kAirportCount + 1);
            station->setChecked(true);
            checked = 0;
            for (const auto* action : actions) checked += action->isChecked();
            selectionExclusive = checked == 1 && station->isChecked() && !actions.first()->isChecked();
            menu->setActiveAction(station);
            QTest::keyClick(menu, Qt::Key_Return);
        });
        QTimer::singleShot(1500, [] {
            if (QWidget* popup = QApplication::activePopupWidget()) popup->close();
        });
        QTest::mouseClick(&board, Qt::LeftButton, Qt::NoModifier, QPoint(1530, 30));
        check(menuOpened, "gear opens a native popup menu");
        check(selectionCorrect, "departure menu checks only the active airport");
        check(selectionExclusive, "airport and station selections are mutually exclusive");
        check(quitIsCommand, "quit has no selection checkbox");
        check(board.AirportIndex() == kAirportCount + 1, "popup selection opens Seoul station");

        bool codexSelected = false, stationOnlyChecked = false;
        board.SetCodexHandler([&] { codexSelected = true; });
        QTimer::singleShot(20, [&] {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu) return;
            int checked = 0;
            for (const auto* action : menu->actions()) checked += action->isChecked();
            stationOnlyChecked = checked == 1 && menu->actions().at(kAirportCount + 1)->isChecked();
            for (auto* action : menu->actions()) if (action->text().startsWith("Codex")) {
                menu->setActiveAction(action); QTest::keyClick(menu, Qt::Key_Return); break;
            }
        });
        QTest::mouseClick(&board, Qt::LeftButton, Qt::NoModifier, QPoint(1530, 30));
        check(codexSelected, "airport menu exposes Codex and invokes view switching");
        check(stationOnlyChecked, "station menu checks only the active station");

        QTest::keyClick(&board, Qt::Key_F11);
        check(board.isFullScreen(), "F11 enters fullscreen");
        QTest::keyClick(&board, Qt::Key_F11);
        check(!board.isFullScreen(), "F11 leaves fullscreen");

        board.setCursor(Qt::BlankCursor);
        QTest::mouseMove(&board, QPoint(100, 100));
        QTest::qWait(20);
        check(board.cursor().shape() != Qt::BlankCursor, "mouse movement restores the pointer");

        QTimer::singleShot(0, [&] { QTest::keyClick(&board, Qt::Key_Escape); });
        QTimer::singleShot(1000, [] { QCoreApplication::exit(3); });
        check(app.exec() == 0, "Escape quits the application");
        std::puts("PASS: offline airport codes, animation, keyboard, settings, resize, 12 boards, popup, fullscreen, cursor, quit");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
