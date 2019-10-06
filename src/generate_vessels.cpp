#include "generate_vessels.h"

#include "global.h"
#include "jobcontroller.h"
#include "simplegraph.h"
#include "voxelmesh.h"
#include "xrange.h"

#include <glm/gtx/norm.hpp>

#include <fmt/printf.h>

#include <openvdb/tools/GridOperators.h>

#include <fstream>
#include <queue>
#include <random>
#include <stack>
#include <unordered_set>

inline bool is_vfrac_in(float value) { return value > .5F; }

/// \brief All adjacent directions for a given cell
static std::vector<glm::ivec3> const directions = []() {
    std::vector<glm::ivec3> ret;

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
    auto bb = g.evalActiveVoxelBoundingBox();

    auto l = bb.min();
    auto h = bb.max();

    for (int32_t i : xrange(l.x(), h.x())) {
        for (int32_t j : xrange(l.y(), h.y())) {
            for (int32_t k : xrange(l.z(), h.z())) {
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
static int64_t
id_for_coord(openvdb::CoordBBox const& bb, int32_t x, int32_t y, int32_t z) {

    if (!bb.isInside({ x, y, z })) return -1;

    auto lx = static_cast<int64_t>(x);
    auto ly = static_cast<int64_t>(y);
    auto lz = static_cast<int64_t>(z);

    auto hx = bb.max().x();
    auto hy = bb.max().y();


    return lx + hx * (ly + hy * lz);
};

///
/// \brief Build initial superflow graph
/// \param volume_fraction Grid of what is in and outside of a mesh
/// \param G Superflow graph to build into
///
static void build_initial_networks(openvdb::FloatGrid const& volume_fraction,
                                   SimpleTransform const&    transform,
                                   SimpleGraph&              G) {

    auto const& bb = volume_fraction.evalActiveVoxelBoundingBox();

    {
        auto bbmin = bb.min();
        if (bbmin.x() < 0 or bbmin.y() < 0 or bbmin.z() < 0) {
            fatal("Bounding box should be all positive.");
        }
    }


    auto get_id = [&](int32_t x, int32_t y, int32_t z) -> int64_t {
        return id_for_coord(bb, x, y, z);
    };

    // populate graph with nodes
    // we can pre compute distances to the root here

    std::optional<glm::vec3> point;
    float                    max_distance = 0.0F;

    if (global_configuration().root_around) {
        point = transform(*(global_configuration().root_around));
    }

    // compute the distance to the user-defined root, if it exists
    auto distance_to_root = [point, &max_distance](glm::vec3 coordinate) {
        float d;

        if (!point) {
            d = 1.0F;
        } else {
            d = glm::distance2(coordinate, *(point));
        }

        if (d > max_distance) {
            max_distance = d;
        }

        return d;
    };

    auto accessor = volume_fraction.getConstAccessor();

    // add node and fill with distance to root
    over_grid(volume_fraction, [&](int32_t i, int32_t j, int32_t k) {
        bool is_in = is_vfrac_in(accessor.getValue({ i, j, k }));

        if (!is_in) return;

        int64_t cell_id = get_id(i, j, k);

        assert(cell_id > 0);

        NodeData data;
        data.position = glm::vec3(i, j, k);
        data.depth    = distance_to_root(data.position);

        G.add_node(cell_id, data);
    });

    // normalize
    for (auto& [key, value] : G.nodes()) {
        value.data.depth /= max_distance;
    }
}

///@{
/// Random sources and distributions
static std::random_device                    random_device;
static std::mt19937                          random_generator(random_device());
static std::uniform_real_distribution<float> random_distribution_0_1(0.0, 1.0);
static std::uniform_real_distribution<float> random_distribution_1_1(-1.0, 1.0);
///@}


///
/// \brief Consider the distance to the root and distances to the edge of the
/// mesh, and use that to store a 'depth'
///
/// \param volume_fraction Volume fraction
/// \param G Superflow graph
/// \param random_scale Noise scale
///
static void sanitize_distances(openvdb::FloatGrid& volume_fraction,
                               SimpleGraph&        G,
                               float               random_scale) {

    // this is stupid, but we use a list of points that are near the border to
    // compute a distance transform
    std::vector<glm::vec3> zero_list;

    { // compute the zero list

        auto accessor = volume_fraction.getConstAccessor();

        auto bb = volume_fraction.evalActiveVoxelBoundingBox();

        over_grid(volume_fraction, [&](int i, int j, int k) {
            bool is_in = is_vfrac_in(accessor.getValue({ i, j, k }));

            // only want ones outside the volume
            if (is_in) return;

            // if all adj are outside, bail.

            bool keep = false;

            for (auto const& dir : directions) {
                auto other_coord = glm::ivec3(i, j, k) + dir;

                int64_t other_cell_id = id_for_coord(
                    bb, other_coord.x, other_coord.y, other_coord.z);

                if (other_cell_id < 0) continue;

                bool other_is_in = is_vfrac_in(accessor.getValue(
                    { other_coord.x, other_coord.y, other_coord.z }));

                if (other_is_in) {
                    // adjacent to volume. keep;
                    keep = true;
                    break;
                }
            }

            if (keep) {
                zero_list.emplace_back(i, j, k);
            }
        });
    }

    // now compute distances to these points and store the min for each node in
    // the graph

    std::unordered_map<int64_t, float> distances;

    // preallocate, because threads will be concurrently updating this structure
    for (auto& [key, node] : G.nodes()) {
        distances[key] = 0;
    }

    { // compute all in parallel
        // make sure the controller goes out of scope to finish
        JobController controller;

        fmt::print("Computing signed distances to border\n");

        for (auto& [key, node] : G.nodes()) {
            int64_t   nid   = key;
            NodeData* ndata = &node.data;

            controller.add_job(
                [nid, ndata, &zero_list, &distances, random_scale]() {
                    // find min distance
                    auto node_coord = ndata->position;

                    float min_squared_distance =
                        std::numeric_limits<float>::max();

                    for (auto const& z : zero_list) {
                        float d = glm::distance2(node_coord, z);

                        if (d < min_squared_distance) {
                            min_squared_distance = d;
                        }
                    }

                    float random_value = random_scale * random_distribution_0_1(
                                                            random_generator);

                    distances.at(nid) = min_squared_distance + random_value;
                });
        }
    }

    // we want distances to be 0 at the core of the input mesh
    fmt::print("Normalizing\n");

    // find the max distance
    auto iter = std::max_element(
        distances.begin(), distances.end(), [](auto const& a, auto const& b) {
            return a.second < b.second;
        });

    float max_distance = iter->second;

    for (auto& [key, value] : distances) {
        value /= max_distance;
    }

    for (auto& [key, node] : G.nodes()) {
        node.data.depth = 1.0F - (distances.at(key) * node.data.depth);
    }

    if (G.nodes().size() == 0) {
        fatal("No nodes in graph!");
    }
}

///
/// \brief Connect all adjacent nodes based on high-to-low distances
///
static void connect_all_grad(openvdb::FloatGrid const& volume_fraction,
                             SimpleGraph&              G) {

    auto bb       = volume_fraction.evalActiveVoxelBoundingBox();
    auto accessor = volume_fraction.getConstAccessor();

    over_grid(volume_fraction, [&](int i, int j, int k) {
        if (accessor.getValue({ i, j, k }) < 0) return;

        auto this_id = id_for_coord(bb, i, j, k);


        for (auto const& dir : directions) {
            auto other_coord = glm::ivec3(i, j, k) + dir;

            int64_t other_cell_id =
                id_for_coord(bb, other_coord.x, other_coord.y, other_coord.z);

            if (other_cell_id < 0) continue;

            bool other_is_in = is_vfrac_in(accessor.getValue(
                { other_coord.x, other_coord.y, other_coord.z }));

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
/// \brief We only support one component for now, so clean out all but the
/// largest component.
///
static void clean_components(SimpleGraph& G) {

    // which is the largest?

    auto components = G.components();

    if (components.empty()) {
        fatal("No components found! Broken component cleaner!");
    }

    fmt::print("Found {} components\n", components.size());

    std::vector<size_t> component_counts(components.size());

    for (auto [nid, cid] : components) {
        if (component_counts.size() <= cid) {
            component_counts.resize(cid + 1);
        }
        component_counts.at(cid)++;
    }

    auto iter =
        std::max_element(component_counts.begin(), component_counts.end());

    if (iter == component_counts.end()) {
        fatal("Broken component counter!");
    }

    size_t largest_component =
        std::labs(std::distance(component_counts.begin(), iter));

    // remove any node that is not part of this component

    fmt::print("Using component {} with {} nodes\n", largest_component, *iter);

    for (auto [nid, cid] : components) {
        if (cid != largest_component) G.remove_node(nid);
    }
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
static int64_t get_starting_node(SimpleGraph const&     G,
                                 SimpleTransform const& transform) {

    if (!global_configuration().root_around) {
        // pick node

        auto iter = std::min_element(G.nodes().begin(),
                                     G.nodes().end(),
                                     [](auto const& a, auto const& b) {
                                         return a.second.data.depth <
                                                b.second.data.depth;
                                     });

        assert(iter != G.nodes().end());

        return iter->first;
    }

    glm::vec3 point = transform(*(global_configuration().root_around));

    fmt::print("Root should be around: {} {} {}\n", point.x, point.y, point.z);


    auto iter = std::min_element(
        G.nodes().begin(),
        G.nodes().end(),
        [point](auto const& a, auto const& b) {
            return glm::distance2(a.second.data.position, point) <
                   glm::distance2(b.second.data.position, point);
        });

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
        fatal("Graph is not DAG!");
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
static void voxel_debug_dump(openvdb::FloatGrid::Ptr const& grid,
                             SimpleGraph const&             G) {
    std::ofstream stream(global_configuration().control_dir / "voxels.csv");

    stream << "x,y,z,depth,vfrac\n";

    auto bb = grid->evalActiveVoxelBoundingBox();

    auto accessor = grid->getConstAccessor();

    over_grid(*grid, [&](int i, int j, int k) {
        auto value = accessor.getValue({ i, j, k });

        if (!is_vfrac_in(value)) return;

        auto id = id_for_coord(bb, i, j, k);

        if (id < 0) return;

        stream << i << "," << j << "," << k << "," << G.node(id).depth << ","
               << value << "\n";
    });
}


SimpleGraph generate_vessels(openvdb::FloatGrid::Ptr const& volume_fraction,
                             SimpleTransform const&         transform) {

    SimpleGraph G;

    fmt::print("Building initial networks\n");

    build_initial_networks(*volume_fraction, transform, G);

    fmt::print("Graph has {} nodes\n", G.nodes().size());

    sanitize_distances(*volume_fraction, G, 10);

    if (global_configuration().dump_voxels) {
        voxel_debug_dump(volume_fraction, G);
    }

    fmt::print("Connecting nodes\n");
    connect_all_grad(*volume_fraction, G);

    // we may get multiple components. For now, just pick the largest one.
    fmt::print("Cleaning components\n");
    clean_components(G);

    fmt::print("Graph has {} edges. Compute MST\n", G.edge_count());
    auto mst = G.compute_min_spanning_tree();

    reposition(G);

    auto starting_node = get_starting_node(G, transform);

    fmt::print(
        "MST has {} edges. Build tree from {}\n", mst.size(), starting_node);

    auto tree = build_tree(mst, starting_node);

    fmt::print("Tree has {} nodes. Compute flow\n", tree.node_count());

    auto flow = compute_flow_size(tree);

    fmt::print("Flow complete, building final graph\n", tree.node_count());

    return build_final_graph(flow, tree, G);
}
