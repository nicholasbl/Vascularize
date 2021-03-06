#include "simplegraph.h"

#include "global.h"

#include <fmt/printf.h>

#include <cassert>
#include <optional>
#include <unordered_set>

SimpleGraph::SimpleGraph() = default;

SimpleGraph::~SimpleGraph() = default;

int64_t SimpleGraph::add_node(int64_t id, NodeData nd) {
    m_nodes.try_emplace(id, nd);
    return id;
}

void SimpleGraph::add_edge(int64_t a, int64_t b, EdgeData data) {
    auto aiter = m_nodes.find(a);
    auto biter = m_nodes.find(b);

    assert(aiter != m_nodes.end());
    assert(biter != m_nodes.end());

    if (aiter->second.edges.count(b)) return;

    auto ptr = std::make_shared<Edge>(a, b, data);

    aiter->second.edges.try_emplace(b, ptr);
    biter->second.edges.try_emplace(a, ptr);

    m_edges.insert(ptr);
}

bool SimpleGraph::has_edge(int64_t a, int64_t b) const {
    return m_nodes.at(a).edges.count(b) > 0;
}

void SimpleGraph::remove_node(int64_t i) {
    // unlink others from our node
    for (auto const& [otherid, edata] : edge(i)) {
        m_nodes.at(otherid).edges.erase(i);

        m_edges.erase(edata);
    }

    m_nodes.erase(i);
}

NodeData const& SimpleGraph::node(int64_t i) const {
    try {
        return m_nodes.at(i).data;
    } catch (...) {
        fatal("Missing node in graph!");
    }
}

NodeData& SimpleGraph::node(int64_t i) {
    try {
        return m_nodes.at(i).data;
    } catch (...) {
        fatal("Missing node in graph!");
    }
}

std::unordered_map<int64_t, Node::Ptr> const&
SimpleGraph::edge(int64_t i) const {
    try {
        return m_nodes.at(i).edges;
    } catch (...) {
        fatal("Cannot get edge; missing node in graph!");
    }
}

struct UnionFind {
    std::unordered_map<int64_t, int64_t> parents;
    std::unordered_map<int64_t, float>   weights;

    int64_t operator[](int64_t object) {
        if (parents.count(object) == 0) {
            parents[object] = object;
            weights[object] = 1;
            return object;
        }

        std::vector<int64_t> path;

        path.push_back(object);

        auto root = parents.at(object);

        while (root != path.back()) {
            path.push_back(root);
            root = parents[root];
        }

        for (auto ancestor : path) {
            parents[ancestor] = root;
        }

        return root;
    }

    void unioned(int64_t a, int64_t b) {
        std::array<int64_t, 2> roots = { operator[](a), operator[](b) };

        auto heaviest_iter = std::max_element(
            roots.begin(), roots.end(), [this](auto a, auto b) {
                return weights[a] < weights[b];
            });

        auto heaviest = *heaviest_iter;

        for (auto r : roots) {
            if (r == heaviest) continue;

            weights[heaviest] += weights[r];
            parents[r] = heaviest;
        }
    }
};

std::vector<EdgeKey> SimpleGraph::compute_min_spanning_tree() const {
    std::vector<EdgeKey> ret;

    std::vector<Edge> edge_list;

    for (auto const& e : m_edges) {
        Edge ne;
        ne.a           = e->a;
        ne.b           = e->b;
        ne.data.weight = e->data.weight;

        edge_list.push_back(ne);
    }

    std::sort(
        edge_list.begin(), edge_list.end(), [](auto const& a, auto const& b) {
            return a.data.weight < b.data.weight;
        });


    UnionFind uf;

    for (auto const& e : edge_list) {
        if (uf[e.a] != uf[e.b]) {
            ret.emplace_back(e.a, e.b);
            uf.unioned(e.a, e.b);
        }
    }

    // for nodes a - b - c, edge count 2, node count 3

    if ((ret.size() + 1) != nodes().size()) {
        fmt::print("Mismatch size {} {}\n", ret.size(), nodes().size());
        fatal("Unable to continue");
    }

    return ret;
}

size_t SimpleGraph::edge_count() const { return m_edges.size(); }

static void color(SimpleGraph const&                   G,
                  std::unordered_map<int64_t, size_t>& colors,
                  int64_t                              starting_node,
                  size_t                               color) {

    std::vector<int64_t> stack;

    stack.push_back(starting_node);

    while (!stack.empty()) {
        auto node = stack.back();
        stack.pop_back();

        if (colors.count(node)) continue;

        colors[node] = color;

        for (auto const& [other_id, edata] : G.edge(node)) {
            stack.push_back(other_id);
        }
    }
}

std::unordered_map<int64_t, size_t> SimpleGraph::components() const {
    size_t                              marker = 0;
    std::unordered_map<int64_t, size_t> colors;

    for (auto const& [nid, ndata] : m_nodes) {
        if (colors.count(nid)) continue;

        color(*this, colors, nid, marker);

        marker++;
    }

    return colors;
}


SimpleTree::SimpleTree(int64_t root) : m_root(root) {}

std::unordered_set<int64_t> const&
SimpleTree::get_children_of(int64_t i) const {
    assert(has_node(i) && "Cannot get children of node that doesnt exist");
    return m_nodes.at(i).out_ids;
}

size_t recursive_size_check(SimpleTree const& t, int64_t a) {
    size_t i = 1;

    for (auto oid : t.get_children_of(a)) {
        i += recursive_size_check(t, oid);
    }

    return i;
}

void SimpleTree::add_edge(int64_t a, int64_t b) {
    // we shouldn't be adding the same edge twice
    assert(m_nodes[a].out_ids.count(b) == 0);

    m_nodes[a].out_ids.insert(b);
    m_nodes.try_emplace(b, TNode{});
}


size_t SimpleTree::node_count() const { return m_nodes.size(); }

bool SimpleTree::validate_tree() {
    std::unordered_set<int64_t> visited;

    auto first = root();

    assert(this->has_node(first));


    std::function<bool(int64_t)> recursive_visit = [&](int64_t node) {
        if (!this->has_node(node)) {
            fmt::print("Missing node {}\n", node);
            assert(0);
        }

        if (visited.count(node)) return false;

        visited.insert(node);


        for (auto id : this->get_children_of(node)) {
            if (!recursive_visit(id)) return false;
        }

        return true;
    };

    if (!recursive_visit(first)) return false;

    for (auto const& n : this->nodes()) {
        if (!visited.count(n.first)) return false;
    }

    return true;
}
