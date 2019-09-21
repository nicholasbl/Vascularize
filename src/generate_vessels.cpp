#include "generate_vessels.h"

#include "global.h"
#include "jobcontroller.h"
#include "simplegraph.h"
#include "xrange.h"

#include <glm/gtx/norm.hpp>

#include <fmt/printf.h>

#include <fstream>
#include <queue>
#include <random>
#include <stack>
#include <unordered_set>

/// \brief All adjacent directions for a given cell
static std::vector<glm::i64vec3> const directions = []() {
    std::vector<glm::i64vec3> ret;

    std::array<int, 3> ld = { -1, 0, 1 };

    for (int x : ld) {
        for (int y : ld) {
            for (int z : ld) {
                if (x == 0 and y == 0 and z == 0) continue;

                ret.emplace_back(x, y, z);
            }
        }
    }

    return ret;
}();

///
/// \brief Execute a function over a 3D grid.
///
/// \param g Graph to iterate over
/// \param f Function, of signature (size_t i, size_t j, size_t k) -> void;
///
template <class GridType, class Function>
void over_grid(GridType& g, Function&& f) {
    for (size_t i : xrange(g.size_x())) {
        for (size_t j : xrange(g.size_y())) {
            for (size_t k : xrange(g.size_z())) {
                f(i, j, k);
            }
        }
    }
}

///
/// \brief Get an id for a coordinate in a grid.
///
/// \return id, or -1 if coordinate is invalid.
///
template <class T>
int64_t id_for_coord(Grid3D<T> const& v, int64_t x, int64_t y, int64_t z) {
    if (x < 0 or y < 0 or z < 0) return -1;

    auto lx = static_cast<size_t>(x);
    auto ly = static_cast<size_t>(y);
    auto lz = static_cast<size_t>(z);

    if (lx >= v.size_x() or ly >= v.size_y() or lz >= v.size_z()) return -1;

    return v.index(lx, ly, lz);
};

///
/// \brief Build initial superflow graph
/// \param volume_fraction Grid of what is in and outside of a mesh
/// \param G Superflow graph to build into
///
static void build_initial_networks(Grid3D<bool> const& volume_fraction,
                                   SimpleGraph&        G) {

    auto get_id = [&](int64_t x, int64_t y, int64_t z) -> int64_t {
        return id_for_coord(volume_fraction, x, y, z);
    };

    // populate graph with nodes first

    over_grid(volume_fraction, [&](size_t i, size_t j, size_t k) {
        bool is_in = volume_fraction(i, j, k);

        if (!is_in) return;

        int64_t cell_id = get_id(i, j, k);

        assert(cell_id > 0);

        NodeData data;
        data.position = glm::vec3(i, j, k);

        G.add_node(cell_id, data);
    });
}

///@{
/// Random sources and distributions
static std::random_device                    random_device;
static std::mt19937                          random_generator(random_device());
static std::uniform_real_distribution<float> random_distribution_0_1(0.0, 1.0);
static std::uniform_real_distribution<float> random_distribution_1_1(-1.0, 1.0);
///@}

///
/// \brief Compute distances of nodes from the edge of the mesh
/// \param volume_fraction What is in and out of the mesh
/// \param G Superflow graph
/// \param random_scale Random perturbation of the distances
///
/// Distances are normalized 0 -> 1
///
static void compute_distances(Grid3D<bool> const& volume_fraction,
                              SimpleGraph&        G,
                              float               random_scale) {

    // this is stupid
    std::vector<glm::vec3> zero_list;

    over_grid(volume_fraction, [&](size_t i, size_t j, size_t k) {
        bool is_in = volume_fraction(i, j, k);

        if (is_in) return;

        // if all adj are outside, bail.

        bool keep = false;

        for (auto const& dir : directions) {
            auto other_coord = glm::i64vec3(i, j, k) + dir;

            int64_t other_cell_id = id_for_coord(
                volume_fraction, other_coord.x, other_coord.y, other_coord.z);

            if (other_cell_id < 0) continue;

            bool other_is_in = volume_fraction(other_coord);

            if (other_is_in) {
                // adjacent to volume. keep;
                keep = true;
            }
        }

        if (keep) {
            zero_list.emplace_back(i, j, k);
        }
    });

    // compute min distances to zero points
    {
        JobController controller;

        for (auto& [key, node] : G.nodes()) {
            NodeData* ndata = &node.data;

            controller.add_job([ndata, &zero_list, random_scale]() {
                // find min distance

                auto node_coord = ndata->position;

                float min_squared_distance = std::numeric_limits<float>::max();

                for (auto const& z : zero_list) {
                    float d = glm::distance2(node_coord, z);

                    if (d < min_squared_distance) {
                        min_squared_distance = d;
                    }
                }

                float random_value =
                    random_scale * random_distribution_0_1(random_generator);

                ndata->depth = min_squared_distance + random_value;
            });
        }
    }

    // find the max distance
    auto iter = std::max_element(
        G.nodes().begin(), G.nodes().end(), [](auto const& a, auto const& b) {
            return a.second.data.depth < b.second.data.depth;
        });

    float max_distance = iter->second.data.depth;

    for (auto& [key, node] : G.nodes()) {
        node.data.depth = 1.0F - node.data.depth / max_distance;
    }
}

