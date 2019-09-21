#include "mutable_mesh.h"

#include "xrange.h"

#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include <map>

void MutableMesh::add(mesh_detail::Vertex const& v) {
    m_verticies.push_back(v);
}

void MutableMesh::add(mesh_detail::Face const& f) { m_faces.push_back(f); }

void MutableMesh::merge_in(MutableMesh const& mesh) {
    auto start_pos = m_verticies.size();

    m_verticies.insert(
        m_verticies.end(), mesh.m_verticies.begin(), mesh.m_verticies.end());

    for (mesh_detail::Face f : mesh.faces()) { // copy face
        for (int i : xrange(3)) {
            f.indicies[i] += start_pos;
        }
        m_faces.push_back(f);
    }
}

void MutableMesh::transform(glm::mat4 const& m) {
    for (mesh_detail::Vertex& vert : m_verticies) {
        glm::vec4 hp  = glm::vec4(vert.position, 1);
        glm::vec4 v   = m * hp;
        vert.position = glm::vec3(v) / v.w;
    }
}

BoundingBox MutableMesh::bounds() const {
    if (m_verticies.empty()) return BoundingBox();

    glm::vec3 lower = m_verticies[0].position;
    glm::vec3 upper = m_verticies[0].position;

    // this looks stupid
    for (size_t i : xrange(1ul, m_verticies.size())) {
        mesh_detail::Vertex const& v = m_verticies[i];

        auto const& pos = v.position;
        lower[0]        = std::min(pos.x, lower[0]);
        upper[0]        = std::max(pos.x, upper[0]);

        lower[1] = std::min(pos.y, lower[1]);
        upper[1] = std::max(pos.y, upper[1]);

        lower[2] = std::min(pos.z, lower[2]);
        upper[2] = std::max(pos.z, upper[2]);
    }

    return BoundingBox(glm::vec3(lower[0], lower[1], lower[2]),
                       glm::vec3(upper[0], upper[1], upper[2]));
}
