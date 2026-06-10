#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QPlainTextEdit;
class CloMcpServer;

// Small non-modal status window for the MCP listener. Lives on CLO's UI thread
// (created in DoFunction) and survives across menu clicks because the plugin
// DLL is pinned. Shows running/stopped state, a Start/Stop button, and a live
// log of every request the listener handles.
class CloStatusPanel : public QWidget {
    Q_OBJECT
public:
    CloStatusPanel(CloMcpServer* server, const QString& host, quint16 port,
                   QWidget* parent = nullptr);

public slots:
    void appendLog(const QString& line);
    void setListening(bool on);

private:
    void onToggleClicked();

    CloMcpServer* server_;     // not owned; both live for the process lifetime
    QString host_;
    quint16 port_;
    QLabel* statusLabel_;
    QPushButton* toggleBtn_;
    QPlainTextEdit* logView_;
};
