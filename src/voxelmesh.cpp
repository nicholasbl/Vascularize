#include "voxelmesh.h"

#include "jobcontroller.h"
#include "wavefrontimport.h"
#include "xrange.h"

#include <fmt/printf.h>
#include <glm/gtx/intersect.hpp>

// FOR DEBUGGING
#include <fstream>

bool is_point_in_object(MutableObject const& o, glm::vec3 const& p) {
    glm::vec3 direction(1, 0, 0);

    size_t isect_count = 0;

    for (auto const& mesh : o.meshes) {
        for (auto const& face : mesh.faces()) {
            auto a = mesh.vertex()[face.indicies[0]].position;
            auto b = mesh.vertex()[face.indicies[1]].position;
            auto c = mesh.vertex()[face.indicies[2]].position;

            glm::vec2 bary;
            float     dist;

            bool isects =
                glm::intersectRayTriangle(p, direction, a, c, b, bary, dist);

            isect_count += isects * dist > 0;
        }
    }

    bool is_even = isect_count % 2 == 0;
    return !is_even;
}


void flood_fill(MutableObject const& object, Grid3D<bool>& volume) {

    JobController controller;

    for (size_t i : xrange(volume.size_x())) {
        for (size_t j : xrange(volume.size_y())) {
            for (size_t k : xrange(volume.size_z())) {
                controller.add_job([&volume, &object, i, j, k]() {
                    auto cube_point = glm::vec3(i, j, k) + glm::vec3(.5);

                    volume(i, j, k) = is_point_in_object(object, cube_point);
                });
            }
        }

        fmt::print("\033[A\33[2KT\r{}% complete...\n",
                   int((i / float(volume.size_x() - 1)) * 100));
    }
}

void dump_mesh(std::vector<MutableObject> const& objects) {
    std::ofstream stream("debug.obj");

    for (auto& o : objects) {
        for (auto& mesh : o.meshes) {

            for (auto& v : mesh.vertex()) {
                stream << "v " << v.position.x << " " << v.position.y << " "
                       << v.position.z << std::endl;
            }

            for (auto& face : mesh.faces()) {
                stream << "f " << face.indicies[0] + 1 << " "
                       << face.indicies[1] + 1 << " " << face.indicies[2] + 1
                       << std::endl;
            }
        }
    }
}

VoxelResult voxelize(std::vector<MutableObject>&& objects, float voxel_size) {

    BoundingBox total_bb;

    for (auto const& o : objects) {
        for (auto const& m : o.meshes) {
            total_bb = total_bb.united(m.bounds());
        }
    }

    auto mesh_volume_size = total_bb.size();

    fmt::print("Mesh bounds {} - {}\n", total_bb.minimum(), total_bb.maximum());

    auto voxel_grid_resolution =
        glm::max(glm::ceil(mesh_volume_size / voxel_size), glm::vec3(1)) +
        glm::vec3(1);

    fmt::print("Voxel resolution {}\n", voxel_grid_resolution);

    Grid3D<bool> ret(voxel_grid_resolution.x,
                     voxel_grid_resolution.y,
                     voxel_grid_resolution.z);

    SimpleTransform tf;
    tf.scale = (voxel_grid_resolution - glm::vec3(1)) / mesh_volume_size;
    tf.translate =
        -((voxel_grid_resolution - glm::vec3(1)) * total_bb.minimum()) /
        mesh_volume_size;

    fmt::print("Mesh to grid transform: scale {}, translate {}\n",
               tf.scale,
               tf.translate);

    // transform mesh into grid coordinates
    for (auto& o : objects) {
        for (auto& mesh : o.meshes) {
            for (auto& v : mesh.vertex()) {
                v.position = tf(v.position);
            }
        }
    }

    // dump_mesh(objects);

    fmt::print("Starting object voxelization\n");

    for (auto const& o : objects) {
        flood_fill(o, ret);
    }

    return { std::move(ret), tf };
}
