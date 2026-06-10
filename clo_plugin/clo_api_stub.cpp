// Offline stub of ICloApi: no CLO, no SDK. Every call logs and returns a
// plausible fake. Lets you build clo_mcp_test and exercise the full TCP/JSON
// protocol against the real MCP server before any CLO/SDK is in the picture.
#include "clo_api.h"

#include <QtGlobal>
#include <memory>

namespace {

class StubCloApi : public ICloApi {
public:
    void importProject(const std::string& path) override {
        qInfo("[stub] import_project %s", path.c_str());
    }
    void importAvatar(const std::string& path, int options) override {
        qInfo("[stub] import_avatar %s opts=%d", path.c_str(), options);
    }
    void autoHang(const std::string& garment, const std::string& hanger, bool bottom) override {
        qInfo("[stub] auto_hang %s -> %s bottom=%d", garment.c_str(), hanger.c_str(), bottom);
    }
    void simulate(int frames) override {
        qInfo("[stub] simulate %d frames", frames);
    }
    int addFabric(const std::string& path) override {
        qInfo("[stub] add_fabric %s", path.c_str());
        return 0;
    }
    void assignFabric(int fabricIndex, int patternIndex) override {
        qInfo("[stub] assign_fabric fabric=%d pattern=%d", fabricIndex, patternIndex);
    }
    void setFabricColor(int colorway, int fabricIndex, int face,
                        double r, double g, double b, double a) override {
        qInfo("[stub] set_fabric_color cw=%d fab=%d face=%d rgba=%.2f,%.2f,%.2f,%.2f",
              colorway, fabricIndex, face, r, g, b, a);
    }
    int copyColorway(int index) override {
        qInfo("[stub] copy_colorway %d", index);
        return index + 1;
    }
    void setColorway(int index, const std::string& name) override {
        qInfo("[stub] set_colorway %d name='%s'", index, name.c_str());
    }
    std::vector<std::string> renderImage() override {
        qInfo("[stub] render_image");
        return {"C:/stub/render_Colorway A_1.png"};
    }
    std::string exportZprj(const std::string& path) override {
        qInfo("[stub] export_zprj %s", path.c_str());
        return path;
    }
    int patternCount() override {
        qInfo("[stub] pattern_count");
        return 0;
    }
    std::string patternInfo(int patternIndex) override {
        qInfo("[stub] pattern_info %d", patternIndex);
        return R"({"name":"stub_pattern","outline":{"lineCount":4}})";
    }
    double lineLength(int patternIndex, int lineIndex) override {
        qInfo("[stub] line_length pattern=%d line=%d", patternIndex, lineIndex);
        return 100.0;
    }
    std::vector<std::map<std::string, std::string>> arrangementList() override {
        qInfo("[stub] arrangement_list");
        return {{{"Arrangement Name", "Front"}, {"Type", "Body"}}};
    }
    void setArrangement(int patternIndex, int arrangementIndex) override {
        qInfo("[stub] set_arrangement pattern=%d arrangement=%d", patternIndex, arrangementIndex);
    }
    void setArrangementPosition(int patternIndex, int x, int y, int offset) override {
        qInfo("[stub] set_arrangement_position pattern=%d x=%d y=%d offset=%d",
              patternIndex, x, y, offset);
    }
    void addSeam(int patternA, int lineA, int patternB, int lineB,
                 bool directionA, bool directionB) override {
        qInfo("[stub] add_seam %d:%d <-> %d:%d dirA=%d dirB=%d",
              patternA, lineA, patternB, lineB, directionA, directionB);
    }
    int seamCount() override {
        qInfo("[stub] seam_count");
        return 1;
    }
    std::string seamName(int groupIndex) override {
        qInfo("[stub] seam_name %d", groupIndex);
        return "Seamline Pair Group " + std::to_string(groupIndex);
    }
    std::vector<unsigned int> seamGroupsInPattern(int patternIndex) override {
        qInfo("[stub] seam_groups_in_pattern %d", patternIndex);
        return {0};
    }
    int createPattern(const std::vector<std::tuple<float, float, int>>& points) override {
        qInfo("[stub] create_pattern with %d points", static_cast<int>(points.size()));
        return 0;
    }
};

}  // namespace

std::shared_ptr<ICloApi> makeStubCloApi() {
    return std::make_shared<StubCloApi>();
}
