#pragma once
#include <QObject>
#include <QSize>
#include <memory>
class BoardView;
class CodexBoard;
class QLocalServer;
class QLockFile;
class QWidget;

// Only controls this application's views. Never sends Codex agent commands.
class DesktopBoards : public QObject {
public:
    enum StartResult { Primary, Forwarded, Failed };
    DesktopBoards(int airport, int phase, QSize size, bool windowed);
    ~DesktopBoards() override;
    StartResult start(const QString& view, QString* error);
    void showView(const QString& view, int airport = -1);
private:
    int airport_, phase_;
    QSize size_;
    bool windowed_;
    std::unique_ptr<BoardView> departures_;
    std::unique_ptr<CodexBoard> codex_;
    QWidget* current_ = nullptr;
    QLocalServer* server_;
    std::unique_ptr<QLockFile> lock_;
};
