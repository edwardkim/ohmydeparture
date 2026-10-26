#pragma once
#include <QWidget>
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <functional>

class QLabel;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class QPlainTextEdit;
class QSplitter;
class QProcess;
class QTimer;
class StatusDelegate;
class BoardView;
class QDialog;
class QSpinBox;
class QDoubleSpinBox;
class QBoxLayout;
class QStackedLayout;

class CodexBoard : public QWidget {
public:
    explicit CodexBoard(const QString& fixture = {}, QWidget* parent = nullptr);
    ~CodexBoard() override;
    bool loadFixture(const QString& path, QString* error);
    void setDeparturesHandler(std::function<void(int)> handler) { departuresHandler_ = std::move(handler); }
    void openDetails(int row = -1);
    void openSessionHistory(int row);
    void showSessionBoard();
    bool historyVisible() const;
    BoardView* historyBoard() const { return historyFlap_; }
    QString historySessionId() const { return historySessionId_; }
    void openSettings();
    BoardView* flapBoard() const { return flap_; }
    QDialog* detailsWindow() const { return details_; }
    QDialog* settingsWindow() const { return settings_; }
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
private:
    struct Pending { QString method, thread; bool append; int generation; };
    void startReader();
    void request(const QString& method, QJsonObject params, bool append = false);
    void receive();
    void refresh(bool reset = true);
    void loadHistory(bool append = false);
    void updateSessions(const QJsonObject& result, bool append);
    void updateHistory(const QJsonObject& result, bool append);
    void buildSessions();
    void buildHistory();
    void selected();
    void disconnected(const QString& reason);
    void syncBoard();
    void syncHistoryBoard();
    void updateDetailsLayout(int width);
    void placeDialog(QDialog* dialog);
    void placeHyprDialog(QDialog* dialog, int attempt = 0);
    std::function<void(int)> departuresHandler_;
    BoardView* flap_;
    BoardView* historyFlap_;
    QStackedLayout* pages_;
    QString historySessionId_;
    QJsonObject historySession_;
    QDialog* details_;
    QDialog* settings_;
    QBoxLayout* detailsHeader_;
    QSpinBox* flipInterval_;
    QDoubleSpinBox* flipDuration_;
    QStringList visibleIds_;
    QString source_, selectedId_, sessionCursor_, historyCursor_, fixture_;
    QJsonArray sessions_, turns_;
    QJsonObject fixtureHistories_;
    QByteArray buffer_;
    QHash<int, Pending> pending_;
    int seq_ = 0, generation_ = 0;
    bool sessionBusy_ = false, historyBusy_ = false, olderHistory_ = false, olderSessions_ = false;
    QLabel *connection_, *summary_, *selection_, *historyNote_, *clock_;
    QLineEdit* search_;
    QComboBox* filter_;
    QCheckBox* archives_;
    QPushButton *moreSessions_, *moreHistory_;
    QTreeWidget *list_, *history_;
    QPlainTextEdit* content_;
    QSplitter* splitter_;
    QProcess* reader_;
    QTimer *poll_, *deadline_;
    StatusDelegate* statusDelegate_;
};
