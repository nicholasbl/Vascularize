#include "mesh_write.h"
#include "global.h"
#include "jobcontroller.h"
#include "voxelmesh.h"
#include "xrange.h"

#include <openvdb/tools/Composite.h>
#include <openvdb/tools/VolumeToMesh.h>

#include <fstream>

using ByteTree = openvdb::tree::Tree4<uint8_t, 5, 4, 3>::Type;
using ByteGrid = openvdb::Grid<ByteTree>;

constexpr float RAIDUS_SCALE      = .01F;
constexpr float PI                = static_cast<float>(M_PI);
constexpr float MINIMUM_RADIUS    = 0.0001F;
constexpr int   VOXEL_INFLATION   = 5;
constexpr float RELAXATION_FACTOR = .5F;


///
/// \brief Given a flow, map this to a radius
///
float compute_radius(float flow) {
    return std::max<float>(std::sqrt(flow / PI) * RAIDUS_SCALE, MINIMUM_RADIUS);
}

///
/// \brief Prune leaves and nodes that dont meet the flow requirements
///
void prune(SimpleGraph& G) {
    int   rounds = global_configuration().prune_rounds;
    float flow   = global_configuration().prune_flow;

    if (flow > 0) {
        // We can't erase in one step, as there is some iterator weirdness,
        // so...

        std::unordered_set<int64_t> to_erase;

        for (auto const& [nid, data] : G.nodes()) {
            if (G.node(nid).flow < flow) {
                to_erase.insert(nid);
            }
        }

        for (auto nid : to_erase) {
            G.remove_node(nid);
        }

        fmt::print(
            "Culled {} nodes based on flow < {}\n", to_erase.size(), flow);
    }


    for (int i : xrange(rounds)) {

        size_t pruned_count = 0;

        std::unordered_map<int64_t, int> degrees;

        for (auto const& [nid, data] : G.nodes()) {
            degrees[nid] = G.edge(nid).size();
        }


        for (auto const& [nid, deg] : degrees) {
            if (deg == 1) {
                pruned_count++;
                G.remove_node(nid);
            }
        }

        fmt::print("Prune round {}, removed {}\n", i, pruned_count);
    }
}

///
/// \brief Relax node positions
/// \param G Graph
/// \param a Upstream node
/// \param n Node
/// \param b Downstream node
///
void relax_part(SimpleGraph& G, int64_t a, int64_t n, int64_t b) {
    auto apos = G.node(a).position;
    auto npos = G.node(n).position;
    auto bpos = G.node(b).position;

    glm::vec3 midpoint = (apos + bpos) / 2.0f;

    glm::vec3 new_pos = (midpoint - npos) * RELAXATION_FACTOR + npos;

    G.node(n).position = new_pos;
}

///
/// \brief Relax all nodes
///
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

struct DistanceToLineInfo {
    float distance; ///< closest distance
    float t;        ///< barycentric coordinate of closest point; 0 -> a, 1 -> b
};

///
/// \brief Find the minimum distance of a point to a line
/// \param p Point in question
/// \param a Line segment start
/// \param b Line segment end
///
DistanceToLineInfo min_distance_to_line(glm::vec3 p, glm::vec3 a, glm::vec3 b) {
    glm::vec3 lunit = b - a;
    float     len2  = glm::distance2(a, b);

    if (len2 <= 0) return { glm::distance(a, p), 0 };

    float t = std::clamp(glm::dot(p - a, b - a) / len2, 0.0F, 1.0F);

    glm::vec3 proj = a + t * lunit;
    return { glm::distance(p, proj), t };
}

///
/// \brief Write a single edge to a voxel grid
/// \param G Flow graph
/// \param edge Edge to write
/// \param grid Voxel grid to modify
///
/// Writes a 0 to 255 value to the grid, where 0 -> not in vessel, 255 -> in
/// vessel, with a smooth transition between.
///
void write_edge(SimpleGraph const& G, Edge const& edge, ByteGrid& grid) {

    auto from_id = edge.a;
    auto to_id   = edge.b;

    auto const& a = G.node(from_id);
    auto const& b = G.node(to_id);

    auto a_position = a.position * float(VOXEL_INFLATION);
    auto b_position = b.position * float(VOXEL_INFLATION);

    float size_a = compute_radius(a.flow) * VOXEL_INFLATION;
    float size_b = compute_radius(b.flow) * VOXEL_INFLATION;

    // if the size of the start is much larger than the end, we are going to
    // clamp the size to end.
    if (glm::abs(size_a - size_b) > 10) {
        size_a = size_b;
    }

    // find the bounds of the grid that we should go over. this is our max
    // radius + fuzzy radius from the line. so the bounding box of the line +-
    // fuzzy distance

    float max_radius = std::max(size_a, size_b);

    float fuzzy_distance = 1 * VOXEL_INFLATION;

    float total_distance = fuzzy_distance + max_radius;


    BoundingBox bb;
    bb.box_union(a.position);
    bb.box_union(b.position);

    auto box_from =
        glm::ivec3(glm::floor(bb.minimum() - glm::vec3(total_distance))) *
        VOXEL_INFLATION;
    auto box_to =
        glm::ivec3(glm::ceil(bb.maximum() + glm::vec3(total_distance))) *
        VOXEL_INFLATION;

    auto accessor = grid.getAccessor();

    for (int i : xrange(box_from.x, box_to.x)) {
        for (int j : xrange(box_from.y, box_to.y)) {
            for (int k : xrange(box_from.z, box_to.z)) {

                // what is our distance to the line?

                auto [distance, t] =
                    min_distance_to_line({ i, j, k }, a_position, b_position);

                // what is the radius of the vessel at this point
                float effective_radius = glm::mix(size_a, size_b, t);

                float value = (distance - effective_radius) / fuzzy_distance;
                value       = 1.0F - std::clamp(value, 0.0F, 1.0F);

                uint8_t compressed_value = 255 * value;

                openvdb::Coord ijk(i, j, k);

                compressed_value =
                    std::max(accessor.getValue(ijk), compressed_value);

                accessor.setValue(ijk, compressed_value);
            }
        }
    }
}

