#include "global.h"

#include <fmt/printf.h>

#include <fstream>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <vector>

static Configuration& config() {
    static Configuration c;
    return c;
}

static auto split_ref(std::string_view str, std::string_view delimiters) {
    std::vector<std::string_view> tokens;

    std::string::size_type last_pos = 0;
    std::string::size_type pos      = str.find_first_of(delimiters, last_pos);

    while (std::string::npos != pos && std::string::npos != last_pos) {
        tokens.push_back(str.substr(last_pos, pos - last_pos));
        last_pos = pos + 1;
        pos      = str.find_first_of(delimiters, last_pos);
    }

    tokens.push_back(str.substr(last_pos, pos - last_pos));

    return tokens;
}

std::string_view trim(std::string_view s) {
    while (!s.empty() and std::isspace(s.at(0))) {
        s = s.substr(1);
    }
    return s;
}

template <class T>
bool wire(std::unordered_map<std::string, std::string> const& map,
          std::string const&                                  v,
          T&                                                  t) {
    auto iter = map.find(v);

    if (iter == map.end()) return false;

    std::stringstream ss(iter->second);

    ss >> t;

    return true;
}

bool parse_arguments(int argc, char* argv[]) {
    if (argc < 2) return EXIT_FAILURE;

    auto& c = config();

    std::unordered_map<std::string, std::string> file_data;

    auto control_file = std::filesystem::path(argv[1]);

    std::ifstream ins(control_file);

    if (!ins.good()) return EXIT_FAILURE;

    fmt::print("Reading file {}\n", argv[1]);

    while (ins.good()) {
        std::string line;
        std::getline(ins, line);

        if (line.empty()) continue;

        auto tokens = split_ref(line, ":");

        if (tokens.size() < 2) continue;

        // fmt::print("Key {}, Value {}\n", tokens.at(0), tokens.at(1));

        file_data[std::string(tokens.at(0))] = trim(tokens.at(1));
    }

    auto control_dir = control_file.parent_path();

    {
        std::string raw_path;
        wire(file_data, "mesh", raw_path);

        c.mesh_path = control_dir / "." / raw_path;
    }

    wire(file_data, "voxel_size", c.cube_size);

    {
        std::string raw_path;

        if (!wire(file_data, "output", raw_path)) {
            c.output_path = "out.obj";
        } else {
            c.output_path = control_dir / "." / raw_path;
        }
    }

    {
        wire(file_data, "prune", c.prune_rounds);

        c.prune_rounds = std::max(0, c.prune_rounds);
    }

    {
        wire(file_data, "prune_flow", c.prune_flow);

        c.prune_flow = std::max(0.0f, c.prune_flow);
    }

    // validate

    if (!std::filesystem::is_regular_file(c.mesh_path)) {
        fmt::print_colored(fmt::red, "{} is not a valid file.", c.mesh_path);

        return false;
    }
    if (c.mesh_path.extension() != ".obj") return false;

    if (c.cube_size <= 0) return false;


    return true;
}

Configuration const& global_configuration() { return config(); }
