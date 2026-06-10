#include "clo_mcp_server.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QElapsedTimer>
#include <QtGlobal>

#include <map>
#include <stdexcept>

CloMcpServer::CloMcpServer(std::shared_ptr<ICloApi> api, QObject* parent)
    : QObject(parent), api_(std::move(api)) {}

CloMcpServer::~CloMcpServer() { stop(); }

bool CloMcpServer::start(const QString& host, quint16 port) {
    if (server_) return true;
    server_ = new QTcpServer(this);
    connect(server_, &QTcpServer::newConnection, this, &CloMcpServer::onNewConnection);
    if (!server_->listen(QHostAddress(host), port)) {
        const QString err = server_->errorString();
        qWarning("[clo-mcp] listen failed on %s:%u: %s",
                 qUtf8Printable(host), port, qUtf8Printable(err));
        delete server_;
        server_ = nullptr;
        emit logMessage(QStringLiteral("listen FAILED on %1:%2 — %3").arg(host).arg(port).arg(err));
        return false;
    }
    qInfo("[clo-mcp] listening on %s:%u (Qt main-thread, responsive)",
          qUtf8Printable(host), port);
    emit logMessage(QStringLiteral("listening on %1:%2").arg(host).arg(port));
    emit listeningChanged(true);
    return true;
}

void CloMcpServer::stop() {
    if (server_) {
        server_->close();
        server_->deleteLater();
        server_ = nullptr;
        qInfo("[clo-mcp] stopped listening");
        emit logMessage(QStringLiteral("stopped listening"));
        emit listeningChanged(false);
    }
}

void CloMcpServer::onNewConnection() {
    while (server_ && server_->hasPendingConnections()) {
        QTcpSocket* sock = server_->nextPendingConnection();
        buffers_.insert(sock, QByteArray());
        connect(sock, &QTcpSocket::readyRead, this, &CloMcpServer::onReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, &CloMcpServer::onDisconnected);
    }
}

void CloMcpServer::onReadyRead() {
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;

    QByteArray& buf = buffers_[sock];
    buf += sock->readAll();

    int nl;
    while ((nl = buf.indexOf('\n')) >= 0) {
        const QByteArray line = buf.left(nl);
        buf.remove(0, nl + 1);
        const QByteArray reply = handleLine(line);
        sock->write(reply);
        sock->write("\n");
        sock->flush();
        if (shutdownRequested_) {
            stop();           // reply already flushed; refuse further connections
            return;
        }
    }
}

void CloMcpServer::onDisconnected() {
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;
    buffers_.remove(sock);
    sock->deleteLater();
}

QByteArray CloMcpServer::handleLine(const QByteArray& line) {
    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(line, &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        emit logMessage(QStringLiteral("request rejected: bad JSON"));
        return QByteArray(R"({"ok":false,"error":"bad JSON"})");
    }
    const QJsonObject req = doc.object();
    const QJsonValue id = req.value("id");
    const QString command = req.value("command").toString();
    const QJsonObject params = req.value("params").toObject();

    QElapsedTimer timer;
    timer.start();
    QJsonObject reply;
    reply.insert("id", id);
    try {
        reply.insert("ok", true);
        reply.insert("result", dispatch(command, params));
        emit logMessage(QStringLiteral("%1 — ok (%2 ms)").arg(command).arg(timer.elapsed()));
    } catch (const std::exception& ex) {
        reply = QJsonObject{};
        reply.insert("id", id);
        reply.insert("ok", false);
        reply.insert("error", QString::fromUtf8(ex.what()));
        emit logMessage(QStringLiteral("%1 — ERROR: %2 (%3 ms)")
                            .arg(command, QString::fromUtf8(ex.what()))
                            .arg(timer.elapsed()));
    }
    return QJsonDocument(reply).toJson(QJsonDocument::Compact);
}