/// \brief Add an edge-voxel modification job to an executor
static std::future<ByteGrid::Ptr>
add_voxel_job(Executor&                           executor,
              SimpleGraph const&                  G,
              std::vector<std::shared_ptr<Edge>>& edge_cache) {
    return executor.enqueue([local_edges = std::move(edge_cache), &G]() {
        auto local_grid = ByteGrid::create(0.0F);

        for (auto const& edge : local_edges) {
            write_edge(G, *edge, *local_grid);
        }

        return local_grid;
    });
}

///
/// \brief Write all the edges of the flow graph to a voxel grid
///
static ByteGrid::Ptr write_edges_to_volume(SimpleGraph const& G) {
    Executor executor;

    size_t const edge_count = G.edges().size();

    size_t const per_thread_edge_count =
        std::ceil(edge_count / double(executor.size()));

    fmt::print("Splitting edges: {} per thread.\n", per_thread_edge_count);

    std::vector<std::shared_ptr<Edge>> edge_cache;

    std::vector<std::future<ByteGrid::Ptr>> future_grids;

    for (auto const& edge : G.edges()) {

        edge_cache.push_back(edge);

        if (edge_cache.size() > per_thread_edge_count) {
            // kick off job
            auto future = add_voxel_job(executor, G, edge_cache);

            future_grids.emplace_back(std::move(future));

            // reset it
            edge_cache = std::vector<std::shared_ptr<Edge>>();
        }
    }

    if (!edge_cache.empty()) {
        auto future = add_voxel_job(executor, G, edge_cache);

        future_grids.emplace_back(std::move(future));
    }


    // for each future grid, merge it into our main grid
    fmt::print("Jobs launched, merging volumes...\n", per_thread_edge_count);

    auto main_grid = ByteGrid::create(0.0F);

    for (auto& future : future_grids) {
        auto ptr = future.get();
        // this does g = max(g, other)
        openvdb::tools::compMax(*main_grid, *ptr);
        // Documentation states that these ops always leave the second grid
        // empty
    }

    return main_grid;
}

void write_mesh_to(SimpleGraph&                 G,
                   SimpleTransform const&       tf,
                   std::filesystem::path const& path) {
    // first prune, this should reduce our load for later steps
    prune(G);

    // relax nodes to reduce harsh bends
    relax(G);

    // voxelize the flow graph. This will use an inflation factor to increase
    // the resolution of the voxel grid to capture fine mesh details.
    // TODO: make the factor automatic
    fmt::print("Writing all edges to output volume.\n");

    auto grid = write_edges_to_volume(G);

    fmt::print("Completed output volume.\n");

    // isosurf the voxel grid
    std::vector<openvdb::Vec3s> position_list;
    std::vector<openvdb::Vec3I> tri_list;
    std::vector<openvdb::Vec4I> quad_list;

    openvdb::tools::volumeToMesh(
        *grid, position_list, tri_list, quad_list, .9 * 255);


    // we need to do the inverse transform to get back to the input mesh
    // coordinate space. We also need to account for the voxel inflation size.
    for (auto& p : position_list) {
        glm::vec3 np(p.x(), p.y(), p.z());
        np = tf.inverted(np);
        np /= VOXEL_INFLATION;
        p = openvdb::Vec3s(np.x, np.y, np.z);
    }

    fmt::print("Generated geometry with {} verts, {} tris, {} quads\n",
               position_list.size(),
               tri_list.size(),
               quad_list.size());

    fmt::print("Writing geometry to {}\n", path.c_str());

    // Dump in .obj format
    {
        std::ofstream stream(path);

        stream << "o vascularization\n";

        stream << "s 1\n";

        for (auto p : position_list) {
            stream << "v " << p.x() << " " << p.y() << " " << p.z() << "\n";
        }

        for (auto f : tri_list) {
            stream << "f " << f.x() + 1 << " " << f.z() + 1 << " " << f.y() + 1
                   << "\n";
        }

        for (auto f : quad_list) {
            stream << "f " << f.y() + 1 << " " << f.x() + 1 << " " << f.w() + 1
                   << " " << f.z() + 1 << "\n";
        }
    }
}
