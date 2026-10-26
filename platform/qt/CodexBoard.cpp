#include "CodexBoard.h"
#include "Board.h"
#include "CodexHistory.h"
#include <QDialog>
#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHeaderView>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSignalBlocker>
#include <QShortcut>
#include <QScrollBar>
#include <QSplitter>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QScreen>
#include <QSettings>
#include <QDialogButtonBox>
#include <QPointer>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QStackedLayout>
#include <algorithm>
#include <cmath>

namespace {
constexpr int IdRole = Qt::UserRole, StateRole = Qt::UserRole + 1, BodyRole = Qt::UserRole + 2;
QString label(const QString& state) {
    static const QHash<QString, QString> labels = {{"running","RUNNING"},{"approval","APPROVAL"},
        {"input","INPUT"},{"idle","IDLE"},{"error","ERROR"},{"history","HISTORY"},
        {"unknown","UNKNOWN"},{"stale","OFFLINE"}};
    return labels.value(state,"UNKNOWN");
}
QString explanation(const QString& state) {
    static const QHash<QString,QString> values = {{"running","Running"},{"approval","Waiting for approval"},
        {"input","Waiting for input"},{"idle","Loaded · no current activity"},{"error","Runtime error"},
        {"history","Stored history · runtime unknown"},{"unknown","Unknown state"},
        {"stale","Disconnected · previous runtime state is stale"}};
    return values.value(state,values.value("unknown"));
}
QColor ink(const QString& state) {
    if (state=="approval" || state=="input") return QColor("#f1be5b");
    if (state=="running") return QColor("#a7d6b0");
    if (state=="error") return QColor("#f29385");
    return QColor("#a8aca7");
}
QString turnLabel(const QString& state) {
    static const QHash<QString,QString> values = {{"completed","Turn completed"},{"interrupted","Interrupted"},
        {"failed","Failed"},{"inProgress","In progress"},{"recorded","Recorded · outcome unknown"}};
    return values.value(state,state);
}
QString when(qint64 seconds) {
    if (seconds<=0) return "—";
    const qint64 age = std::max<qint64>(0,QDateTime::currentSecsSinceEpoch()-seconds);
    if (age<60) return "just now";
    if (age<3600) return QString::number(age/60)+"m ago";
    if (age<86400) return QString::number(age/3600)+"h ago";
    return QDateTime::fromSecsSinceEpoch(seconds).toString("MM.dd");
}
QString helperPath() {
    const QDir binary(QCoreApplication::applicationDirPath());
    const QStringList paths = {binary.filePath("../libexec/codex-reader.py"),
        binary.filePath("../share/ohmydeparture/codex-reader.py"), binary.filePath("platform/codex/reader.py")};
    for (const auto& path:paths) if (QFile::exists(path)) return QDir::cleanPath(path);
    return {};
}
}

class StatusDelegate : public QStyledItemDelegate {
public:
    explicit StatusDelegate(QObject* parent):QStyledItemDelegate(parent) {}
    void observe(const QJsonArray& rows) {
        const qint64 now=QDateTime::currentMSecsSinceEpoch();
        for (auto value:rows) {
            const auto row=value.toObject(); const auto id=row["id"].toString(), state=row["status"].toString();
            if (states.contains(id) && states[id]!=state) changed[id]=now;
            states[id]=state;
        }
    }
    void paint(QPainter* p,const QStyleOptionViewItem& option,const QModelIndex& index) const override {
        QStyledItemDelegate::paint(p,option,index); // text is empty, preserve selection background
        const QString state=index.data(StateRole).toString();
        const auto rect=option.rect.adjusted(9,10,-9,-10);
        p->save();p->setRenderHint(QPainter::Antialiasing);
        p->setPen(QColor("#343934"));p->setBrush(QColor("#222722"));p->drawRoundedRect(rect,3,3);
        p->setClipRect(rect);
        const auto elapsed=QDateTime::currentMSecsSinceEpoch()-changed.value(index.data(IdRole).toString(),0);
        if (elapsed>=0 && elapsed<240) {
            p->translate(rect.center());p->scale(1,std::abs(std::cos(elapsed/240.0*3.141592653589793)));
            p->translate(-rect.center());
        }
        p->setPen(ink(state));QFont font("TeX Gyre Heros Cn",12,QFont::Bold);font.setLetterSpacing(QFont::AbsoluteSpacing,1.1);
        p->setFont(font);p->drawText(rect,Qt::AlignCenter,label(state));
        p->setPen(QColor("#101410"));p->drawLine(rect.left(),rect.center().y(),rect.right(),rect.center().y());
        p->restore();
    }
private:
    QHash<QString,QString> states;
    QHash<QString,qint64> changed;
};

