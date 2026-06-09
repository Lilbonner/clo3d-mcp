// CLO plugin entry point (General API Plugin model, per SDK v4.3.4 sample
// ExportPlugin). CLO loads this .dll at startup and adds a menu action under
// Settings -> Plug-in. Clicking it runs DoFunction() ON THE MAIN THREAD, which
// toggles a QTcpServer serving the MCP protocol. Because the server lives on
// CLO's Qt event loop, commands dispatch on the main thread without blocking the
// UI -- the whole reason for going native (see clo7-no-realtime-hook).
#include <fstream>
#include <memory>
#include <string>
#include <QString>

#include "CLOAPIInterface.h"   // UTILITY_API (DisplayMessageBox) etc.
#include "clo_mcp_server.h"
#include "clo_api.h"

// CLO swallows plugin exceptions and qInfo/qWarning go nowhere visible, so the
// only reliable diagnostic channel is a plain file. Append-only; survives crashes.
static void pluginLog(const char* msg) {
    std::ofstream f("C:/Users/Public/Documents/CLO/clo_mcp_plugin.log", std::ios::app);
    f << msg << "\n";
}

#ifdef _WIN32
#  include <windows.h>
BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) pluginLog("DllMain: attached");
    return TRUE;
}
#  define CLO_PLUGIN_SPECIFIER __declspec(dllexport)
#else
#  define CLO_PLUGIN_SPECIFIER
#endif

std::shared_ptr<ICloApi> makeCloBackend();   // from clo_api_clo.cpp

namespace {
std::unique_ptr<CloMcpServer> g_server;      // persists across DoFunction() calls (DLL stays loaded)
constexpr quint16 kPort = 5005;
}

extern "C" {

CLO_PLUGIN_SPECIFIER const char* GetActionName() {
    pluginLog("GetActionName: called");
    return "MCP Listener (start / stop)";
}

CLO_PLUGIN_SPECIFIER const char* GetObjectNameTreeToAddAction() {
    pluginLog("GetObjectNameTreeToAddAction: called");
    return "menu_Setting / menuPlug_In";     // Settings -> Plug-in, as in the SDK sample
}

CLO_PLUGIN_SPECIFIER int GetPositionIndexToAddAction() {
    return 1;                                // below the anchor item
}

CLO_PLUGIN_SPECIFIER void DoFunction() {
    pluginLog("DoFunction: entered");
    try {
        if (g_server) {                      // running -> stop, hand the port back
            g_server->stop();
            g_server.reset();
            pluginLog("DoFunction: server stopped");
            if (UTILITY_API) UTILITY_API->DisplayMessageBox("MCP listener stopped.");
            return;
        }
        g_server = std::make_unique<CloMcpServer>(makeCloBackend());
        pluginLog("DoFunction: server object created");
        if (g_server->start(QStringLiteral("127.0.0.1"), kPort)) {
            pluginLog("DoFunction: listening on 127.0.0.1:5005");
            if (UTILITY_API) UTILITY_API->DisplayMessageBox(
                "MCP listener running on 127.0.0.1:5005. CLO stays interactive — "
                "click this menu item again to stop.");
            else pluginLog("DoFunction: UTILITY_API is null (no message box)");
        } else {
            g_server.reset();
            pluginLog("DoFunction: listen FAILED on port 5005");
            if (UTILITY_API) UTILITY_API->DisplayMessageBox("MCP listener failed to start (is port 5005 in use?).");
        }
    } catch (const std::exception& e) {
        pluginLog((std::string("DoFunction: exception: ") + e.what()).c_str());
        g_server.reset();
    } catch (...) {
        pluginLog("DoFunction: unknown exception");
        g_server.reset();
    }
}

CLO_PLUGIN_SPECIFIER void DoFunctionAfterLoadingCLOFile(const char* /*fileExtension*/) {}

}  // extern "C"
