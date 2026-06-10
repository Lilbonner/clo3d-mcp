#pragma once

#include <QObject>
#include <QByteArray>
#include <QHash>
#include <QString>
#include <memory>

#include "clo_api.h"

class QTcpServer;
class QTcpSocket;
class QJsonObject;

// Owns a QTcpServer that lives on CLO's main (UI) thread. Each client opens one
// short-lived connection, sends one newline-terminated JSON request, gets one
// JSON reply, and disconnects -- exactly what clo_mcp/clo_client.py does.
//
// The whole point: because the QTcpServer is created on CLO's main thread, its
// newConnection/readyRead signals are delivered by CLO's own Qt event loop on
// that thread. So dispatch() -- and the CLO API calls it makes -- run on the
// main thread (the only safe place) WITHOUT a blocking loop. The UI stays live;
// it only stalls for the duration of a genuinely long call (simulate/render),
// which is inherent and identical to clicking the same action by hand.
class CloMcpServer : public QObject {
    Q_OBJECT
public:
    explicit CloMcpServer(std::shared_ptr<ICloApi> api, QObject* parent = nullptr);
    ~CloMcpServer() override;

    bool start(const QString& host = QStringLiteral("127.0.0.1"), quint16 port = 5005);
    void stop();   // stop listening; existing replies still flush
    bool isListening() const { return server_ != nullptr; }

signals:
    void logMessage(const QString& line);     // human-readable event log (UI panel)
    void listeningChanged(bool listening);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    QByteArray handleLine(const QByteArray& line);          // request -> reply (no trailing \n)
    QJsonObject dispatch(const QString& command, const QJsonObject& params);  // throws on failure

    std::shared_ptr<ICloApi> api_;
    QTcpServer* server_ = nullptr;
    QHash<QTcpSocket*, QByteArray> buffers_;   // per-connection accumulation until '\n'
    bool shutdownRequested_ = false;
};
