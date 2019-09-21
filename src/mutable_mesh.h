#ifndef MUTMESH_H
#define MUTMESH_H

#include "boundingbox.h"
#include "glm_include.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mesh_detail {

struct Vertex {
    glm::vec3 position;
};

struct Face {
    uint32_t indicies[3];
};

} // namespace mesh_detail

/*!
 * \brief The MutableMesh class encapsulates a baked cpu-side mesh.
 */
class MutableMesh {
    std::vector<mesh_detail::Vertex> m_verticies;
    std::vector<mesh_detail::Face>   m_faces;

public:
    /*!
     * \brief Construct an empty mesh.
     */
    MutableMesh() = default;

    ///
    /// \brief Construct a mesh with given vertex and face list
    ///
    MutableMesh(std::vector<mesh_detail::Vertex>&&,
                std::vector<mesh_detail::Face>&&);

    MutableMesh(MutableMesh const&) = delete;
    MutableMesh& operator=(MutableMesh const&) = delete;

    MutableMesh(MutableMesh&&) = default;
    MutableMesh& operator=(MutableMesh&&) = default;

    auto const& vertex() const { return m_verticies; }
    auto const& faces() const { return m_faces; }

    auto& vertex() { return m_verticies; }

    /*!
     * \brief Compute the bounding box of this mesh.
     *
     * This is not cached, so calling bounds() multiple times is performance
     * negative.
     */
    BoundingBox bounds() const;

public:
    void add(mesh_detail::Vertex const&);
    void add(mesh_detail::Face const&);

    /*!
     * \brief Duplicate a source mesh into this current mesh.
     *
     * No vertex or face duplication checks are made.
     */
    void merge_in(MutableMesh const&);

    /*!
     * \brief Transform all the vertex positions (normals are subject to
     * rotation)
     */
    void transform(glm::mat4 const&);
};

#endif // MUTMESH_H