///
/// \brief Connect all adjacent nodes based on high-to-low distances
///
static void connect_all_grad(Grid3D<bool> const& volume_fraction,
                             SimpleGraph&        G) {

    over_grid(volume_fraction, [&](size_t i, size_t j, size_t k) {
        if (!volume_fraction(i, j, k)) return;

        auto this_id = id_for_coord(volume_fraction, i, j, k);


        for (auto const& dir : directions) {
            auto other_coord = glm::i64vec3(i, j, k) + dir;

            int64_t other_cell_id = id_for_coord(
                volume_fraction, other_coord.x, other_coord.y, other_coord.z);

            if (other_cell_id < 0) continue;

            bool other_is_in = volume_fraction(other_coord);

            if (!other_is_in) continue;

            float delta = G.node(this_id).depth - G.node(other_cell_id).depth;

            if (delta < 0) {
                continue;
            }

            EdgeData edata;
            edata.weight = -delta;

            G.add_edge(this_id, other_cell_id, edata);
        }
    });
}

///
/// \brief Generate a random vector using a given radius scale
///
static glm::vec3 ball_random(float radius) {
    return glm::vec3(random_distribution_1_1(random_generator),
                     random_distribution_1_1(random_generator),
                     random_distribution_1_1(random_generator)) *
           radius;
}

///
/// \brief Jitter node positions
///
static void reposition(SimpleGraph& G) {
    for (auto& [key, node] : G.nodes()) {
        node.data.position +=
            ball_random(global_configuration().position_randomness);
    }
}

///
/// \brief Figure a starting node for our flow tree. Picks the lowest distance
/// value.
///
static int64_t get_starting_node(SimpleGraph const& G) {
    // pick node for now

    auto iter = std::min_element(
        G.nodes().begin(), G.nodes().end(), [](auto const& a, auto const& b) {
            return a.second.data.depth < b.second.data.depth;
        });

    assert(iter != G.nodes().end());

    return iter->first;
}

///
/// \brief Build a flow tree
///
static SimpleTree build_tree(std::vector<EdgeKey> const& mst,
                             int64_t                     starting_node) {

    // This is stupid but it makes it easier for me to think about.

    // We create a graph of the tree first

    SimpleGraph precursor;

    for (auto edge : mst) {
        if (!precursor.has_node(edge.a)) {
            precursor.add_node(edge.a, {});
        }

        if (!precursor.has_node(edge.b)) {
            precursor.add_node(edge.b, {});
        }

        assert(!precursor.has_edge(edge.a, edge.b));

        precursor.add_edge(edge.a, edge.b, {});
    }

    assert(precursor.has_node(starting_node));

    assert(precursor.component_count() == 1);

    // Now we build the tree

    SimpleTree tree(starting_node);

    std::unordered_set<int64_t> discovered_set;

    std::stack<int64_t> stack;
    stack.push(starting_node);

    while (!stack.empty()) {
        auto node_id = stack.top();
        stack.pop();

        if (discovered_set.count(node_id)) continue;

        discovered_set.insert(node_id);

        assert(precursor.has_node(node_id));

        for (auto const& [outgoing_id, edge_data] : precursor.edge(node_id)) {

            if (discovered_set.count(outgoing_id)) continue;

            stack.push(outgoing_id);

            assert(!tree.has_node(outgoing_id));

            tree.add_edge(node_id, outgoing_id);
        }
    }

    assert(tree.has_node(starting_node));

    assert(tree.validate_tree());

    assert(precursor.nodes().size() == discovered_set.size());

    assert(precursor.nodes().size() == tree.node_count());

    return tree;
}