CodexBoard::CodexBoard(const QString& fixture,QWidget* parent):QWidget(parent),fixture_(fixture) {
    setWindowTitle("Oh My Departure — Codex");setMinimumSize(720,600);resize(1600,900);
    static BApplication fallbackBridge;
    if (!be_app) be_app = &fallbackBridge;
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0);
    flap_ = new BoardView(BRect(0,0,1599,899),0);
    flap_->setObjectName("codexFlapBoard");
    historyFlap_=new BoardView(BRect(0,0,1599,899),0);
    historyFlap_->setObjectName("codexHistoryBoard");
    historyFlap_->SetCodexHistory({}, {}, "PARTIAL HISTORY");
    pages_=new QStackedLayout;
    mainLayout->addLayout(pages_);pages_->addWidget(flap_);pages_->addWidget(historyFlap_);
    pages_->setCurrentWidget(flap_);
    flap_->SetCodexRows({},"Connecting · read only");
    flap_->SetRowHandler([this](int row){openSessionHistory(row);});
    flap_->SetCodexHandler([this]{showSessionBoard();});
    flap_->SetBrowserHandler([this]{openDetails();});
    flap_->SetSettingsHandler([this]{openSettings();});
    flap_->SetAirportHandler([this](int index){details_->hide();if (departuresHandler_) departuresHandler_(index);});
    historyFlap_->SetBackHandler([this]{showSessionBoard();});
    historyFlap_->SetCodexHandler([this]{showSessionBoard();});
    historyFlap_->SetBrowserHandler([this]{openDetails();});
    historyFlap_->SetSettingsHandler([this]{openSettings();});
    historyFlap_->SetAirportHandler([this](int index){details_->hide();if (departuresHandler_) departuresHandler_(index);});
    // An independent window avoids Wayland's parent-centred dialog placement.
    // We own and delete it explicitly; geometry uses the monitor work area.
    details_ = new QDialog;
    details_->setObjectName("codexDetails");
    details_->setWindowTitle("Codex session details");
    details_->resize(1400,800);
    details_->setStyleSheet(R"(
        QWidget { background:#080a10; color:#ecece4; font-family:"Noto Sans CJK KR"; font-size:13px; }
        QLabel#eyebrow { color:#ffcc32; font-size:12px; letter-spacing:2px; }
        QLabel#heading { font-family:"TeX Gyre Heros Cn"; font-size:40px; font-weight:bold; }
        QLabel#summary { color:#c8cebf; font-size:14px; }
        QLabel#connection, QLabel#historyNote { color:#a0ab9c; font-size:12px; }
        QLabel#selection { font-size:17px; font-weight:bold; }
        QPushButton,QComboBox,QLineEdit { background:#212124; border:1px solid #323236; border-radius:4px; padding:7px 10px; }
        QPushButton:hover { background:#303b28; border-color:#b49d67; }
        QPushButton:disabled { color:#65705f; }
        QLineEdit:focus { border-color:#dfb65e; }
        QTreeWidget { border:1px solid #323236; background:#080a10; outline:0; }
        QTreeWidget::item { padding:5px 6px; border-bottom:1px solid #293124; }
        QTreeWidget::item:selected { background:#303039; }
        QTreeWidget::item:hover { background:#252f20; }
        QHeaderView::section { background:#000000; color:#b4bea8; border:0; padding:10px; }
        QPlainTextEdit { background:#080a10; border:1px solid #323236; padding:9px; selection-background-color:#5a6841; }
        QSplitter::handle { background:#263020; }
        QScrollBar:vertical { background:#171f15; width:10px; }
        QScrollBar::handle:vertical { background:#48553d; min-height:24px; }
        QToolTip { background:#283120; color:#f3eddd; border:1px solid #6b7756; }
    )");
    auto* layout=new QVBoxLayout(details_);layout->setContentsMargins(28,22,28,18);layout->setSpacing(13);
    detailsHeader_=new QBoxLayout(QBoxLayout::LeftToRight);
    auto* headings=new QVBoxLayout;auto* eyebrow=new QLabel("OH MY DEPARTURE   /   SESSION DETAILS");eyebrow->setObjectName("eyebrow");
    headings->addWidget(eyebrow);auto* heading=new QLabel("CODEX / DETAILS");heading->setObjectName("heading");headings->addWidget(heading);
    detailsHeader_->addLayout(headings);
    auto* top=new QHBoxLayout;
    top->addStretch();auto* readonly=new QLabel("Read only");readonly->setStyleSheet("color:#efc171; padding:8px; border:1px solid #74643d; border-radius:3px;");
    readonly->setMaximumHeight(34);top->addWidget(readonly);
    auto* departures = new QPushButton("Airports / railways");
    departures->setObjectName("openDepartures");
    connect(departures,&QPushButton::clicked,this,[this]{details_->hide();if (departuresHandler_) departuresHandler_(-1);});
    top->addWidget(departures);
    auto* preferences=new QPushButton("Display settings");preferences->setObjectName("openSettings");
    connect(preferences,&QPushButton::clicked,this,[this]{openSettings();});top->addWidget(preferences);
    clock_=new QLabel;top->addWidget(clock_);detailsHeader_->addLayout(top,1);layout->addLayout(detailsHeader_);
    connection_=new QLabel("Connecting…");connection_->setTextFormat(Qt::PlainText);connection_->setObjectName("connection");connection_->setWordWrap(true);layout->addWidget(connection_);
    auto* controls=new QHBoxLayout;
    search_=new QLineEdit;search_->setObjectName("sessionSearch");search_->setPlaceholderText("Search sessions, projects or agents");search_->setClearButtonEnabled(true);
    controls->addWidget(search_,1);filter_=new QComboBox;filter_->setObjectName("stateFilter");
    filter_->addItem("All sessions", "all");filter_->addItem("Active / waiting", "live");filter_->addItem("Needs attention", "attention");filter_->addItem("History", "history");controls->addWidget(filter_);
    archives_=new QCheckBox("Archived");archives_->setObjectName("archives");controls->addWidget(archives_);
    auto* refreshButton=new QPushButton("Refresh");refreshButton->setObjectName("refresh");controls->addWidget(refreshButton);layout->addLayout(controls);
    summary_=new QLabel;summary_->setObjectName("summary");summary_->setWordWrap(true);layout->addWidget(summary_);
    splitter_=new QSplitter;splitter_->setChildrenCollapsible(false);layout->addWidget(splitter_,1);
    auto* sessionsPane=new QWidget;auto* sl=new QVBoxLayout(sessionsPane);sl->setContentsMargins(0,0,0,0);sl->setSpacing(8);
    list_=new QTreeWidget;list_->setObjectName("sessions");list_->setColumnCount(4);list_->setHeaderLabels({"Session / subagent","Current state","Project","Updated"});
    list_->setUniformRowHeights(true);list_->setIndentation(19);list_->setSelectionMode(QAbstractItemView::SingleSelection);list_->setTextElideMode(Qt::ElideRight);
    list_->header()->setSectionResizeMode(0,QHeaderView::Stretch);list_->setColumnWidth(1,135);list_->setColumnWidth(2,150);list_->setColumnWidth(3,80);
    list_->header()->setStretchLastSection(false);statusDelegate_=new StatusDelegate(list_);list_->setItemDelegateForColumn(1,statusDelegate_);sl->addWidget(list_,1);
    moreSessions_=new QPushButton("Load older sessions");moreSessions_->setObjectName("moreSessions");moreSessions_->hide();sl->addWidget(moreSessions_);splitter_->addWidget(sessionsPane);
    auto* detailPane=new QWidget;auto* dl=new QVBoxLayout(detailPane);dl->setContentsMargins(8,0,0,0);dl->setSpacing(8);
    selection_=new QLabel("Select a session");selection_->setTextFormat(Qt::PlainText);selection_->setObjectName("selection");selection_->setWordWrap(true);dl->addWidget(selection_);
    historyNote_=new QLabel("Select a session to inspect its turns and tool history.");historyNote_->setTextFormat(Qt::PlainText);historyNote_->setObjectName("historyNote");historyNote_->setWordWrap(true);dl->addWidget(historyNote_);
    history_=new QTreeWidget;history_->setObjectName("history");history_->setHeaderLabels({"Turns / tool history","Result"});history_->header()->setSectionResizeMode(0,QHeaderView::Stretch);
    history_->setColumnWidth(1,105);history_->header()->setStretchLastSection(false);history_->setTextElideMode(Qt::ElideRight);dl->addWidget(history_,3);
    moreHistory_=new QPushButton("Load older turns");moreHistory_->setObjectName("moreHistory");moreHistory_->hide();dl->addWidget(moreHistory_);
    content_=new QPlainTextEdit;content_->setObjectName("eventContent");content_->setReadOnly(true);content_->setPlaceholderText("Select an item to inspect its stored contents.");dl->addWidget(content_,2);
    splitter_->addWidget(detailPane);splitter_->setStretchFactor(0,3);splitter_->setStretchFactor(1,2);splitter_->setSizes({900,560});
    auto* foot=new QLabel("HISTORY = runtime unknown   ·   IDLE = loaded / no current activity   ·   F11 fullscreen   ·   Esc close");foot->setWordWrap(true);foot->setStyleSheet("color:#87957d; font-size:11px;");layout->addWidget(foot);
    connect(search_,&QLineEdit::textChanged,this,[this]{buildSessions();});connect(filter_,&QComboBox::currentIndexChanged,this,[this]{buildSessions();});
    connect(archives_,&QCheckBox::toggled,this,[this]{++generation_;selectedId_.clear();sessions_={};turns_={};sessionCursor_.clear();historyCursor_.clear();content_->clear();selection_->setText("Select a session");buildSessions();buildHistory();refresh();});
    connect(refreshButton,&QPushButton::clicked,this,[this]{olderHistory_=false;refresh();});
    connect(moreSessions_,&QPushButton::clicked,this,[this]{olderSessions_=true;request("sessions",{{"cursor",sessionCursor_},{"source",source_},{"archived",archives_->isChecked()}},true);});
    connect(moreHistory_,&QPushButton::clicked,this,[this]{olderHistory_=true;loadHistory(true);});
    connect(list_,&QTreeWidget::itemExpanded,this,[this]{syncBoard();});
    connect(list_,&QTreeWidget::itemCollapsed,this,[this]{syncBoard();});
    connect(list_,&QTreeWidget::itemSelectionChanged,this,[this]{selected();});
    connect(list_,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem* item,int){
        const int row=visibleIds_.indexOf(item->data(0,IdRole).toString());if (row>=0) openSessionHistory(row);
    });
    connect(history_,&QTreeWidget::itemSelectionChanged,this,[this]{
        if (auto* item=history_->currentItem()) { const auto body=item->data(0,BodyRole).toString();if (content_->toPlainText()!=body) content_->setPlainText(body); }
    });
    reader_=new QProcess(this);reader_->setProcessChannelMode(QProcess::SeparateChannels);
    connect(reader_,&QProcess::readyReadStandardOutput,this,[this]{receive();});
    connect(reader_,&QProcess::readyReadStandardError,this,[this]{reader_->readAllStandardError();});
    connect(reader_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError){disconnected("Cannot start the reader. Check Python 3 and the installed files.");});
    connect(reader_,&QProcess::finished,this,[this](int,QProcess::ExitStatus){disconnected("Reader exited. Refresh to reconnect.");});
    deadline_=new QTimer(this);deadline_->setSingleShot(true);deadline_->setInterval(14000);
    connect(deadline_,&QTimer::timeout,this,[this]{reader_->kill();disconnected("Response timed out · retrying on the next refresh.");});
    poll_=new QTimer(this);poll_->setInterval(5000);connect(poll_,&QTimer::timeout,this,[this]{refresh(false);});
    auto* escape=new QShortcut(QKeySequence(Qt::Key_Escape),this);connect(escape,&QShortcut::activated,this,[this]{if (historyVisible()) showSessionBoard();else close();});
    auto* fullscreen=new QShortcut(QKeySequence(Qt::Key_F11),this);connect(fullscreen,&QShortcut::activated,this,[this]{isFullScreen()?showNormal():showFullScreen();});
    auto* ticker=new QTimer(this);ticker->setInterval(80);connect(ticker,&QTimer::timeout,this,[this]{clock_->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));list_->viewport()->update();});ticker->start();
    settings_=new QDialog;
    settings_->setWindowTitle("Oh My Departure settings");settings_->setObjectName("codexSettings");
    settings_->setStyleSheet(details_->styleSheet());settings_->resize(440,300);
    auto* preferencesLayout=new QVBoxLayout(settings_);preferencesLayout->setContentsMargins(22,20,22,20);
    preferencesLayout->addWidget(new QLabel("Repeat interval"));
    flipInterval_=new QSpinBox;flipInterval_->setObjectName("flipInterval");
    flipInterval_->setRange(0,3600);flipInterval_->setSuffix(" seconds");flipInterval_->setSpecialValueText("Disabled");
    bool intervalOK=false;
    int savedInterval=QSettings().value("codex/flipIntervalSeconds",3).toInt(&intervalOK);
    flipInterval_->setValue(intervalOK?qBound(0,savedInterval,3600):3);
    preferencesLayout->addWidget(flipInterval_);
    preferencesLayout->addWidget(new QLabel("Flip duration"));
    flipDuration_=new QDoubleSpinBox;flipDuration_->setObjectName("flipDuration");
    flipDuration_->setRange(.2,2.0);flipDuration_->setDecimals(2);flipDuration_->setSingleStep(.05);flipDuration_->setSuffix(" seconds");
    bool durationOK=false;
    int savedDuration=QSettings().value("codex/flipDurationMs",650).toInt(&durationOK);
    flipDuration_->setValue((durationOK?qBound(200,savedDuration,2000):650)/1000.0);
    flipDuration_->setToolTip("Higher values make each flip slower.");preferencesLayout->addWidget(flipDuration_);
    auto* help=new QLabel("Defaults: every 3 seconds · 0.65 seconds per flip\nSet the interval to 0 to disable periodic replay.\nContents and project colours stay unchanged.");help->setWordWrap(true);preferencesLayout->addWidget(help);
    auto* closeSettings=new QDialogButtonBox(QDialogButtonBox::Close);
    connect(closeSettings,&QDialogButtonBox::rejected,settings_,&QDialog::reject);preferencesLayout->addWidget(closeSettings);
    flap_->SetCodexFlipInterval(flipInterval_->value());
    flap_->SetCodexFlipDurationMs(qRound(flipDuration_->value()*1000));
    historyFlap_->SetCodexFlipInterval(flipInterval_->value());
    historyFlap_->SetCodexFlipDurationMs(qRound(flipDuration_->value()*1000));
    connect(flipInterval_,&QSpinBox::valueChanged,this,[this](int seconds){
        flap_->SetCodexFlipInterval(seconds);
        historyFlap_->SetCodexFlipInterval(seconds);
        QSettings settings;settings.setValue("codex/flipIntervalSeconds",seconds);settings.sync();
    });
    connect(flipDuration_,&QDoubleSpinBox::valueChanged,this,[this](double seconds){
        const int milliseconds=qRound(seconds*1000);flap_->SetCodexFlipDurationMs(milliseconds);
        historyFlap_->SetCodexFlipDurationMs(milliseconds);
        QSettings settings;settings.setValue("codex/flipDurationMs",milliseconds);settings.sync();
    });
    if (!fixture_.isEmpty()) {archives_->setEnabled(false);QString error; if (!loadFixture(fixture_,&error)) connection_->setText(error);}
    else {QTimer::singleShot(0,this,[this]{refresh();});poll_->start();}
    details_->installEventFilter(this);
}

CodexBoard::~CodexBoard() {
    for (auto* process:findChildren<QProcess*>("dialogPlacement",Qt::FindDirectChildrenOnly)) {
        process->disconnect(this);
        if (process->state()!=QProcess::NotRunning) {process->kill();process->waitForFinished(100);}
    }
    reader_->disconnect(this);
    if (reader_->state()!=QProcess::NotRunning) {
        reader_->closeWriteChannel();if (!reader_->waitForFinished(700)) {reader_->kill();reader_->waitForFinished(300);}
    }
    delete settings_;
    delete details_;
}

bool CodexBoard::loadFixture(const QString& path,QString* error) {
    QFile file(path);if (!file.open(QIODevice::ReadOnly)) {*error="Cannot open demo fixture: "+path;return false;}
    QJsonParseError parse;const auto doc=QJsonDocument::fromJson(file.readAll(),&parse);
    if (parse.error!=QJsonParseError::NoError || !doc.isObject() || !doc.object()["sessions"].isArray()) {*error="Invalid demo JSON";return false;}
    fixtureHistories_=doc.object()["histories"].toObject();auto result=doc.object();result["source"]="demo";
    result["notice"]="DEMO DATA · not connected to real sessions";updateSessions(result,false);return true;
}

void CodexBoard::startReader() {
    if (reader_->state()!=QProcess::NotRunning) return;
    const auto path=helperPath();
    if (path.isEmpty()) {disconnected("codex-reader.py is missing. Rebuild the installation bundle.");return;}
    // The helper only uses the standard library. Avoid project Python shims
    // (mise/pyenv) whose trust/config state can differ in a desktop launcher.
    reader_->setProgram(QFile::exists("/usr/bin/python3")?"/usr/bin/python3":"python3");
    reader_->setArguments({"-u",path});reader_->start();
}

void CodexBoard::request(const QString& method,QJsonObject params,bool append) {
    if (!fixture_.isEmpty()) return;
    if ((method=="sessions" && sessionBusy_) || (method=="history" && historyBusy_)) return;
    startReader();if (reader_->state()==QProcess::NotRunning) return;
    if (method=="sessions") sessionBusy_=true;else historyBusy_=true;
    const int id=++seq_;pending_.insert(id,{method,params["threadId"].toString(),append,generation_});
    reader_->write(QJsonDocument(QJsonObject{{"id",id},{"method",method},{"params",params}}).toJson(QJsonDocument::Compact)+'\n');
    if (!deadline_->isActive()) deadline_->start();
}

void CodexBoard::receive() {
    buffer_+=reader_->readAllStandardOutput();
    if (buffer_.size()>40*1024*1024) {reader_->kill();disconnected("Response too large");return;}
    while (buffer_.contains('\n')) {
        const auto end=buffer_.indexOf('\n');const auto line=buffer_.left(end);buffer_.remove(0,end+1);
        QJsonParseError error;const auto doc=QJsonDocument::fromJson(line,&error);if (error.error!=QJsonParseError::NoError) continue;
        const auto response=doc.object();const int id=response["id"].toInt();if (!pending_.contains(id)) continue;
        const auto pending=pending_.take(id);
        if (pending.method=="sessions") sessionBusy_=false;else historyBusy_=false;
        if (pending.generation!=generation_ || (pending.method=="history" && pending.thread!=selectedId_)) {
            if (pending.method=="sessions") refresh(); else loadHistory();
            continue;
        }
        if (response.contains("error")) {
            if (pending.method=="sessions") disconnected(response["error"].toString());
            else {historyNote_->setText("Cannot read history: "+response["error"].toString());turns_={};historyCursor_.clear();content_->clear();buildHistory();}
            continue;
        }
        const auto result=response["result"].toObject();
        if (pending.method=="sessions") updateSessions(result,pending.append);else updateHistory(result,pending.append);
    }
    if (pending_.isEmpty()) deadline_->stop();else deadline_->start();
}

void CodexBoard::refresh(bool reset) {
    if (!fixture_.isEmpty()) return;
    if (reset) {sessionCursor_.clear();olderSessions_=false;}
    request("sessions",{{"archived",archives_->isChecked()}});
}

void CodexBoard::updateSessions(const QJsonObject& result,bool append) {
    const auto newSource=result["source"].toString();if (newSource!=source_) {append=false;olderSessions_=false;olderHistory_=false;historyCursor_.clear();}
    source_=newSource;
    if (!append && olderSessions_) {
        // Refresh the first page while retaining older history rows. Runtime samples
        // outside that page are no longer current and must lose their live badges.
        auto fresh=result["sessions"].toArray();QSet<QString> ids;
        for (const auto v:fresh) ids.insert(v.toObject()["id"].toString());
        for (const auto v:sessions_) {auto row=v.toObject();if (!ids.contains(row["id"].toString())) {if (row["status"]!="history") row["status"]="unknown";fresh.append(row);}}
        sessions_=fresh;
    } else if (!append) {sessions_=result["sessions"].toArray();sessionCursor_=result["nextCursor"].toString();}
    else {QSet<QString> ids;for (const auto v:sessions_) ids.insert(v.toObject()["id"].toString());for (const auto v:result["sessions"].toArray()) if (!ids.contains(v.toObject()["id"].toString())) sessions_.append(v);}
    if (append) sessionCursor_=result["nextCursor"].toString();
    connection_->setText(result["notice"].toString()+"   ·   "+QDateTime::currentDateTime().toString("HH:mm:ss")+" Updated");
    statusDelegate_->observe(sessions_);buildSessions();moreSessions_->setVisible(!sessionCursor_.isEmpty());
    if (selectedId_.isEmpty() && list_->topLevelItemCount()>0) list_->setCurrentItem(list_->topLevelItem(0));
    if (!selectedId_.isEmpty() && !historyBusy_ && !olderHistory_) loadHistory();
}

void CodexBoard::buildSessions() {
    QSet<QString> expanded;QString old=selectedId_;for (QTreeWidgetItemIterator it(list_);*it;++it) if ((*it)->isExpanded()) expanded.insert((*it)->data(0,IdRole).toString());
    const int scroll=list_->verticalScrollBar()->value();QSignalBlocker blocker(list_);list_->clear();
    QHash<QString,QJsonObject> rows;QSet<QString> visible;
    const auto search=search_->text().trimmed();const auto filter=filter_->currentData().toString();
    int active=0,attention=0,stored=0;
    for (const auto v:sessions_) {
        const auto row=v.toObject();const auto id=row["id"].toString(),state=row["status"].toString();rows[id]=row;
        const bool live=(state=="running"||state=="approval"||state=="input");
        if (live) ++active;
        if (state=="approval"||state=="input"||state=="error") ++attention;
        if (state=="history") ++stored;
        if ((filter=="live"&&!live)||(filter=="attention"&&state!="approval"&&state!="input"&&state!="error")||(filter=="history"&&state!="history")) continue;
        const auto hay=row["title"].toString()+" "+row["cwd"].toString()+" "+row["agent"].toString()+" "+row["role"].toString()+" "+id;
        if (search.isEmpty()||hay.contains(search,Qt::CaseInsensitive)) visible.insert(id);
    }
    // Retain matching agents' ancestors, including when the parent itself doesn't match the filter.
    const auto matches=visible;
    for (const auto& id:matches) {QSet<QString> seen;auto parent=rows.value(id)["parentId"].toString();while (rows.contains(parent)&&!seen.contains(parent)) {visible.insert(parent);seen.insert(parent);parent=rows[parent]["parentId"].toString();}}
    QHash<QString,QTreeWidgetItem*> items;
    for (const auto v:sessions_) {
        const auto row=v.toObject();const auto id=row["id"].toString();if (!visible.contains(id)||items.contains(id)) continue;
        auto* item=new QTreeWidgetItem;items[id]=item;const auto state=row["status"].toString();
        const auto agent=row["agent"].toString();auto title=row["title"].toString();if (!agent.isEmpty()&&agent!=title) title=agent+" · "+title;
        item->setText(0,title);item->setText(2,row["project"].toString());item->setText(3,when(qint64(row["updatedAt"].toDouble())));
        item->setData(0,IdRole,id);item->setData(1,IdRole,id);item->setData(1,StateRole,state);item->setSizeHint(0,QSize(0,54));
        item->setToolTip(0,Qt::convertFromPlainText(title+"\n"+id+"\n"+row["cwd"].toString()+"\n"+row["role"].toString()));item->setToolTip(1,explanation(state));item->setToolTip(2,Qt::convertFromPlainText(row["cwd"].toString()));
    }
    for (const auto v:sessions_) {
        const auto row=v.toObject();const auto id=row["id"].toString();if (!items.contains(id)) continue;
        const auto parent=row["parentId"].toString();bool cycle=false;QSet<QString> seen{id};auto ancestor=parent;
        while (rows.contains(ancestor)) {if (seen.contains(ancestor)) {cycle=true;break;}seen.insert(ancestor);ancestor=rows[ancestor]["parentId"].toString();}
        if (!cycle&&items.contains(parent)) items[parent]->addChild(items[id]);else list_->addTopLevelItem(items[id]);
        items[id]->setExpanded(expanded.contains(id)||(!search.isEmpty()));
    }
    summary_->setText(QString("%1 sessions   /   active or waiting %2   /   needs attention %3   /   history %4").arg(sessions_.size()).arg(active).arg(attention).arg(stored));
    if (items.contains(old)) {list_->setCurrentItem(items[old]);for (auto* p=items[old]->parent();p;p=p->parent()) p->setExpanded(true);}
    else if (!old.isEmpty()) {selectedId_.clear();turns_={};buildHistory();selection_->setText("Select a session");content_->clear();}
    list_->verticalScrollBar()->setValue(scroll);
    syncBoard();
    if (sessions_.isEmpty()) {historyNote_->setText("No sessions found. Check CODEX_HOME and the archive filter.");}
}

void CodexBoard::selected() {
    auto* item=list_->currentItem();if (!item) return;const auto id=item->data(0,IdRole).toString();if (id==selectedId_) return;
    selectedId_=id;turns_={};historyCursor_.clear();olderHistory_=false;historyNote_->setText("Loading history…");buildHistory();content_->clear();
    selection_->setText(item->text(0));selection_->setToolTip(item->toolTip(0));loadHistory();syncHistoryBoard();
}

void CodexBoard::loadHistory(bool append) {
    if (selectedId_.isEmpty()) return;
    if (!fixture_.isEmpty()) {auto result=fixtureHistories_[selectedId_].toObject();result["threadId"]=selectedId_;updateHistory(result,false);return;}
    QJsonObject params{{"threadId",selectedId_},{"source",source_}};if (append) params["cursor"]=historyCursor_;request("history",params,append);
}

void CodexBoard::updateHistory(const QJsonObject& result,bool append) {
    if (result["threadId"].toString()!=selectedId_) return;
    if (!append) turns_=result["turns"].toArray();else for (const auto turn:result["turns"].toArray()) turns_.append(turn);
    historyCursor_=result["nextCursor"].toString();moreHistory_->setVisible(!historyCursor_.isEmpty());historyNote_->setText(result["notice"].toString());buildHistory();
}

void CodexBoard::buildHistory() {
    QString selected;QSet<QString> expanded;for (QTreeWidgetItemIterator it(history_);*it;++it) if ((*it)->isExpanded()) expanded.insert((*it)->data(0,IdRole).toString());
    if (history_->currentItem()) selected=history_->currentItem()->data(0,IdRole).toString();
    const int scroll=history_->verticalScrollBar()->value();QSignalBlocker blocker(history_);history_->clear();
    QTreeWidgetItem* restore=nullptr;
    for (const auto value:turns_) {
        const auto turn=value.toObject();const auto id=turn["id"].toString();auto* top=new QTreeWidgetItem(history_);
        const qint64 time=qint64(turn["time"].toDouble());const QString date=time>0?QDateTime::fromSecsSinceEpoch(time).toString("MM.dd HH:mm"):"No timestamp";
        top->setText(0,date+" · "+id.left(8));top->setText(1,turnLabel(turn["status"].toString()));top->setData(0,IdRole,id);
        top->setData(0,BodyRole,"Turn "+id+"\n"+turnLabel(turn["status"].toString())+"\n\nThis outcome applies to this turn only.");
        int number=0;
        for (const auto itemValue:turn["items"].toArray()) {
            const auto item=itemValue.toObject();auto* child=new QTreeWidgetItem(top);const auto key=id+"/"+(item["id"].toString().isEmpty()?QString::number(number):item["id"].toString());++number;
            auto title=item["label"].toString();auto body=item["text"].toString();const auto first=body.section('\n',0,0).simplified();if (!first.isEmpty()) title+=" · "+first.left(100);
            child->setText(0,title);child->setText(1,item["status"].toString()=="completed"?QString("Completed"):turnLabel(item["status"].toString()));child->setToolTip(0,Qt::convertFromPlainText(title));child->setData(0,IdRole,key);child->setData(0,BodyRole,item["label"].toString()+"\n\n"+body);
            if (key==selected) restore=child;
        }
        top->setExpanded(expanded.contains(id)||(expanded.isEmpty()&&history_->topLevelItemCount()==1));if (id==selected) restore=top;
    }
    if (restore) {history_->setCurrentItem(restore);const auto text=restore->data(0,BodyRole).toString();if (content_->toPlainText()!=text) content_->setPlainText(text);}
    history_->verticalScrollBar()->setValue(scroll);moreHistory_->setVisible(!historyCursor_.isEmpty());syncHistoryBoard();
}

void CodexBoard::disconnected(const QString& reason) {
    pending_.clear();buffer_.clear();sessionBusy_=historyBusy_=false;deadline_->stop();
    connection_->setText("Disconnected · "+reason);
    for (int i=0;i<sessions_.size();++i) {auto row=sessions_[i].toObject();if (row["status"]!="history") row["status"]="stale";sessions_[i]=row;}
    statusDelegate_->observe(sessions_);buildSessions();
}
void CodexBoard::keyPressEvent(QKeyEvent* event) {
    if (event->key()==Qt::Key_F11) {isFullScreen()?showNormal():showFullScreen();event->accept();return;}
    if (event->key()==Qt::Key_Escape) {if (historyVisible()) showSessionBoard();else close();event->accept();return;}
    QWidget::keyPressEvent(event);
}
void CodexBoard::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
}

void CodexBoard::syncBoard() {
    QHash<QString,QJsonObject> rows;
    for (const auto value:sessions_) rows[value.toObject()["id"].toString()]=value.toObject();
    QJsonArray visible;
    visibleIds_.clear();
    std::function<void(QTreeWidgetItem*,int)> append=[&](QTreeWidgetItem* item,int depth){
        const auto id=item->data(0,IdRole).toString();
        auto row=rows.value(id);row["_depth"]=depth;row["_children"]=item->childCount();
        visible.append(row);visibleIds_.append(id);
        if (item->isExpanded()) for (int i=0;i<item->childCount();++i) append(item->child(i),depth+1);
    };
    for (int i=0;i<list_->topLevelItemCount();++i) append(list_->topLevelItem(i),0);
    QString notice=source_=="demo"?"DEMO DATA":source_=="appServer"?"Codex connected · read only":"Local history · runtime unknown";
    if (connection_->text().startsWith("Disconnected")) notice="Disconnected · previous state unknown";
    flap_->SetCodexRows(visible,notice+" | DOUBLE-CLICK: HISTORY | ← → PAGE");
    syncHistoryBoard();
}

bool CodexBoard::historyVisible() const { return pages_->currentWidget()==historyFlap_; }

void CodexBoard::openSessionHistory(int row) {
    if (row<0 || row>=visibleIds_.size()) return;
    const auto id=visibleIds_[row];
    historySessionId_=id;historySession_={};
    for (const auto& value:sessions_) if (value.toObject()["id"].toString()==id) {historySession_=value.toObject();break;}
    const bool same=selectedId_==id;
    historyFlap_->SetCodexHistory({QJsonObject{{"event","HISTORY"},{"text","Loading recent history..."},{"result","LOADING"}}},historySession_,"PARTIAL HISTORY");
    for (QTreeWidgetItemIterator it(list_);*it;++it) if ((*it)->data(0,IdRole).toString()==id) {list_->setCurrentItem(*it);break;}
    if (same && !historyBusy_) loadHistory();
    details_->hide();syncHistoryBoard();pages_->setCurrentWidget(historyFlap_);
    setWindowTitle("Oh My Departure — Session history");historyFlap_->setFocus();
}

void CodexBoard::showSessionBoard() {
    pages_->setCurrentWidget(flap_);historySessionId_.clear();historySession_={};
    setWindowTitle("Oh My Departure — Codex");flap_->setFocus();
}

void CodexBoard::syncHistoryBoard() {
    if (historySessionId_.isEmpty()) return;
    QJsonArray rows;
    if (selectedId_==historySessionId_) rows=codexHistoryRows(turns_);
    if (rows.isEmpty()) {
        const bool loading=selectedId_==historySessionId_ && (historyBusy_||historyNote_->text().startsWith("Loading"));
        const bool unavailable=selectedId_!=historySessionId_ || historyNote_->text().startsWith("Cannot read history:")
            || connection_->text().startsWith("Disconnected");
        const auto text=selectedId_!=historySessionId_?QString("Selected session is not available")
            :unavailable?QString("History is currently unavailable")
            :loading?QString("Loading recent history..."):QString("No recorded history in this preview");
        rows.append(QJsonObject{{"event","HISTORY"},{"text",text},{"result",unavailable?"UNAVAILABLE":loading?"LOADING":"EMPTY"}});
    }
    const QString source=connection_->text().startsWith("Disconnected")?"OFFLINE / ":source_=="demo"?"DEMO / ":source_=="appServer"?"CODEX / ":"LOCAL / ";
    historyFlap_->SetCodexHistory(rows,historySession_,source+"PARTIAL HISTORY");
}

void CodexBoard::openDetails(int row) {
    if (row>=0 && row<visibleIds_.size()) {
        const auto id=visibleIds_[row];
        for (QTreeWidgetItemIterator it(list_);*it;++it) {
            if ((*it)->data(0,IdRole).toString()==id) {list_->setCurrentItem(*it);list_->scrollToItem(*it);break;}
        }
    }
    placeDialog(details_);details_->show();placeDialog(details_);details_->raise();details_->activateWindow();
    QTimer::singleShot(80,this,[this]{placeHyprDialog(details_);});
}

void CodexBoard::placeDialog(QDialog* dialog) {
    QScreen* target=screen();if (!target) target=QGuiApplication::primaryScreen();if (!target) return;
    dialog->setScreen(target);
    dialog->ensurePolished();
    const QRect area=target->availableGeometry().adjusted(16,16,-16,-16);
    const QSize frame=dialog->frameGeometry().size()-dialog->size();
    const QSize maximum(qMax(1,area.width()-qMax(0,frame.width())),qMax(1,area.height()-qMax(0,frame.height())));
    // Choose the compact layout before Qt computes its minimum window size.
    // Otherwise the wide header can prevent resizing to a small work area.
    if (dialog==details_) updateDetailsLayout(qMin(dialog->width(),maximum.width()));
    if (dialog->layout()) {dialog->layout()->invalidate();dialog->layout()->activate();}
    dialog->setMaximumSize(maximum);dialog->resize(dialog->size().boundedTo(maximum));
    QRect geometry=dialog->frameGeometry();geometry.moveCenter(area.center());dialog->move(geometry.topLeft());
}

void CodexBoard::openSettings() {
    placeDialog(settings_);settings_->show();placeDialog(settings_);settings_->raise();settings_->activateWindow();
    QTimer::singleShot(80,this,[this]{placeHyprDialog(settings_);});
}

void CodexBoard::placeHyprDialog(QDialog* dialog,int attempt) {
    // Wayland does not allow QWidget::move to place a toplevel. Omarchy's
    // installed Hyprland 0.56 API can float/size/centre one explicitly selected
    // window. Match both our PID and the fixed dialog title; never "active".
    if (!dialog->isVisible() || qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE")
        || QStandardPaths::findExecutable("hyprctl").isEmpty()) return;
    auto* query=new QProcess(this);query->setObjectName("dialogPlacement");
    connect(query,&QProcess::errorOccurred,query,&QObject::deleteLater);
    QPointer<QDialog> target=dialog;
    connect(query,&QProcess::finished,this,[this,query,target,attempt](int code,QProcess::ExitStatus status){
        const auto clients=QJsonDocument::fromJson(query->readAllStandardOutput()).array();query->deleteLater();
        if (!target || !target->isVisible() || code || status!=QProcess::NormalExit) return;
        QString address;
        for (const auto value:clients) {const auto client=value.toObject();
            if (qint64(client["pid"].toDouble())==QCoreApplication::applicationPid()
                && client["title"].toString()==target->windowTitle()) {address=client["address"].toString();break;}
        }
        if (address.isEmpty()) {
            if (attempt<4) QTimer::singleShot(80,this,[this,target,attempt]{if(target) placeHyprDialog(target,attempt+1);});
            return;
        }
        static const QRegularExpression valid("^0x[0-9a-fA-F]+$");if (!valid.match(address).hasMatch()) return;
        const QString selector="address:"+address;
        const QString command=QString("dispatch hl.dsp.window.float({ window = \"%1\", action = \"enable\" }); "
            "dispatch hl.dsp.window.resize({ window = \"%1\", x = %2, y = %3 }); "
            "dispatch hl.dsp.window.center({ window = \"%1\" })").arg(selector).arg(target->width()).arg(target->height());
        auto* apply=new QProcess(this);apply->setObjectName("dialogPlacement");
        connect(apply,&QProcess::errorOccurred,apply,&QObject::deleteLater);
        apply->setStandardOutputFile(QProcess::nullDevice());apply->setStandardErrorFile(QProcess::nullDevice());
        connect(apply,&QProcess::finished,apply,&QObject::deleteLater);
        QTimer::singleShot(1000,apply,[apply]{if(apply->state()!=QProcess::NotRunning) apply->kill();});
        apply->start("hyprctl",{"--batch",command});
    });
    query->setStandardErrorFile(QProcess::nullDevice());
    QTimer::singleShot(1000,query,[query]{if(query->state()!=QProcess::NotRunning) query->kill();});
    query->start("hyprctl",{"-j","clients"});
}

void CodexBoard::updateDetailsLayout(int width) {
    detailsHeader_->setDirection(width<1000?QBoxLayout::TopToBottom:QBoxLayout::LeftToRight);
    splitter_->setOrientation(width<1150?Qt::Vertical:Qt::Horizontal);
    list_->setColumnHidden(2,width<850);list_->setColumnHidden(3,width<850);
}

bool CodexBoard::eventFilter(QObject* watched,QEvent* event) {
    if (watched==details_ && event->type()==QEvent::Resize) updateDetailsLayout(details_->width());
    return QWidget::eventFilter(watched,event);
}

void CodexBoard::hideEvent(QHideEvent* event) {
    details_->hide();settings_->hide();QWidget::hideEvent(event);
}
