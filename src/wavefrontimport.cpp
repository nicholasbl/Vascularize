#include "wavefrontimport.h"

#include "mutable_mesh.h"
#include "wavefrontimport.h"
#include "xrange.h"

#include <fmt/printf.h>
#include <glm/gtx/norm.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <unordered_map>
#include <vector>

using namespace mesh_detail;

struct WaveFrontVertSpec {
    int32_t p_ind; // we cant just use a negative value here. some
    int32_t t_ind; // variants of the format take joy in using negatives
    int32_t n_ind;

    bool p_ind_valid = false;
    bool t_ind_valid = false;
    bool n_ind_valid = false;

    inline void set_p(int32_t v) {
        p_ind       = v;
        p_ind_valid = true;
    }
    inline void set_t(int32_t v) {
        t_ind       = v;
        t_ind_valid = true;
    }
    inline void set_n(int32_t v) {
        n_ind       = v;
        n_ind_valid = true;
    }
};

inline int32_t sanitize_index(int32_t index) {
    if (index < 0) return index;
    return index - 1;
}

inline int32_t get_raw_index(std::string_view s) {
    int32_t ret;

    auto result = std::from_chars(s.data(), s.data() + s.size(), ret);

    if (result.ec == std::errc::invalid_argument)
        throw std::runtime_error("Malformed wavefront!");
    return ret;
}

