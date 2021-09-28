#include <algorithm>
#include <string>
#include <vector>
#include "stf_enums.hpp"
#include "stf_exception.hpp"

namespace mavis_helpers {
    extern const std::vector<std::string> MAVIS_JSON_FILES_RV32;
    extern const std::vector<std::string> MAVIS_JSON_FILES_RV64;

    static inline const std::vector<std::string>& getMavisArray(const stf::INST_IEM iem) {
        switch(iem) {
            case stf::INST_IEM::STF_INST_IEM_RV32:
                return MAVIS_JSON_FILES_RV32;
            case stf::INST_IEM::STF_INST_IEM_RV64:
                return MAVIS_JSON_FILES_RV64;
            case stf::INST_IEM::STF_INST_IEM_INVALID:
                stf_throw("Invalid IEM value specified: INVALID");
            case stf::INST_IEM::STF_INST_IEM_RESERVED:
                stf_throw("Invalid IEM value specified: RESERVED");
        }

        stf_throw("Invalid IEM value specified: " << stf::enums::to_printable_int(iem));
    }

    static inline std::vector<std::string> getMavisJSONs(const std::string& mavis_path, const stf::INST_IEM iem) {
        const auto& mavis_json_array = getMavisArray(iem);

        std::vector<std::string> jsons;
        jsons.reserve(mavis_json_array.size());
        std::transform(mavis_json_array.begin(),
                       mavis_json_array.end(),
                       std::back_inserter(jsons),
                       [&mavis_path](const std::string& f) { return mavis_path + "/json/" + f; });

        return jsons;
    }
}
