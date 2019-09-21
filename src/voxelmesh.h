#ifndef VOXELMESH_H
#define VOXELMESH_H

#include "grid.h"
#include "mutable_mesh.h"

struct MutableObject;

///
/// \brief The SimpleTransform class models models just scale and translation of
/// an affine transform
///
/// Instead of using a matrix, we just use a simple component-wise deal for
/// performance reasons. Scale goes first, then translation.
///
class SimpleTransform {
    glm::vec3 m_scale;
    glm::vec3 m_translate;

public:
    SimpleTransform(glm::vec3 scale, glm::vec3 translate)
        : m_scale(scale), m_translate(translate) {}

    [[nodiscard]] glm::vec3 operator()(glm::vec3 v) const {
        return (v * m_scale) + m_translate;
    }

    [[nodiscard]] glm::vec3 inverted(glm::vec3 v) const {
        return (v - m_translate) / m_scale;
    }

    glm::vec3 scale() const { return m_scale; }
    glm::vec3 translate() const { return m_translate; }
};

struct VoxelResult {
    Grid3D<bool>    voxels;
    SimpleTransform tf;
};

///
/// \brief Voxelize a given mesh using a given size of voxel in mesh space.
///
/// Note that we consume the given mesh, so that we can do transforms inplace.
///
VoxelResult voxelize(std::vector<MutableObject>&&, double voxel_size);


#endif // VOXELMESH_H
