#include <fstream>
#include <iostream>
#include <queue>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "filesystem.hpp"
#include "mavis_helpers.hpp"
#include "tools_util.hpp"
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

class LineIteratorData {
    private:
        std::string data_;

    public:
        friend std::istream& operator>>(std::istream& is, LineIteratorData& l) {
            return std::getline(is, l.data_);
        }

        const std::string& get() const {
            return data_;
        }
};

using LineIterator = std::istream_iterator<LineIteratorData>;

inline void appendJSONs(std::vector<std::string>& final_jsons, const std::vector<std::string>& json_files) {
    std::transform(std::begin(json_files),
                   std::end(json_files),
                   std::back_inserter(final_jsons),
                   [](const auto& f) {
                        return fs::path(f).filename().string();
                   }
    );
}

std::vector<std::string> getJSONs(const char* arch_size) {
    std::vector<std::string> json_files;
    std::vector<std::string> jsons_with_expands;
    std::vector<std::string> excluded_files;

    std::vector<fs::directory_entry> mavis_json_files;
    std::copy_if(fs::directory_iterator(MAVIS_JSON_PATH),
                 fs::directory_iterator(),
                 std::back_inserter(mavis_json_files),
                 [arch_size](const fs::directory_entry& f) {
                     const auto& json_path = f.path();
                     return fs::is_regular_file(f.status()) &&
                            json_path.extension() == ".json" &&
                            json_path.filename().string().find(arch_size) != std::string::npos;
                 });

    std::sort(mavis_json_files.begin(),
              mavis_json_files.end(),
              [](const fs::directory_entry& e1, const fs::directory_entry& e2) {
                  return fs::file_size(e1.path()) > fs::file_size(e2.path());
              });

    std::set<std::string> mnemonics;
    for(const auto& f: mavis_json_files) {
        bool is_dupe = false;
        bool has_expands = false;
        const auto& json_path = f.path();
        std::ifstream json_stream(json_path);

        rapidjson::IStreamWrapper isw(json_stream);
        rapidjson::Document d;
        d.ParseStream(isw);

        for(const auto& entry: d.GetArray()) {
            const auto& m = entry["mnemonic"];
            bool inserted = mnemonics.emplace(m.GetString(), m.GetStringLength()).second;
            if(!inserted) {
                is_dupe = true;
                break;
            }

            has_expands |= entry.HasMember("expand");
        }

        if(has_expands) {
            jsons_with_expands.emplace_back(json_path.string());
        }
        else if(!is_dupe) {
            json_files.emplace_back(json_path.string());
        }
    }

    std::vector<std::string> final_jsons;

    appendJSONs(final_jsons, json_files);
    appendJSONs(final_jsons, jsons_with_expands);

    return final_jsons;
}

int main(int argc, char* argv[]) {
    static const char* CACHE_FILE = ".mavis_json_cache";
    static const char* CPP_FILE = "mavis_json_files.cpp";
    static const std::array<const char*, 2> ARCH_SIZES {"32", "64"};

    std::map<const char*, std::vector<std::string>> jsons;
    for(const auto& arch_size: ARCH_SIZES) {
        jsons.emplace(arch_size, getJSONs(arch_size));
    }

    bool write_files = true;

    if(fs::exists(CACHE_FILE)) {
        std::set<std::string> current_jsons;
        for(const auto& p: jsons) {
            std::copy(p.second.begin(), p.second.end(), std::inserter(current_jsons, current_jsons.end()));
        }
        std::set<std::string> cached_jsons;

        std::ifstream cache(CACHE_FILE);
        auto it = LineIterator(cache);
        const auto end_it = LineIterator();
        for(; it != end_it; ++it) {
            cached_jsons.insert(it->get());
        }

        const auto& exe_path = getExecutablePath();
        write_files = ((current_jsons != cached_jsons) ||
                       !fs::exists(CPP_FILE) ||
                       (fs::last_write_time(CACHE_FILE) < fs::last_write_time(exe_path)) ||
                       (fs::last_write_time(CPP_FILE) < fs::last_write_time(exe_path)));
    }

    for(const auto& p: jsons) {
        try {
            std::vector<std::string> test_paths;
            test_paths.reserve(p.second.size());
            std::transform(p.second.begin(),
                           p.second.end(),
                           std::back_inserter(test_paths),
                           [](const std::string& s) { return MAVIS_JSON_PATH "/" + s; });
            mavis_helpers::Mavis mavis(test_paths, {}); // Create a Mavis object to make sure we have a valid set of JSONs
        }
        catch(const mavis::BaseException&) { // Something went wrong - remove all output files
            fs::remove(CPP_FILE);
            fs::remove(CACHE_FILE);
            throw;
        }
    }

    if(write_files) {
        std::ofstream cpp_os(CPP_FILE);
        std::ofstream cache_os(CACHE_FILE);

        cpp_os << "#include \"mavis_json_files.hpp\"" << std::endl
               << "namespace mavis_helpers {" << std::endl;

        for(const auto& p: jsons) {
            cpp_os << "    const std::vector<std::string> MAVIS_JSON_FILES_RV" << p.first << " {" << std::endl;

            for(const auto& f: p.second) {
                cpp_os << "        \"" << f << "\"," << std::endl;
                cache_os << f << std::endl;
            }

            cpp_os << "    };" << std::endl;
        }

        cpp_os << "} // end namespace mavis_helpers" << std::endl;
    }

    return 0;
}
