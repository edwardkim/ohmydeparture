#include "DesktopBoards.h"
#include "Board.h"
#include "CodexBoard.h"
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>

DesktopBoards::DesktopBoards(int airport,int phase,QSize size,bool windowed)
    :airport_(airport),phase_(phase),size_(size),windowed_(windowed),server_(new QLocalServer(this)) {}
DesktopBoards::~DesktopBoards() = default;

DesktopBoards::StartResult DesktopBoards::start(const QString& view,QString* error) {
    const auto runtime=QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty()) {*error="A private runtime directory is required.";return Failed;}
    const auto socketPath=QDir(runtime).filePath("ohmydeparture-ui.sock");
    lock_=std::make_unique<QLockFile>(QDir(runtime).filePath("ohmydeparture-ui.lock"));
    lock_->setStaleLockTime(0);
    if (!lock_->tryLock(0)) {
        // The first process can still be loading its font / binding the socket.
        for (int attempt=0;attempt<30;++attempt) {
            QLocalSocket socket;
            socket.connectToServer(socketPath);
            if (socket.waitForConnected(50)) {
                socket.write(QJsonDocument(QJsonObject{{"view",view},{"airport",airport_}}).toJson(QJsonDocument::Compact)+'\n');
                socket.flush();
                if (socket.waitForReadyRead(3000) && socket.readAll().contains("\"ok\":true")) return Forwarded;
                *error="The running board did not accept the view request.";return Failed;
            }
            QThread::msleep(40);
        }
        *error="The running board is not responding. Close its old window and try again.";return Failed;
    }
    // Only the lock owner may remove a socket left by a crashed instance.
    QLocalServer::removeServer(socketPath);
    server_->setSocketOptions(QLocalServer::UserAccessOption);
    if (!server_->listen(socketPath)) {*error=server_->errorString();return Failed;}
    connect(server_,&QLocalServer::newConnection,this,[this]{
        while (server_->hasPendingConnections()) {
            auto* socket=server_->nextPendingConnection();
            connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
            QTimer::singleShot(3000,socket,[socket]{socket->abort();socket->deleteLater();});
            connect(socket,&QLocalSocket::readyRead,this,[this,socket]{
                if (socket->bytesAvailable()>4096) {socket->abort();return;}
                if (!socket->canReadLine()) return;
                const auto request=QJsonDocument::fromJson(socket->readLine(4096)).object();
                const auto target=request["view"].toString();
                if (target!="codex" && target!="departures") {socket->write("{\"ok\":false}\n");socket->disconnectFromServer();return;}
                // Hide a modeless details window before switching the main view.
                showView(target,target=="departures"?request["airport"].toInt(-1):-1);
                socket->write("{\"ok\":true}\n");socket->flush();socket->disconnectFromServer();
            });
        }
    });
    return Primary;
}

void DesktopBoards::showView(const QString& view,int airport) {
    if (current_) {
        windowed_=!current_->isFullScreen();
        size_=current_->isFullScreen()?current_->normalGeometry().size():current_->size();
    }
    QWidget* target;
    if (view=="codex") {
        if (!codex_) {
            codex_=std::make_unique<CodexBoard>();
            codex_->setDeparturesHandler([this](int index){showView("departures",index);});
        }
        target=codex_.get();
    } else {
        if (!departures_) {
            departures_=std::make_unique<BoardView>(BRect(0,0,size_.width()-1,size_.height()-1),airport_);
            departures_->setWindowTitle("Oh My Departure");departures_->setMinimumSize(480,240);
            if (phase_>=0) departures_->SetFixedPhase(phase_);
            departures_->SetCodexHandler([this]{showView("codex");});
        }
        if (airport>=0) departures_->SetAirport(airport);
        target=departures_.get();
    }
    if (current_ && current_!=target) current_->hide();
    current_=target;
    target->resize(size_.expandedTo(target->minimumSize()));
    if (windowed_) target->showNormal();else target->showFullScreen();
    target->raise();target->activateWindow();
}