///
/// \brief Sort nodes of the tree topologically
///
static std::vector<int64_t> topological_sort(SimpleTree const& tree) {
    std::vector<int64_t> ret;

    std::unordered_map<int64_t, int64_t> in_degree_map;

    for (auto const& [key, value] : tree) {
        in_degree_map[key] += 0; // we just want to create it

        for (auto other_id : value.out_ids) {
            in_degree_map[other_id] += 1;
        }
    }

    std::vector<int64_t> zero_in_degree;

    for (auto const& [key, value] : in_degree_map) {
        assert(value <= 1);
        if (value == 0) {
            zero_in_degree.push_back(key);
        }
    }

    std::erase_if(in_degree_map, [](auto const& c) { return c.second == 0; });


    while (zero_in_degree.size()) {
        auto node = zero_in_degree.back();
        zero_in_degree.pop_back();

        for (auto other_id : tree.get_children_of(node)) {
            in_degree_map.at(other_id) -= 1;

            if (in_degree_map.at(other_id) == 0) {
                zero_in_degree.push_back(other_id);
                in_degree_map.erase(other_id);
            }
        }

        ret.push_back(node);
    }

    if (in_degree_map.size()) {
        throw std::runtime_error("Graph is not DAG!");
    }

    return ret;
}

///
/// \brief Compute 'flow' which is the number of downstream nodes
///
static std::unordered_map<int64_t, float>
compute_flow_size(SimpleTree const& tree) {


    std::unordered_map<int64_t, float> ret;


    std::vector<int64_t> order = topological_sort(tree);

    while (!order.empty()) {

        auto id = order.back();
        order.pop_back();

        assert(tree.has_node(id));

        float sum = tree.get_children_of(id).size();

        for (auto cid : tree.get_children_of(id)) {
            assert(tree.has_node(cid));
            assert(ret.count(cid));
            sum += ret.at(cid);
        }

        ret[id] = sum;
    }


    return ret;
}

///
/// \brief build a final graph from flow, the MST, and the superflow graph
///
static SimpleGraph
build_final_graph(std::unordered_map<int64_t, float> const& flow_data,
                  SimpleTree const&                         tree,
                  SimpleGraph const&                        G) {

    SimpleGraph R;

    for (auto const& [id, ndata] : G.nodes()) {
        NodeData d = ndata.data;
        d.flow     = flow_data.at(id);

        R.add_node(id, d);
    }

    for (auto [key, value] : tree.nodes()) {
        for (auto oid : value.out_ids) {
            R.add_edge(key, oid, {});
        }
    }

    return R;
}

///
/// \brief Dump voxels to a csv
///
static void voxel_debug_dump(Grid3D<bool> const& grid, SimpleGraph const& G) {
    std::ofstream stream("voxels.csv");

    for (size_t i : xrange(grid.size_x())) {
        for (size_t j : xrange(grid.size_y())) {
            for (size_t k : xrange(grid.size_z())) {
                if (!grid(i, j, k)) continue;

                auto id = grid.index(i, j, k);

                stream << i << "," << j << "," << k << "," << G.node(id).depth
                       << std::endl;
            }
        }
    }
}


SimpleGraph generate_vessels(Grid3D<bool> const& volume_fraction) {

    SimpleGraph G;

    fmt::print("Building initial networks\n");

    build_initial_networks(volume_fraction, G);

    fmt::print("Graph has {} nodes. Computing distances\n", G.nodes().size());
    compute_distances(volume_fraction, G, 10);

    if (global_configuration().dump_voxels) {
        voxel_debug_dump(volume_fraction, G);
    }

    fmt::print("Connecting nodes\n");
    connect_all_grad(volume_fraction, G);

    fmt::print("Graph has {} edges. Compute MST\n", G.edge_count());
    auto mst = G.compute_min_spanning_tree();

    reposition(G);

    auto starting_node = get_starting_node(G);

    fmt::print(
        "MST has {} edges. Build tree from {}\n", mst.size(), starting_node);

    auto tree = build_tree(mst, starting_node);

    fmt::print("Tree has {} nodes. Compute flow\n", tree.node_count());

    auto flow = compute_flow_size(tree);

    fmt::print("Flow complete, building final graph\n", tree.node_count());

    return build_final_graph(flow, tree, G);
}
