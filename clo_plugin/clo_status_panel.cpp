#include "clo_status_panel.h"

#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTime>

#include "clo_mcp_server.h"

CloStatusPanel::CloStatusPanel(CloMcpServer* server, const QString& host,
                               quint16 port, QWidget* parent)
    : QWidget(parent), server_(server), host_(host), port_(port) {
    setWindowTitle(QStringLiteral("CLO MCP Listener"));
    setWindowFlags(Qt::Window);
    resize(480, 320);

    statusLabel_ = new QLabel(this);
    toggleBtn_ = new QPushButton(this);
    connect(toggleBtn_, &QPushButton::clicked, this, &CloStatusPanel::onToggleClicked);

    logView_ = new QPlainTextEdit(this);
    logView_->setReadOnly(true);
    logView_->setMaximumBlockCount(2000);   // keep memory bounded on long sessions
    logView_->setLineWrapMode(QPlainTextEdit::NoWrap);

    auto* header = new QHBoxLayout();
    header->addWidget(statusLabel_, /*stretch=*/1);
    header->addWidget(toggleBtn_);

    auto* root = new QVBoxLayout(this);
    root->addLayout(header);
    root->addWidget(logView_, /*stretch=*/1);

    connect(server_, &CloMcpServer::logMessage, this, &CloStatusPanel::appendLog);
    connect(server_, &CloMcpServer::listeningChanged, this, &CloStatusPanel::setListening);
    setListening(server_->isListening());
}

void CloStatusPanel::appendLog(const QString& line) {
    logView_->appendPlainText(
        QStringLiteral("[%1] %2").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), line));
}

void CloStatusPanel::setListening(bool on) {
    if (on) {
        statusLabel_->setText(
            QStringLiteral("<span style='color:#2e7d32;font-weight:bold'>&#9679; Running</span> "
                           "on %1:%2").arg(host_).arg(port_));
        toggleBtn_->setText(QStringLiteral("Stop"));
    } else {
        statusLabel_->setText(
            QStringLiteral("<span style='color:#9e9e9e;font-weight:bold'>&#9675; Stopped</span>"));
        toggleBtn_->setText(QStringLiteral("Start"));
    }
}

void CloStatusPanel::onToggleClicked() {
    if (server_->isListening())
        server_->stop();
    else
        server_->start(host_, port_);   // start() logs failure to the panel itself
}