// Maps each command to ICloApi 1:1, mirroring clo_mcp_listener.py's _handle().
QJsonObject CloMcpServer::dispatch(const QString& command, const QJsonObject& params) {
    const auto str = [&](const char* k) { return params.value(k).toString().toStdString(); };

    if (command == "ping")     return QJsonObject{{"clo", true}};
    if (command == "shutdown") { shutdownRequested_ = true; return QJsonObject{{"stopping", true}}; }

    if (command == "import_project") { api_->importProject(str("path")); return {}; }
    if (command == "import_avatar")  { api_->importAvatar(str("path"), params.value("options").toInt(0)); return {}; }
    if (command == "auto_hang")      { api_->autoHang(str("garment"), str("hanger"), params.value("bottom").toBool(false)); return {}; }
    if (command == "simulate")       { api_->simulate(params.value("frames").toInt(60)); return {}; }

    if (command == "add_fabric")     return QJsonObject{{"fabric_index", api_->addFabric(str("path"))}};
    if (command == "assign_fabric")  { api_->assignFabric(params.value("fabric_index").toInt(), params.value("pattern_index").toInt()); return {}; }
    if (command == "set_fabric_color") {
        api_->setFabricColor(params.value("colorway").toInt(0), params.value("fabric_index").toInt(),
                             params.value("face").toInt(0), params.value("r").toDouble(),
                             params.value("g").toDouble(), params.value("b").toDouble(),
                             params.value("a").toDouble(1.0));
        return {};
    }
    if (command == "copy_colorway")  return QJsonObject{{"new_index", api_->copyColorway(params.value("index").toInt(0))}};
    if (command == "set_colorway")   { api_->setColorway(params.value("index").toInt(), str("name")); return {}; }

    if (command == "render_image") {
        QJsonArray paths;
        for (const auto& p : api_->renderImage()) paths.append(QString::fromStdString(p));
        return QJsonObject{{"paths", paths}};
    }
    if (command == "export_zprj")    return QJsonObject{{"path", QString::fromStdString(api_->exportZprj(str("path")))}};
    if (command == "pattern_count")  return QJsonObject{{"count", api_->patternCount()}};

    if (command == "pattern_info")
        return QJsonObject{{"info", QString::fromStdString(api_->patternInfo(params.value("pattern_index").toInt()))}};
    if (command == "line_length")
        return QJsonObject{{"length", api_->lineLength(params.value("pattern_index").toInt(),
                                                       params.value("line_index").toInt())}};
    if (command == "arrangement_list") {
        QJsonArray arrangements;
        for (const auto& entry : api_->arrangementList()) {
            QJsonObject obj;
            for (const auto& kv : entry)
                obj.insert(QString::fromStdString(kv.first), QString::fromStdString(kv.second));
            arrangements.append(obj);
        }
        return QJsonObject{{"arrangements", arrangements}};
    }
    if (command == "set_arrangement") {
        api_->setArrangement(params.value("pattern_index").toInt(),
                             params.value("arrangement_index").toInt());
        return {};
    }
    if (command == "set_arrangement_position") {
        api_->setArrangementPosition(params.value("pattern_index").toInt(),
                                     params.value("x").toInt(), params.value("y").toInt(),
                                     params.value("offset").toInt());
        return {};
    }
    if (command == "add_seam") {
        api_->addSeam(params.value("pattern_a").toInt(), params.value("line_a").toInt(),
                      params.value("pattern_b").toInt(), params.value("line_b").toInt(),
                      params.value("dir_a").toBool(true), params.value("dir_b").toBool(true));
        return {};
    }
    if (command == "seam_count")     return QJsonObject{{"count", api_->seamCount()}};

    if (command == "seam_info") {
        const int groups = api_->seamCount();
        const int patterns = api_->patternCount();
        std::map<int, QJsonArray> members;            // group index -> pattern indices
        for (int p = 0; p < patterns; ++p)
            for (unsigned int g : api_->seamGroupsInPattern(p))
                members[static_cast<int>(g)].append(p);
        QJsonArray seams;
        for (int g = 0; g < groups; ++g) {
            QJsonObject o;
            o.insert("index", g);
            o.insert("name", QString::fromStdString(api_->seamName(g)));
            o.insert("patterns", members.count(g) ? members.at(g) : QJsonArray{});
            seams.append(o);
        }
        return QJsonObject{{"seams", seams}};
    }

    if (command == "create_pattern") {
        std::vector<std::tuple<float, float, int>> points;
        for (const QJsonValue& v : params.value("points").toArray()) {
            const QJsonArray p = v.toArray();
            if (p.size() < 2)
                throw std::runtime_error("create_pattern: each point must be [x, y] or [x, y, type]");
            points.emplace_back(static_cast<float>(p.at(0).toDouble()),
                                static_cast<float>(p.at(1).toDouble()),
                                p.at(2).toInt(0));
        }
        if (points.size() < 3)
            throw std::runtime_error("create_pattern: need at least 3 points");
        return QJsonObject{{"pattern_index", api_->createPattern(points)}};
    }

    throw std::runtime_error("unknown command: " + command.toStdString());
}
