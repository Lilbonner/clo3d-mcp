// The seam between the network/protocol layer and CLO itself.
//
// CloMcpServer talks only to this interface. Two implementations sit behind it:
//   * StubCloApi   (clo_api_stub.cpp)  - no-ops + plausible fakes, no SDK; lets
//                                        the protocol/server be tested offline.
//   * CloApiBackend (clo_api_clo.cpp)  - the real thing, written against the CLO
//                                        SDK: CLOAPI::APICommand::getInstance()...
//
// Each method maps 1:1 to a command in clo_mcp_listener.py's _handle(). On
// failure, THROW std::runtime_error -- CloMcpServer turns the message into the
// JSON {"ok":false,"error":<message>} reply, matching the Python listener.
#pragma once

#include <string>
#include <vector>
#include <stdexcept>

struct ICloApi {
    virtual ~ICloApi() = default;

    virtual void importProject(const std::string& path) = 0;
    virtual void importAvatar(const std::string& path, int options) = 0;
    virtual void autoHang(const std::string& garment, const std::string& hanger,
                          bool bottom) = 0;
    virtual void simulate(int frames) = 0;
    virtual int  addFabric(const std::string& path) = 0;              // -> fabric_index
    virtual void assignFabric(int fabricIndex, int patternIndex) = 0;
    virtual void setFabricColor(int colorway, int fabricIndex, int face,
                                double r, double g, double b, double a) = 0;
    virtual int  copyColorway(int index) = 0;                         // -> new_index
    virtual void setColorway(int index, const std::string& name) = 0; // empty name = no rename
    virtual std::vector<std::string> renderImage() = 0;               // -> paths
    virtual std::string exportZprj(const std::string& path) = 0;      // -> saved path
    virtual int  patternCount() = 0;                                  // -> count
};
