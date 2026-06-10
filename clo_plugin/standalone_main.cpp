// Standalone harness: runs CloMcpServer with the stub backend as a plain
// console app -- no CLO, no SDK. Build target `clo_mcp_test`, run it, then point
// the MCP server at 127.0.0.1:5005 and call clo_ping / simulate / etc. This
// proves the network + protocol + dispatch layer end-to-end; only the ICloApi
// backend is swapped (stub -> real CLO) when we build the actual plugin.
#include <QCoreApplication>
#include <QtGlobal>
#include <memory>

#include "clo_mcp_server.h"
#include "clo_api.h"

std::shared_ptr<ICloApi> makeStubCloApi();   // from clo_api_stub.cpp

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    // Optional argv[1]: listen port (default 5005 — taken when real CLO is up).
    quint16 port = 5005;
    if (argc > 1)
        port = static_cast<quint16>(QString::fromLocal8Bit(argv[1]).toUShort());

    CloMcpServer server(makeStubCloApi());
    if (!server.start(QStringLiteral("127.0.0.1"), port)) {
        qWarning("[clo-mcp-test] failed to start server");
        return 1;
    }
    qInfo("[clo-mcp-test] stub server up. Ctrl+C to quit.");
    return app.exec();
}
