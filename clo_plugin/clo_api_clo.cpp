// Real ICloApi backend: drives CLO through the SDK singleton.
// Built only when -DCLO_SDK_DIR is set. One-to-one with clo_addon/clo_mcp_listener.py's
// _handle(), but calling the C++ API (CLOAPIInterface.h) instead of the Python modules.
//
// Verified against CLO SDK v4.3.4 (CLO 7.3.240): signatures in
//   CLOAPIInterface/include/{Import,Export,Fabric,Pattern,Utility}APIInterface.h
#include "clo_api.h"

#include "CLOAPIInterface.h"   // -I <CLO_SDK_DIR>/CLOAPIInterface ; defines IMPORT_API/EXPORT_API/...

#include <memory>
#include <stdexcept>

namespace {

class CloApiBackend : public ICloApi {
public:
    void importProject(const std::string& path) override {
        if (!IMPORT_API->ImportFile(path))
            throw std::runtime_error("ImportFile failed: " + path);
    }

    void importAvatar(const std::string& /*path*/, int /*options*/) override {
        // SDK v4.3.4 exposes only ImportAvatar(const wstring&, const Marvelous::ImportExportOption&).
        // Wiring the option struct (see include/CloApiData.h) is a TODO; import_project covers
        // .avt/.zprj/.zpac loading in the meantime.
        throw std::runtime_error("import_avatar not yet wired for SDK v4.3.4 (use import_project)");
    }

    void autoHang(const std::string&, const std::string&, bool) override {
        // No AutoHang in the v4.3.4 C++ API (it is a Python-only binding).
        throw std::runtime_error("auto_hang is not available in CLO SDK v4.3.4");
    }

    void simulate(int frames) override {
        UTILITY_API->Simulate(static_cast<unsigned int>(frames));
    }

    int addFabric(const std::string& path) override {
        return static_cast<int>(FABRIC_API->AddFabric(path));
    }

    void assignFabric(int fabricIndex, int patternIndex) override {
        if (!FABRIC_API->AssignFabricToPattern(static_cast<unsigned int>(fabricIndex),
                                               static_cast<unsigned int>(patternIndex)))
            throw std::runtime_error("AssignFabricToPattern failed");
    }

    void setFabricColor(int colorway, int fabricIndex, int face,
                        double r, double g, double b, double a) override {
        if (!FABRIC_API->SetFabricPBRMaterialBaseColor(
                static_cast<unsigned int>(colorway), static_cast<unsigned int>(fabricIndex),
                static_cast<unsigned int>(face), static_cast<float>(r), static_cast<float>(g),
                static_cast<float>(b), static_cast<float>(a)))
            throw std::runtime_error("SetFabricPBRMaterialBaseColor failed");
    }

    int copyColorway(int index) override {
        const unsigned int created = UTILITY_API->CopyColorway(static_cast<unsigned int>(index));
        UTILITY_API->UpdateColorways(true);
        return static_cast<int>(created);
    }

    void setColorway(int index, const std::string& name) override {
        UTILITY_API->SetCurrentColorwayIndex(static_cast<unsigned int>(index));
        if (!name.empty())
            UTILITY_API->SetColorwayName(static_cast<unsigned int>(index), name);
    }

    std::vector<std::string> renderImage() override {
        // ExportRenderingImage(true) renders all colorways; returns one path list per colorway.
        const std::vector<std::vector<std::string>> nested = EXPORT_API->ExportRenderingImage(true);
        std::vector<std::string> flat;
        for (const auto& inner : nested)
            for (const auto& p : inner) flat.push_back(p);
        return flat;
    }

    std::string exportZprj(const std::string& path) override {
        const std::string out = EXPORT_API->ExportZPrj(path);
        if (out.empty())
            throw std::runtime_error("ExportZPrj returned empty path (export failed)");
        return out;
    }

    int patternCount() override {
        return PATTERN_API->GetPatternCount();
    }

    std::string patternInfo(int patternIndex) override {
        const std::string info = PATTERN_API->GetPatternInformation(patternIndex);
        if (info.empty())
            throw std::runtime_error("GetPatternInformation returned empty (bad pattern index?)");
        return info;
    }

    double lineLength(int patternIndex, int lineIndex) override {
        return static_cast<double>(PATTERN_API->GetLineLength(patternIndex, lineIndex));
    }

    std::vector<std::map<std::string, std::string>> arrangementList() override {
        return PATTERN_API->GetArrangementList();
    }

    void setArrangement(int patternIndex, int arrangementIndex) override {
        PATTERN_API->SetArrangement(patternIndex, arrangementIndex);
    }

    void setArrangementPosition(int patternIndex, int x, int y, int offset) override {
        PATTERN_API->SetArrangementPosition(patternIndex, x, y, offset);
    }

    void addSeam(int patternA, int lineA, int patternB, int lineB,
                 bool directionA, bool directionB) override {
        if (!PATTERN_API->AddSeamlinePairGroup(patternA, lineA, patternB, lineB,
                                               directionA, directionB))
            throw std::runtime_error("AddSeamlinePairGroup failed (bad pattern/line index?)");
    }

    int seamCount() override {
        return PATTERN_API->GetSeamlinePairGroupCount();
    }
};

}  // namespace

std::shared_ptr<ICloApi> makeCloBackend() {
    return std::make_shared<CloApiBackend>();
}
