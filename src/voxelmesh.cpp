#include "voxelmesh.h"

#include "jobcontroller.h"
#include "wavefrontimport.h"
#include "xrange.h"

#include <fmt/printf.h>

#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/intersect.hpp>

#include <fstream>
#include <random>

///@{
/// Random sources and distributions
static std::random_device                    random_device;
static std::mt19937                          random_generator(random_device());
static std::uniform_real_distribution<float> random_distribution_1_1(-1.0, 1.0);
///@}

///
/// \brief Generate a float, [-1, 1] to help build random directions
///
static float random_axis() { return random_distribution_1_1(random_generator); }

///
/// \brief Generate a random unit vector
///
static glm::vec3 random_dir() {
    return glm::normalize(
        glm::vec3(random_axis(), random_axis(), random_axis()) * 2.0f - 1.0f);
}

///
/// \brief Ask if a point is within a mesh
///
static bool is_point_in_object(MutableObject const& o, glm::vec3 const& p) {
    glm::vec3 direction = random_dir();

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

            isect_count += static_cast<float>(isects) * dist > 0;
        }
    }

    bool is_even = isect_count % 2 == 0;

    return !is_even;
}

///
/// \brief Check every point in our grid to see if it is inside the mesh
///
void grid_fill(MutableObject const& object, Grid3D<bool>& volume) {
    fmt::print("Starting grid fill\n");

    static const glm::vec3 offset(.5);

    JobController controller;

    for (size_t i : xrange(volume.size_x())) {
        for (size_t j : xrange(volume.size_y())) {
            controller.add_job([&volume, &object, i, j]() {
                for (size_t k : xrange(volume.size_z())) {
                    auto cube_point = glm::vec3(i, j, k) + offset;

                    volume(i, j, k) = is_point_in_object(object, cube_point);
                }
            });
        }
    }
}

static SimpleTransform make_transform(glm::vec3 voxel_grid_resolution,
                                      glm::vec3 mesh_volume_size,
                                      glm::vec3 bounding_box_minimum) {

    glm::vec3 voxel_grid_res_sub1 = voxel_grid_resolution - glm::vec3(1);

    glm::vec3 scale = voxel_grid_res_sub1 / mesh_volume_size;
    glm::vec3 translate =
        -(voxel_grid_res_sub1 * bounding_box_minimum) / mesh_volume_size;

    return SimpleTransform(scale, translate);
}


VoxelResult voxelize(std::vector<MutableObject>&& objects, double voxel_size) {

    BoundingBox total_bb;

    for (auto const& o : objects) {
        for (auto const& m : o.meshes) {
            total_bb = total_bb.united(m.bounds());
        }
    }

    auto mesh_volume_size = total_bb.size();

    fmt::print("Mesh bounds {} - {}\n", total_bb.minimum(), total_bb.maximum());

    auto voxel_grid_resolution =
        glm::max(glm::ceil(mesh_volume_size / static_cast<float>(voxel_size)),
                 glm::vec3(1)) +
        glm::vec3(1);

    fmt::print("Voxel resolution {}\n", voxel_grid_resolution);

    Grid3D<bool> ret(voxel_grid_resolution.x,
                     voxel_grid_resolution.y,
                     voxel_grid_resolution.z);

    SimpleTransform tf = make_transform(
        voxel_grid_resolution, mesh_volume_size, total_bb.minimum());


    fmt::print("Mesh to grid transform: scale {}, translate {}\n",
               tf.scale(),
               tf.translate());

    // transform mesh into grid coordinates
    for (auto& o : objects) {
        for (auto& mesh : o.meshes) {
            for (auto& v : mesh.vertex()) {
                v.position = tf(v.position);
            }
        }
    }

    fmt::print("Starting object voxelization\n");

    for (auto const& o : objects) {
        grid_fill(o, ret);
    }

    return { std::move(ret), tf };
}
