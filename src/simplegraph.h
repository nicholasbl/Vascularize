#ifndef SIMPLEGRAPH_H
#define SIMPLEGRAPH_H

#include "glm_include.h"

#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct Edge;

///
/// \brief The NodeData struct holds user data for a node
///
struct NodeData {
    glm::vec3 position;

    float depth = 0;
    float flow  = 0;
};

///
/// \brief The Node struct holds data and topology information for a node
///
struct Node {
    using Ptr = std::shared_ptr<Edge>;

    std::unordered_map<int64_t, Ptr> edges;

    NodeData data;

    Node() = default;
    Node(NodeData d) : data(d) {}
};


///
/// \brief The EdgeData struct holds user data for an edge
///
struct EdgeData {
    float weight = 0;
};


///
/// \brief The EdgeKey struct models the related nodes for an edge
///
struct EdgeKey {
    int64_t a = -1;
    int64_t b = -1;

    EdgeKey() = default;
    EdgeKey(int64_t _a, int64_t _b) : a(_a), b(_b) {}

    bool operator==(EdgeKey const& other) const {
        return std::tie(a, b) == std::tie(other.a, other.b);
    }
};

///
/// \brief The Edge struct represents an edge in the graph
///
struct Edge : EdgeKey {
    EdgeData data;

    Edge() = default;
    Edge(int64_t _a, int64_t _b, EdgeData _data)
        : EdgeKey(_a, _b), data(_data) {}
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

///
/// \brief The SimpleGraph class is a super simple undirected graph
///
class SimpleGraph {
    using EdgeMap = std::unordered_map<int64_t, Node::Ptr>;
    std::unordered_map<int64_t, Node>         m_nodes;
    std::unordered_set<std::shared_ptr<Edge>> m_edges;

public:
    SimpleGraph();
    ~SimpleGraph();

    ///
    /// \brief Add a node to the graph
    ///
    int64_t add_node(int64_t, NodeData);

    ///
    /// \brief Add an edge to the graph
    ///
    void add_edge(int64_t, int64_t, EdgeData);

    ///
    /// \brief Unlink and delete a node from the graph
    ///
    void remove_node(int64_t);

    ///@{
    /// Get the user data for a given node. Throws if node is missing.
    [[nodiscard]] NodeData const& node(int64_t) const;
    [[nodiscard]] NodeData&       node(int64_t);
    ///@}

    /// \brief Ask if the graph has a node
    [[nodiscard]] bool has_node(int64_t n) const {
        return m_nodes.count(n) > 0;
    }

    /// \brief Ask if two nodes are connected. Throws if nodes are missing.
    [[nodiscard]] bool has_edge(int64_t a, int64_t b) const;

    /// \brief Get all edges that involve a given node. Throws if node is
    /// missing.
    [[nodiscard]] EdgeMap const& edge(int64_t) const;

    ///@{
    /// Get all nodes in the graph as a map.
    [[nodiscard]] auto&       nodes() { return m_nodes; }
    [[nodiscard]] auto const& nodes() const { return m_nodes; }
    ///@}

    ///@{
    /// Get all edges in the graph as a set.
    [[nodiscard]] auto&       edges() { return m_edges; }
    [[nodiscard]] auto const& edges() const { return m_edges; }
    ///@}

    /// \brief Compute a minimum spanning tree, returns an edge list
    [[nodiscard]] std::vector<EdgeKey> compute_min_spanning_tree() const;

    /// \brief Get the number of edges
    [[nodiscard]] size_t edge_count() const;

    /// \brief Get connected components
    /// \return A map from node to component number
    [[nodiscard]] std::unordered_map<int64_t, size_t> components() const;
};

///
/// \brief The SimpleTree class models a tree.
///
/// We use a node list approach so we can add edges at random
///
class SimpleTree {
    using EdgeSet = std::unordered_set<int64_t>;

    ///
    /// \brief The TNode struct is a node in a tree
    ///
    struct TNode {
        int64_t id;

        EdgeSet out_ids;
    };

    using NodeMap = std::unordered_map<int64_t, TNode>;

    /// The root of the tree
    int64_t m_root = -1;
    NodeMap m_nodes;

public:
    ///
    /// \brief Create a new tree with a given node as the root
    ///
    SimpleTree(int64_t root);

    [[nodiscard]] int64_t root() const { return m_root; }

    /// \brief Get the downstream nodes of a node
    [[nodiscard]] EdgeSet const& get_children_of(int64_t) const;

    ///
    /// \brief Add a directed edge from one node to another
    ///
    /// Will create nodes as needed. Will explode if you add the same edge
    /// twice.
    ///
    void add_edge(int64_t, int64_t);

    /// \brief Ask if a node exists in the tree
    bool has_node(int64_t n) const { return m_nodes.count(n) > 0; }

    /// \brief Get all nodes as a map
    [[nodiscard]] auto const& nodes() const { return m_nodes; }

    [[nodiscard]] auto begin() const { return m_nodes.begin(); }
    [[nodiscard]] auto end() const { return m_nodes.end(); }

    /// \brief Get the count of nodes in the tree
    [[nodiscard]] size_t node_count() const;

    /// \brief Ensure the tree is consistent, due to our choice of data
    /// structure.
    bool validate_tree();
};


#endif // SIMPLEGRAPH_H
