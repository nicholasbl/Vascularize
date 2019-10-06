#include "global.h"

#include <fmt/color.h>
#include <fmt/printf.h>

#include <fstream>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <vector>

///
/// \brief Get config, avoid static init order fiasco
///
static Configuration& config() {
    static Configuration c;
    return c;
}

///
/// \brief Split string into string views/refs
/// \param str String to split
/// \param delimiter Delimiter to split on
///
static auto split_ref(std::string_view str, std::string_view delimiter) {
    std::vector<std::string_view> tokens;

    std::string::size_type last_pos = 0;
    std::string::size_type pos      = str.find_first_of(delimiter, last_pos);

    while (std::string::npos != pos && std::string::npos != last_pos) {
        tokens.push_back(str.substr(last_pos, pos - last_pos));
        last_pos = pos + 1;
        pos      = str.find_first_of(delimiter, last_pos);
    }

    tokens.push_back(str.substr(last_pos, pos - last_pos));

    return tokens;
}

///
/// \brief Trim whitespace off the start of a string
///
std::string_view trim(std::string_view s) {
    while (!s.empty() and std::isspace(s.at(0))) {
        s = s.substr(1);
    }
    return s;
}


std::istream& operator>>(std::istream& stream, glm::vec3& v) {
    return stream >> v.x >> v.y >> v.z;
}

///
/// \brief Check a map for a given key, if it exists, interpret the value as T.
///
template <class T>
bool wire(std::unordered_map<std::string, std::string> const& map,
          std::string const&                                  v,
          T&                                                  t) {
    auto iter = map.find(v);

    if (iter == map.end()) return false;

    std::stringstream ss(iter->second);

    ss >> std::boolalpha >> t;

    return true;
}

///
/// \brief Check a map for a given key, if it exists, interpret the value as T.
///
/// For optional types
///
template <class T>
bool wire(std::unordered_map<std::string, std::string> const& map,
          std::string const&                                  v,
          std::optional<T>&                                   opt) {
    auto iter = map.find(v);

    if (iter == map.end()) return false;

    std::stringstream ss(iter->second);

    T& t = opt.emplace();

    ss >> std::boolalpha >> t;

    return true;
}

bool parse_arguments(int argc, char* argv[]) {
    if (argc < 2) return EXIT_FAILURE;

    auto& c = config();

    // file key value store
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

        file_data[std::string(tokens.at(0))] = trim(tokens.at(1));
    }

    c.control_dir = control_file.parent_path();

    {
        std::string raw_path;
        wire(file_data, "mesh", raw_path);

        c.mesh_path = c.control_dir / "." / raw_path;

        if (!std::filesystem::is_regular_file(c.mesh_path)) {
            fatal("Missing input mesh!");
        }
    }

    wire(file_data, "voxel_size", c.cube_size);

    {
        std::string raw_path;

        if (!wire(file_data, "output", raw_path)) {
            c.output_path = "out.obj";
        } else {
            c.output_path = c.control_dir / "." / raw_path;
        }
    }

    {
        if (wire(file_data, "root_at", c.root_around)) {
            assert(c.root_around);
        }
    }

    {
        wire(file_data, "position_randomness", c.position_randomness);

        c.position_randomness = std::max(0.0f, c.position_randomness);
    }

    {
        wire(file_data, "prune", c.prune_rounds);

        c.prune_rounds = std::max(0, c.prune_rounds);
    }

    {
        wire(file_data, "prune_flow", c.prune_flow);

        c.prune_flow = std::max(0.0F, c.prune_flow);
    }

    wire(file_data, "dump_voxels", c.dump_voxels);

    // validate

    if (!std::filesystem::is_regular_file(c.mesh_path)) {
        fmt::print(fg(fmt::terminal_color::red),
                   "{} is not a valid file.",
                   c.mesh_path);

        return false;
    }
    if (c.mesh_path.extension() != ".obj") return false;

    if (c.cube_size <= 0) return false;


    return true;
}

Configuration const& global_configuration() { return config(); }

[[noreturn]] void
_fatal_msg_impl(const char* file, int line, std::string_view text) noexcept {
    try {
        fmt::print(fmt::fg(fmt::terminal_color::red),
                   "Fatal in {}:{}; {}.",
                   file,
                   line,
                   text);
    } catch (...) {
        // supposedly can happen
        printf("Fatal in %s : %i", file, line);
    }

    exit(EXIT_FAILURE);
}
