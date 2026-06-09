# Build for the CLO real-time MCP plugin.
#
# One SDK-independent core, two products:
#   clo_mcp_test   - standalone executable, STUB backend, NO CLO SDK needed.
#                    Speaks the exact TCP/JSON protocol the MCP server expects,
#                    so you can verify the clo_mcp <-> server plumbing today.
#   clo_mcp_plugin - the CLO plugin (.dll), REAL backend. Enabled by passing
#                    -DCLO_SDK_DIR=<sdk root> (the unzipped CLO_SDK_v7.3.240/).
#
# Target CLO: 7.3.240 -> SDK v4.3.4 -> Qt 5.x (Core + Network here).
# CLO loads RELEASE binaries only and uses the dynamic runtime (/MD).

cmake_minimum_required(VERSION 3.20)
project(clo_mcp_plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
# CLO + its SDK use the DLL runtime; the plugin must match or it won't load.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

# Qt5 for CLO 7.3.x (find_package falls back to Qt5 when Qt6 is absent).
find_package(QT NAMES Qt5 Qt6 REQUIRED COMPONENTS Core Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network)
message(STATUS "clo-mcp: building against Qt${QT_VERSION_MAJOR} ${Qt${QT_VERSION_MAJOR}_VERSION}")

# --- SDK-independent core (network + protocol + dispatch) -------------------
add_library(clo_mcp_core STATIC
    clo_mcp_server.cpp
    clo_mcp_server.h
    clo_api.h
)
target_link_libraries(clo_mcp_core PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
)
target_include_directories(clo_mcp_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# --- standalone test exe (stub backend, no SDK) ----------------------------
add_executable(clo_mcp_test
    standalone_main.cpp
    clo_api_stub.cpp
)
target_link_libraries(clo_mcp_test PRIVATE clo_mcp_core)

# --- the actual CLO plugin (needs the SDK) ---------------------------------
# cmake -S . -B build -DCLO_SDK_DIR=C:/path/to/CLO_SDK_v7.3.240
if(CLO_SDK_DIR)
    set(CLO_IFACE "${CLO_SDK_DIR}/CLOAPIInterface")
    find_library(CLO_API_INTERFACE_LIB NAMES CLOAPIInterface PATHS "${CLO_IFACE}/Lib" REQUIRED)
    message(STATUS "clo-mcp: plugin -> ${CLO_API_INTERFACE_LIB}")

    add_library(clo_mcp_plugin SHARED
        plugin_main.cpp
        clo_api_clo.cpp
    )
    target_include_directories(clo_mcp_plugin PRIVATE "${CLO_IFACE}")
    target_link_libraries(clo_mcp_plugin PRIVATE clo_mcp_core ${CLO_API_INTERFACE_LIB})
    if(MSVC)
        target_compile_definitions(clo_mcp_plugin PRIVATE UNICODE _WINDOWS _USRDLL BOMAPIDLL_EXPORTS)
    endif()
    # CLOAPIInterface.dll must sit next to the plugin (and ship into the Plugins folder).
    add_custom_command(TARGET clo_mcp_plugin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CLO_IFACE}/Lib/CLOAPIInterface.dll" "$<TARGET_FILE_DIR:clo_mcp_plugin>")
else()
    message(STATUS "clo-mcp: CLO_SDK_DIR not set -> building clo_mcp_test only (stub)")
endif()
