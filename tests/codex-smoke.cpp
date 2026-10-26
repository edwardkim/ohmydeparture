#include "CodexBoard.h"
#include "Branding.h"
#include "Board.h"
#include <QDialog>
#include <QPushButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcess>
#include <QSplitter>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSettings>
#include <QScreen>
#include <QCryptographicHash>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QtTest/QTest>
#include <cstdio>
#include <stdexcept>

static void check(bool condition,const char* message) {if (!condition) throw std::runtime_error(message);}
static bool await(const std::function<bool()>& condition) {
    for (int i=0;i<100;++i) {if (condition()) return true;QTest::qWait(30);}return condition();
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);qunsetenv("HYPRLAND_INSTANCE_SIGNATURE");
    app.setOrganizationName("edwardkim");app.setApplicationName("OhMyDeparture");
    QTemporaryDir preferences;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,preferences.path());
    QFontDatabase::addApplicationFont(":/fonts/texgyreheroscn-bold.otf");
    try {
        QSettings legacy("rainygirl","RDepartures");
        legacy.setValue("airport","SEOUL");legacy.setValue("codex/flipIntervalSeconds",7);
        legacy.setValue("codex/flipDurationMs",1100);legacy.setValue("unrelated/private","keep");legacy.sync();
        QSettings().setValue("codex/flipIntervalSeconds",4);
        importLegacyPreferences();
        check(QSettings().value("airport")=="SEOUL" && QSettings().value("codex/flipDurationMs")==1100,"legacy preferences imported");
        check(QSettings().value("codex/flipIntervalSeconds")==4 && !QSettings().contains("unrelated/private"),"existing values preserved; only supported preferences imported");
        legacy.setValue("airport","NRT");legacy.sync();importLegacyPreferences();
        check(QSettings().value("airport")=="SEOUL" && legacy.value("unrelated/private")=="keep","one-time migration leaves legacy file intact");
        QSettings().clear();legacy.clear();
        CodexBoard board("tests/codex-fixture.json");board.show();QTest::qWait(80);
        check(board.flapBoard()->CodexRowCount()==4,"main view uses original renderer with four root sessions");
        board.openDetails(0);
        auto* details=board.detailsWindow();
        check(details->isVisible(),"main row opens supplementary details");
        const QRect available=board.screen()->availableGeometry();
        board.move(available.right()-30,available.top()+100);board.openDetails(0);QTest::qWait(30);
        check(details->parentWidget()==nullptr,"details is independent of the parent window position");
        check(available.contains(details->frameGeometry()),"details is clamped to available monitor bounds");
        check(qAbs(details->frameGeometry().center().x()-available.center().x())<=2,"details is centred across monitor width");
        board.move(0,0);
        board.openSettings();auto* interval=board.settingsWindow()->findChild<QSpinBox*>("flipInterval");
        check(interval->value()==3 && board.flapBoard()->CodexFlipInterval()==3,"default periodic flip interval is three seconds");
        auto* duration=board.settingsWindow()->findChild<QDoubleSpinBox*>("flipDuration");
        check(qAbs(duration->value()-.65)<.001 && board.flapBoard()->CodexFlipDurationMs()==650,"default individual flip lasts 650 milliseconds");
        duration->setValue(.9);check(QSettings().value("codex/flipDurationMs").toInt()==900 && board.flapBoard()->CodexFlipDurationMs()==900,"flip duration applies and persists separately from repeat interval");
        duration->setValue(.65);
        QDir().mkpath("test-output");board.settingsWindow()->grab().save("test-output/codex-flip-settings.png");
        check(available.contains(board.settingsWindow()->frameGeometry()),"settings fits monitor bounds");
        board.settingsWindow()->hide();
        // Wayland placement must select only this PID + the fixed dialog title.
        const QByteArray originalPath=qgetenv("PATH");
        QFile hypr(preferences.path()+"/hyprctl");check(hypr.open(QIODevice::WriteOnly),"mock Hyprland client");
        hypr.write("#!/usr/bin/python3\nimport os,json,sys\n"
            "if sys.argv[1:]==['-j','clients']:\n"
            " print(json.dumps([{'pid':int(os.environ['PLACEMENT_TEST_PID'])+1,'title':'Codex session details','address':'0x999'},"
            "{'pid':int(os.environ['PLACEMENT_TEST_PID']),'title':'Other window','address':'0x888'},"
            "{'pid':int(os.environ['PLACEMENT_TEST_PID']),'title':'Codex session details','address':'0x123'}]))\n"
            "else:\n"
            " with open(os.environ['PLACEMENT_TEST_LOG'],'w') as f:f.write(sys.argv[-1])\n");hypr.close();
        hypr.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner);
        const QString placementLog=preferences.path()+"/placement.log";
        qputenv("PATH",preferences.path().toUtf8()+':'+originalPath);qputenv("HYPRLAND_INSTANCE_SIGNATURE","test");
        qputenv("PLACEMENT_TEST_PID",QByteArray::number(QCoreApplication::applicationPid()));qputenv("PLACEMENT_TEST_LOG",placementLog.toUtf8());
        board.openDetails(0);check(await([&]{return QFile::exists(placementLog);}),"Hyprland dialog placement dispatched");
        QFile trace(placementLog);check(trace.open(QIODevice::ReadOnly),"placement trace");const auto dispatch=trace.readAll();
        check(dispatch.contains("address:0x123") && !dispatch.contains("0x999") && !dispatch.contains("0x888"),"placement is scoped to own dialog");
        check(dispatch.contains("window.float")&&dispatch.contains("window.resize")&&dispatch.contains("window.center"),"Wayland explicitly floats, sizes and centres selected dialog");
        qunsetenv("HYPRLAND_INSTANCE_SIGNATURE");qputenv("PATH",originalPath);
        auto* list=details->findChild<QTreeWidget*>("sessions");auto* history=details->findChild<QTreeWidget*>("history");
        auto* search=details->findChild<QLineEdit*>("sessionSearch");auto* filter=details->findChild<QComboBox*>("stateFilter");
        auto* text=details->findChild<QPlainTextEdit*>("eventContent");
        check(list->topLevelItemCount()==4,"roots exclude child agent");
        auto* parent=list->topLevelItem(0);check(parent->childCount()==1,"agent is nested under actual parent");
        parent->setExpanded(true);check(board.flapBoard()->CodexRowCount()==5,"expanded agent appears on flap board");list->setCurrentItem(parent->child(0));
        check(history->topLevelItem(0)->child(0)->text(0).contains("Tool validation"),"child selection loads child history");
        history->setCurrentItem(history->topLevelItem(0)->child(0));check(text->toPlainText().contains("methods"),"event content is inspectable");
        search->setText("Validation agent");check(list->topLevelItemCount()==1 && list->topLevelItem(0)->childCount()==1,"search retains matching child and parent");
        search->clear();filter->setCurrentIndex(2);check(list->topLevelItemCount()==2,"attention filter excludes idle and stored sessions");
        filter->setCurrentIndex(3);check(list->topLevelItemCount()==1,"history filter");check(list->topLevelItem(0)->data(1,Qt::UserRole+1)=="history","historical session never labelled complete");
        list->setCurrentItem(list->topLevelItem(0));check(history->topLevelItem(0)->text(1)=="Interrupted","turn outcome distinct from runtime badge");
        filter->setCurrentIndex(0);list->setCurrentItem(list->topLevelItem(0));
        check(history->topLevelItemCount()==2,"selected session turns");
        history->setCurrentItem(history->topLevelItem(0)->child(1));check(text->toPlainText().contains("make test"),"tool details");
        for (const auto size:{QSize(1600,900),QSize(900,1100),QSize(720,600)}) {
            board.resize(size);QTest::qWait(30);check(board.size()==size,"responsive window keeps requested size");
            check((details->resize(size), QTest::qWait(20), details->findChild<QSplitter*>()->orientation()==(details->width()<1150?Qt::Vertical:Qt::Horizontal)),"narrow layout stacks details");
            check(text->mapTo(details,QPoint(text->width(),text->height())).y()<=details->height(),"event panel fits window");
            auto* heading=details->findChild<QLabel*>("heading");
            for (const auto* name:{"openDepartures","openSettings"}) {
                auto* button=details->findChild<QPushButton*>(name);
                check(details->rect().contains(QRect(button->mapTo(details,QPoint()),button->size())),"header actions fit narrow window");
                if (details->width()<1000)
                    check(button->mapTo(details,QPoint()).y()>heading->mapTo(details,QPoint(0,heading->height())).y(),"narrow header places actions below title");
            }
        }
        details->resize(1400,800);QTest::qWait(30);QDir().mkpath("test-output");details->grab().save("test-output/codex-details-demo.png");
        text->setFocus();QTest::keyClick(text,Qt::Key_Escape);check(!details->isVisible() && board.isVisible(),"Escape closes details and retains board");
        board.resize(1600,900);board.activateWindow();list->topLevelItem(0)->setExpanded(true);QTest::qWait(4800);board.grab().save("test-output/codex-board-demo.png");
        interval->setValue(0);QTest::qWait(350);
        auto image=board.flapBoard()->Bitmap()->Image();
        check(image.pixelColor(696,100)==image.pixelColor(696,138),"same project / child agent shares one colour badge");
        check(image.pixelColor(696,100)!=image.pixelColor(696,176),"different projects have distinct visible colours");
        check(image.pixelColor(696,100)!=image.pixelColor(696,290),"blank row clears its project badge");
        auto tile=[&]{return board.flapBoard()->Bitmap()->Image().copy(690,94,200,34);};
        const auto stable=tile();
        QTest::qWait(1200);check(tile()==stable,"disabled animation leaves project badge still");
        QDir().mkpath("test-output/codex-flip");
        board.flapBoard()->Bitmap()->Image().copy(0,0,1600,300).save("test-output/codex-flip/frame-000.png");
        interval->setValue(2);check(QSettings().value("codex/flipIntervalSeconds").toInt()==2,"interval setting persists");
        bool moved=false;for(int i=0;i<80;++i) {QTest::qWait(35);if(tile()!=stable){moved=true;break;}}
        check(moved,"unchanged project badge visibly flips at configured interval");
        for(int frame=1;frame<=32;++frame) {
            QTest::qWait(50);
            if(frame==7) check(tile()!=stable,"flip remains visibly folded after 350 milliseconds");
            if(frame==18) {check(tile()==stable,"slower flip restores original project colour and text");interval->setValue(0);}
            board.flapBoard()->Bitmap()->Image().copy(0,0,1600,300).save(QString("test-output/codex-flip/frame-%1.png").arg(frame,3,10,QChar('0')));
        }
        interval->setValue(3);

        QTest::keyClick(board.flapBoard(),Qt::Key_F11);check(board.isFullScreen(),"F11 on flap view");
        QTest::keyClick(board.flapBoard(),Qt::Key_F11);check(!board.isFullScreen(),"F11 returns to window");
        bool returned=false;board.setDeparturesHandler([&](int){returned=true;});board.openDetails();
        QTest::mouseClick(details->findChild<QPushButton*>("openDepartures"),Qt::LeftButton);check(returned,"details provides return to departures");
        board.activateWindow();QTest::qWait(20);QTest::keyClick(board.flapBoard(),Qt::Key_Escape);check(!board.isVisible(),"Escape closes main flap board");
        // Exercise the real asynchronous helper using an isolated, tiny Codex home.
        QTemporaryDir temp;check(temp.isValid(),"temporary home");
        qputenv("CODEX_HOME",temp.path().toUtf8());QDir(temp.path()).mkpath("sessions/2026");
        QFile log(temp.path()+"/sessions/2026/rollout-a.jsonl");check(log.open(QIODevice::WriteOnly),"local log fixture");
        log.write("{\"type\":\"session_meta\",\"payload\":{\"id\":\"local-root\",\"cwd\":\"/demo/test\",\"source\":\"cli\"}}\n{\"type\":\"event_msg\",\"payload\":{\"type\":\"task_complete\"}}\n");log.close();
        CodexBoard local;local.show();auto* localList=local.detailsWindow()->findChild<QTreeWidget*>("sessions");
        check(await([&]{return localList->topLevelItemCount()==1;}),"async reader returns local session");
        check(localList->topLevelItem(0)->data(1,Qt::UserRole+1)=="history","disconnected local state is history");
        check(await([&]{return local.detailsWindow()->findChild<QTreeWidget*>("history")->topLevelItemCount()==1;}),"async history request completes");
        // Archive response must not retain the old selection, events, or more button.
        local.detailsWindow()->findChild<QCheckBox*>("archives")->setChecked(true);
        check(await([&]{return localList->topLevelItemCount()==0;}),"archive mode excludes active rollouts");
        check(local.detailsWindow()->findChild<QPlainTextEdit*>("eventContent")->toPlainText().isEmpty(),"archive clears stale content");
        std::puts("PASS: session tree, child/search/filter, turn/tool details, responsive layout, keys, async local fallback, archives");
        return 0;
    } catch (const std::exception& e) {std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