inline int32_t get_and_sanitize_index(std::string_view s) {
    return sanitize_index(get_raw_index(s));
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

// till c++20

bool starts_with(std::string_view view, std::string_view content) {
    return content.size() >= view.size() and
           content.compare(0, view.size(), view) == 0;
}

float to_float(std::string_view v) {
    // lacking from_chars support for floats
    return std::stof(std::string(v));
}

static WaveFrontVertSpec from_wavefront_face_string(std::string_view src) {
    WaveFrontVertSpec ret;

    auto splits = split_ref(src, "/");

    auto size = splits.size();

    switch (size) { // congrats on stupid
    case 1:         // vertex only
        ret.set_p(get_and_sanitize_index(splits[0]));
        break;
    case 2: // vertex and texture
        ret.set_p(get_and_sanitize_index(splits[0]));
        ret.set_t(get_and_sanitize_index(splits[1]));
        break;
    case 3: // vertex texture normal
        ret.set_p(get_and_sanitize_index(splits[0]));
        if (!splits[1].empty()) {
            ret.set_t(get_and_sanitize_index(splits[1]));
        }
        ret.set_n(get_and_sanitize_index(splits[2]));
        break;
    default: throw std::runtime_error("Malformed obj face!");
    }
    return ret;
}

template <typename T>
void hash_combine(size_t& seed, T const& v) {
    seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std {
template <>
struct hash<WaveFrontVertSpec> {
    typedef WaveFrontVertSpec argument_type;
    typedef size_t            result_type;

    result_type operator()(argument_type const& s) const {
        size_t seed = 0;
        hash_combine(seed, s.p_ind);
        hash_combine(seed, s.t_ind);
        hash_combine(seed, s.n_ind);
        return seed;
    }
};
} // namespace std

bool operator<(WaveFrontVertSpec const& a, WaveFrontVertSpec const& b) {
    return a.p_ind < b.p_ind and a.t_ind < b.t_ind and a.n_ind < b.n_ind;
}

bool operator==(WaveFrontVertSpec const& a, WaveFrontVertSpec const& b) {
    return a.p_ind == b.p_ind and a.t_ind == b.t_ind and a.n_ind == b.n_ind;
}

struct WaveFrontMtl {
    glm::vec3 Kd = glm::vec3(1); // for now
};

class WaveFrontConverterData {
    std::filesystem::path m_wavefront_file_path;

public:
    std::vector<MutableObject> m_objects;

    void push_new_object(std::string_view n) {
        m_objects.emplace_back(MutableObject());
        current_object().name = std::string(n);

        push_mesh();
    }

    void push_mesh() {
        assert(!m_objects.empty());

        if (current_object().meshes.size() < 400) {
            current_object().meshes.push_back(MutableMesh());
        } else {
            std::string      new_name = current_object().name + "_1";
            std::string_view r(new_name);
            push_new_object(r);
        }
    }

    MutableObject& current_object() {
        if (m_objects.empty()) {
            std::string obj_name = m_wavefront_file_path.filename();
            push_new_object(std::string_view(obj_name));
        }

        return m_objects.back();
    }

    MutableMesh& current_mesh() {
        assert(current_object().meshes.size() > 0);
        return current_object().meshes.back();
    }

    void check() {
        for (auto& obj : m_objects) {
            while (obj.meshes.empty() and obj.meshes.back().vertex().empty()) {
                obj.meshes.pop_back();
            }
        }
    }

    std::unordered_map<WaveFrontVertSpec, int> m_vert_pos_map;

    std::vector<glm::vec3> m_file_verts;
    std::vector<glm::vec2> m_file_tex;
    std::vector<glm::vec3> m_file_nors;

    std::unordered_map<std::string, WaveFrontMtl> m_materials;
    std::string                                   m_current_material_string;
    WaveFrontMtl                                  m_current_material;

    WaveFrontConverterData(std::filesystem::path const& file_path)
        : m_wavefront_file_path(file_path) {
        fmt::print("Loading wavefront: {}\n", file_path);

        if (!std::filesystem::exists(file_path)) {
            fmt::print_colored(fmt::red, "File does not exist\n");
            return;
        }

        auto status = std::filesystem::status(file_path);

        if (status.type() != std::filesystem::file_type::regular) {
            fmt::print("Not a file\n");
            return;
        }

        std::ifstream stream(file_path);

        for (std::string line; std::getline(stream, line);) {
            std::string_view view(line);

            auto splits = split_ref(view, " ");

            if (splits.empty()) continue;
            if (starts_with(splits[0], "#")) continue;

            on_line(splits);
        }
    }

    void import_mtl(std::filesystem::path const& file_path) {
        std::ifstream stream(file_path);

        for (std::string line; std::getline(stream, line);) {
            std::string_view view(line);

            auto splits = split_ref(view, " ");

            if (splits.empty()) continue;

            if (splits[0] == "newmtl") {
                std::string new_mat_name(splits.at(1));
                m_materials[new_mat_name] = WaveFrontMtl();
                m_current_material_string = new_mat_name;
            } else if (splits[0] == "Kd") {
                m_materials[m_current_material_string].Kd =
                    glm::vec4(to_float(splits[1]),
                              to_float(splits[2]),
                              to_float(splits[3]),
                              1);
            }
        }
    }

    // build a vertex from the source [ p, t, n ], including color from current
    // material
    // t and n may be -1 to skip
    Vertex construct_vert(WaveFrontVertSpec components) {
        Vertex v;

        int32_t p_index = components.p_ind;
        if (p_index < 0) {
            // qDebug() << "reset" << p_index << m_file_verts.size() + p_index;
            p_index = m_file_verts.size() + p_index;
        }

        v.position = m_file_verts.at(p_index);

        if (components.t_ind_valid) {
            int32_t t_index = components.t_ind;
            if (t_index < 0) {
                t_index = m_file_tex.size() + t_index;
            }

            // v.texture = m_file_tex.at(t_index);
        }

        if (components.n_ind_valid) {
            int32_t n_index = components.n_ind;
            if (n_index < 0) {
                n_index = m_file_nors.size() + n_index;
            }

            // v.normal = m_file_nors.at(n_index);
        }

        // v.set_color(m_current_material.Kd);
        return v;
    }

    void on_line(std::vector<std::string_view> const& splits) {
        // these are ordered based on likelihood
        if (splits[0] == "v") {
            // next three are floating coords
            glm::vec3 position = { to_float(splits[1]),
                                   to_float(splits[2]),
                                   to_float(splits[3]) };

            m_file_verts.push_back(position);

        } else if (splits[0] == "vn") {
            // next three are floating coords
            glm::vec3 n(
                to_float(splits[1]), to_float(splits[2]), to_float(splits[3]));
            n = glm::normalize(n);

            m_file_nors.push_back(n);

        } else if (splits[0] == "vt") {
            // next two are floating coords
            // there could be more, but hey, a fixme.

            glm::vec2 texture = { to_float(splits[1]), to_float(splits[2]) };

            m_file_tex.push_back(texture);

        } else if (splits[0] == "f") {
            Face face;

            std::array<int, 3> vertex_positions = { { -1, -1, -1 } };

            if (splits.size() != 4) { // not a whole face
                printf("Skipping broken face\n");
                return;
            }

            for (int i : xrange(3)) {
                auto fv = from_wavefront_face_string(splits[i + 1]);

                if (fv.p_ind_valid) {
                    if (fv.p_ind < 0) {
                        fv.p_ind = m_file_verts.size() + fv.p_ind;
                    }
                }

                if (fv.n_ind_valid) {
                    if (fv.n_ind < 0 and !m_file_nors.empty()) {
                        fv.n_ind = m_file_nors.size() + fv.n_ind;
                    }
                }

                if (fv.t_ind_valid) {
                    if (fv.t_ind < 0 and !m_file_tex.empty()) {
                        fv.t_ind = m_file_tex.size() + fv.t_ind;
                    }
                }

                auto iter = m_vert_pos_map.find(fv);

                if (iter == m_vert_pos_map.end()) {
                    vertex_positions[i] = current_mesh().vertex().size();

                    m_vert_pos_map[fv] = vertex_positions[i];

                    Vertex v = construct_vert(fv);

                    current_mesh().add(v);

                } else {
                    vertex_positions[i] = iter->second;
                }

                assert(vertex_positions[i] >= 0);
            }

            for (size_t i : xrange(3ul)) {
                face.indicies[i] = vertex_positions[i];
            }

            current_mesh().add(face);

            if (current_mesh().vertex().size() >= 65000) {
                m_vert_pos_map.clear();
                push_mesh();
                // qDebug() << "Mesh push";
            }
        } else if (splits[0] == "usemtl") {
            // qDebug() << "use material" << splits[1];
            m_current_material_string = std::string(splits[1]);
            auto iter = m_materials.find(m_current_material_string);
            if (iter == m_materials.end()) {
                fmt::print("no material found\n");
            }
            m_current_material = m_materials[m_current_material_string];
        } else if (splits[0] == "g" or splits[0] == "o") {
            std::string default_name("WF OB ");
            default_name += std::to_string(m_objects.size());

            push_new_object(std::string_view(default_name));

        } else if (splits[0] == "mtllib") {
            // do nothing
        }
    }
};

ImportedMesh import_wavefront(std::filesystem::path const& path) {
    WaveFrontConverterData cv(path);
    cv.check();

    ImportedMesh ret;
    ret.objects = std::move(cv.m_objects);


    return ret;
}
