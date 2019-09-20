#ifndef SIMPLEGRAPH_H
#define SIMPLEGRAPH_H

#include "glm_include.h"

#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct Edge;

struct NodeData {
    glm::vec3 position;

    float depth = 0;
    float flow  = 0;
};

struct Node {
    using Ptr = std::shared_ptr<Edge>;

    std::unordered_map<int64_t, Ptr> edges;

    NodeData data;

    Node() = default;
    Node(NodeData d) : data(d) {}
};


struct EdgeData {
    float weight = 0;
};


struct Edge {
    int64_t a;
    int64_t b;

    EdgeData data;

    Edge() = default;
    Edge(int64_t _a, int64_t _b, EdgeData _data) : a(_a), b(_b), data(_data) {}
};

struct EdgeKey {
    int64_t a;
    int64_t b;

    EdgeKey() = default;
    EdgeKey(int64_t _a, int64_t _b) : a(_a), b(_b) {}

    bool operator==(EdgeKey const& other) const {
        return std::tie(a, b) == std::tie(other.a, other.b);
    }
};

template <class T>
inline void hash_combine(std::size_t& seed, const T& v) {
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std {

template <>
struct hash<EdgeKey> {
    std::size_t operator()(EdgeKey const& k) const {
        using std::hash;
        using std::size_t;
        using std::string;

        size_t seed = 0;
        hash_combine(seed, k.a);
        hash_combine(seed, k.b);

        return seed;
    }
};

} // namespace std

class SimpleGraph {
    std::unordered_map<int64_t, Node>         m_nodes;
    std::unordered_set<std::shared_ptr<Edge>> m_edges;

public:
    SimpleGraph();
    ~SimpleGraph();

    int64_t add_node(int64_t, NodeData);
    void    add_edge(int64_t, int64_t, EdgeData);

    void remove_node(int64_t);

    NodeData const& node(int64_t) const;
    NodeData&       node(int64_t);

    bool has_node(int64_t n) const { return m_nodes.count(n) > 0; }
    bool has_edge(int64_t a, int64_t b) const;

    std::unordered_map<int64_t, Node::Ptr> const& edge(int64_t) const;

    auto&       nodes() { return m_nodes; }
    auto const& nodes() const { return m_nodes; }

    auto&                edges() { return m_edges; }
    auto const&          edges() const { return m_edges; }
    std::vector<EdgeKey> compute_min_spanning_tree() const;

    size_t edge_count() const;

    size_t component_count() const;
};

class SimpleTree {
    struct TNode {
        int64_t id;

        std::unordered_set<int64_t> out_ids;
    };

    int64_t                            m_root;
    std::unordered_map<int64_t, TNode> m_nodes;

public:
    SimpleTree();

    void    set_root(int64_t i) { m_root = i; }
    int64_t root() const { return m_root; }

    std::unordered_set<int64_t> const& get_children_of(int64_t) const;

    void add_edge(int64_t, int64_t);

    bool has_node(int64_t n) const { return m_nodes.count(n) > 0; }

    auto const& nodes() const { return m_nodes; }

    auto begin() const { return m_nodes.begin(); }
    auto end() const { return m_nodes.end(); }

    size_t node_count() const;
};

bool validate_tree(SimpleTree const& t);


#endif // SIMPLEGRAPH_H
