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

int main(int argc, char* argv[]) {
    static const char* MAVIS_PATH = "../../mavis/json";
    static const char* CACHE_FILE = ".mavis_json_cache";
    static const char* CPP_FILE = "mavis_json_files.cpp";

    std::vector<std::string> json_files;
    std::vector<std::string> jsons_with_expands;
    std::vector<std::string> excluded_files;

    std::vector<fs::directory_entry> mavis_json_files;
    std::copy_if(fs::directory_iterator(MAVIS_PATH),
                 fs::directory_iterator(),
                 std::back_inserter(mavis_json_files),
                 [](const fs::directory_entry& f) {
                     const auto& json_path = f.path();
                     return fs::is_regular_file(f.status()) &&
                            json_path.extension() == ".json" &&
                            json_path.filename().string().find("64") != std::string::npos;
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

    for(const auto& f: json_files) {
        final_jsons.emplace_back(fs::path(f).filename().string());
    }

    for(const auto& f: jsons_with_expands) {
        final_jsons.emplace_back(fs::path(f).filename().string());
    }

    bool write_files = true;

    if(fs::exists(CACHE_FILE)) {
        std::set<std::string> current_jsons(final_jsons.begin(), final_jsons.end());
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

    try {
        mavis_helpers::Mavis mavis(json_files, {}); // Create a Mavis object to make sure we have a valid set of JSONs
    }
    catch(const mavis::BaseException&) { // Something went wrong - remove all output files
        fs::remove(CPP_FILE);
        fs::remove(CACHE_FILE);
    }

    if(write_files) {
        std::ofstream cpp_os(CPP_FILE);
        std::ofstream cache_os(CACHE_FILE);

        cpp_os << "#include <array>" << std::endl
               << "#include \"mavis_json_files.hpp\"" << std::endl
               << "namespace mavis_helpers {" << std::endl
               << "    const size_t NUM_MAVIS_JSON_FILES = " << final_jsons.size() << ';' << std::endl
               << "    const char* MAVIS_JSON_FILES[NUM_MAVIS_JSON_FILES] = {" << std::endl;

        for(const auto& f: final_jsons) {
            cpp_os << "        \"" << f << "\"," << std::endl;
            cache_os << f << std::endl;
        }

        cpp_os << "    };" << std::endl
               << "} // end namespace mavis_helpers" << std::endl;
    }

    return 0;
}
