#include <string>
#include <vector>

namespace mavis_helpers {
    extern const size_t NUM_MAVIS_JSON_FILES;
    extern const char* MAVIS_JSON_FILES[];

    static inline std::vector<std::string> getMavisJSONs(const std::string& mavis_path) {
        std::vector<std::string> jsons;
        for(size_t i = 0; i < NUM_MAVIS_JSON_FILES; ++i) {
            jsons.emplace_back(mavis_path + "/json/" + MAVIS_JSON_FILES[i]);
        }

        return jsons;
    }
}
