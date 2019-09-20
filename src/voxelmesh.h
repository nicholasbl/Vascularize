#ifndef VOXELMESH_H
#define VOXELMESH_H

#include "grid.h"
#include "mutable_mesh.h"

struct MutableObject;

struct SimpleTransform {
    glm::vec3 scale;
    glm::vec3 translate;

    glm::vec3 operator()(glm::vec3 v) const { return (v * scale) + translate; }

    glm::vec3 inverted(glm::vec3 v) const { return (v - translate) / scale; }
};

struct VoxelResult {
    Grid3D<bool>    voxels;
    SimpleTransform tf;
};

VoxelResult voxelize(std::vector<MutableObject>&&, float voxel_size);


#endif // VOXELMESH_H
