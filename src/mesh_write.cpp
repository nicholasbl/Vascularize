#include "mesh_write.h"
#include "global.h"
#include "voxelmesh.h"
#include "xrange.h"

#include <fstream>

class MeshWriter {
    std::vector<glm::vec3>  m_position_list;
    std::vector<glm::ivec3> m_face_list;

public:
    std::vector<glm::vec3>& positions() { return m_position_list; }

    int add_vertex(glm::vec3 p) {
        auto place = m_position_list.size();
        m_position_list.push_back(p);
        return place;
    }

    void add_face(int a, int b, int c) {
        assert(a != b and b != c and a != c);

        m_face_list.emplace_back(a + 1, b + 1, c + 1);
    }

    void write_to(std::filesystem::path const& path) {
        std::ofstream stream(path);

        stream << "o vascularization\n";

        for (auto p : m_position_list) {
            stream << "v " << p.x << " " << p.y << " " << p.z << "\n";
        }

        for (auto f : m_face_list) {
            stream << "f " << f.x << " " << f.y << " " << f.z << "\n";
        }
    }
};

struct Basis {
    glm::vec3 up;
    glm::vec3 side;
    glm::vec3 position;
};

std::array<Basis, 2> get_basis(SimpleGraph const& G, int64_t a, int64_t b) {
    auto apos = G.node(a).position;
    auto bpos = G.node(b).position;

    auto dir = glm::normalize(bpos - apos);

    auto starting_up = glm::vec3(1, 0, 0);

    if (dir == starting_up) {
        starting_up = glm::normalize(glm::vec3(1, 1, 0));
    }

    auto side = glm::normalize(glm::cross(starting_up, dir));

    auto up = glm::normalize(glm::cross(dir, side));

    return { { { up, side, apos }, { up, side, bpos } } };
}

void write_ring(MeshWriter&       writer,
                Basis const&      basis,
                float             size,
                std::vector<int>& v_ids) {
    constexpr size_t num_segments = 6;
    v_ids.resize(num_segments);

    for (size_t i : xrange_over(v_ids)) {
        float angle = 2.0 * M_PI * i / float(num_segments);

        glm::vec3 up   = basis.up * glm::sin(angle) * size;
        glm::vec3 side = basis.side * glm::cos(angle) * size;
        glm::vec3 p    = basis.position + up + side;

        v_ids[i] = writer.add_vertex(p);
    }
}

float compute_radius(float flow, float scale = .01) {
    return std::max<float>(std::sqrt(flow / M_PI) * scale, 0.0001f);
}

void prune(SimpleGraph& G, int rounds, float flow) {
    for (int i : xrange(rounds)) {

        std::unordered_map<int64_t, int> degrees;

        for (auto const& [nid, data] : G.nodes()) {
            degrees[nid] = G.edge(nid).size();
        }


        for (auto const& [nid, deg] : degrees) {
            if (deg == 1) G.remove_node(nid);
        }
    }

    std::unordered_set<int64_t> to_erase;

    for (auto const& [nid, data] : G.nodes()) {
        if (G.node(nid).flow < flow) {
            to_erase.insert(nid);
        }
    }

    for (auto nid : to_erase) {
        G.remove_node(nid);
    }
}


void relax_part(SimpleGraph& G, int64_t a, int64_t n, int64_t b) {
    auto apos = G.node(a).position;
    auto npos = G.node(n).position;
    auto bpos = G.node(b).position;

    glm::vec3 midpoint = (apos + bpos) / 2.0f;

    glm::vec3 new_pos = (midpoint - npos) * .5f + npos;

    G.node(n).position = new_pos;
}

void relax(SimpleGraph& G) {
    for (auto const& [nid, ndata] : G.nodes()) {
        for (auto const& ea : G.edge(nid)) {
            for (auto const& eb : G.edge(nid)) {
                if (ea.first == eb.first) continue;

                relax_part(G, ea.first, nid, eb.first);
            }
        }
    }
}

void write_mesh_to(SimpleGraph&                 G,
                   SimpleTransform const&       tf,
                   std::filesystem::path const& path) {
    prune(G,
          global_configuration().prune_rounds,
          global_configuration().prune_flow);

    relax(G);

    MeshWriter writer;

    std::vector<int> vids_a, vids_b;

    for (auto const& edge : G.edges()) {

        auto from_id = edge->a;
        auto to_id   = edge->b;

        float size_a = compute_radius(G.node(from_id).flow);
        float size_b = compute_radius(G.node(to_id).flow);

        auto tbasis = get_basis(G, from_id, to_id);

        write_ring(writer, tbasis[0], size_a, vids_a);
        write_ring(writer, tbasis[1], size_b, vids_b);

        for (size_t i : xrange_over(vids_a)) {
            size_t j = (i + 1) % vids_a.size();

            writer.add_face(vids_a[i], vids_a[j], vids_b[i]);
            writer.add_face(vids_b[j], vids_b[i], vids_a[j]);
        }
    }

    for (auto& p : writer.positions()) {
        p = tf.inverted(p);
    }

    writer.write_to(path);
}
