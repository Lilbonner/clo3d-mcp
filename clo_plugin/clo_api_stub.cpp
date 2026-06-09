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
};

}  // namespace

std::shared_ptr<ICloApi> makeStubCloApi() {
    return std::make_shared<StubCloApi>();
}
