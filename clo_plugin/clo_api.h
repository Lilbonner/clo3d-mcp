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

#include <map>
#include <string>
#include <tuple>
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

    // --- pattern introspection / arrangement / sewing (DXF workflow) --------
    virtual std::string patternInfo(int patternIndex) = 0;            // -> JSON string
    virtual double lineLength(int patternIndex, int lineIndex) = 0;   // -> length
    virtual std::vector<std::map<std::string, std::string>> arrangementList() = 0;
    virtual void setArrangement(int patternIndex, int arrangementIndex) = 0;
    virtual void setArrangementPosition(int patternIndex, int x, int y, int offset) = 0;
    virtual void addSeam(int patternA, int lineA, int patternB, int lineB,
                         bool directionA, bool directionB) = 0;
    virtual int  seamCount() = 0;                                     // -> seamline pair groups
    virtual std::string seamName(int groupIndex) = 0;                 // -> group's display name
    virtual std::vector<unsigned int> seamGroupsInPattern(int patternIndex) = 0;

    // --- pattern drafting ----------------------------------------------------
    // Each point is (x, y, vertexType) in mm; vertexType 0 = straight,
    // 2 = spline curve, 3 = bezier curve. The outline closes automatically.
    virtual int createPattern(
        const std::vector<std::tuple<float, float, int>>& points) = 0;  // -> new pattern index
};
