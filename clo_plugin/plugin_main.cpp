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
#include "clo_status_panel.h"
#include "clo_api.h"

// CLO swallows plugin exceptions and qInfo/qWarning go nowhere visible, so the
// only reliable diagnostic channel is a plain file. Append-only; survives crashes.
static void pluginLog(const char* msg) {
    std::ofstream f("C:/Users/Public/Documents/CLO/clo_mcp_plugin.log", std::ios::app);
    f << msg << "\n";
}

#ifdef _WIN32
#  include <windows.h>
BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        char path[MAX_PATH] = "?";
        GetModuleFileNameA(h, path, MAX_PATH);
        pluginLog((std::string("DllMain: attached from ") + path).c_str());
    }
    return TRUE;
}
#  define CLO_PLUGIN_SPECIFIER __declspec(dllexport)
#else
#  define CLO_PLUGIN_SPECIFIER
#endif

std::shared_ptr<ICloApi> makeCloBackend();   // from clo_api_clo.cpp

namespace {
std::unique_ptr<CloMcpServer> g_server;      // persists because the module is pinned (see DoFunction)
CloStatusPanel* g_panel = nullptr;           // owned by Qt's top-level widget list; lives until process exit
constexpr quint16 kPort = 5005;
const QString kHost = QStringLiteral("127.0.0.1");
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

// Menu click: open (or raise) the status panel. The listener itself is
// started/stopped with the panel's button; the first click also auto-starts it.
CLO_PLUGIN_SPECIFIER void DoFunction() {
    pluginLog("DoFunction: entered");
    try {
#ifdef _WIN32
        // CLO loads this DLL fresh for every export call and frees it as soon as
        // DoFunction returns — which would destroy g_server/g_panel with it.
        // Pin the module so they survive; LoadLibrary on the next click returns
        // this same pinned instance.
        HMODULE self = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_PIN,
                                reinterpret_cast<LPCWSTR>(&DoFunction), &self))
            pluginLog("DoFunction: WARNING: failed to pin module");
#endif
        if (!g_server) {
            g_server = std::make_unique<CloMcpServer>(makeCloBackend());
            g_panel = new CloStatusPanel(g_server.get(), kHost, kPort);
            pluginLog("DoFunction: server + panel created");
            if (g_server->start(kHost, kPort))
                pluginLog("DoFunction: listening on 127.0.0.1:5005");
            else
                pluginLog("DoFunction: listen FAILED on port 5005");
        }
        g_panel->show();
        g_panel->raise();
        g_panel->activateWindow();
    } catch (const std::exception& e) {
        pluginLog((std::string("DoFunction: exception: ") + e.what()).c_str());
    } catch (...) {
        pluginLog("DoFunction: unknown exception");
    }
}

CLO_PLUGIN_SPECIFIER void DoFunctionAfterLoadingCLOFile(const char* /*fileExtension*/) {}

}  // extern "C"
