#include "voxelmesh.h"

#include "jobcontroller.h"
#include "wavefrontimport.h"
#include "xrange.h"

#include <fmt/printf.h>

#include <openvdb/openvdb.h>
#include <openvdb/tools/Composite.h>
#include <openvdb/tools/MeshToVolume.h>

#include <fstream>

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

    auto volume_fraction =
        openvdb::FloatGrid::create(std::numeric_limits<int>::lowest());

    {
        openvdb::CoordBBox cbb(0,
                               0,
                               0,
                               voxel_grid_resolution.x,
                               voxel_grid_resolution.y,
                               voxel_grid_resolution.z);
        volume_fraction->sparseFill(cbb, 0.0F);
    }


    for (auto const& o : objects) {
        for (auto const& m : o.meshes) {
            auto ptr = openvdb::tools::meshToVolume<openvdb::FloatGrid>(
                m, {}, 1.0f, 1.0f);

            openvdb::tools::compMax(*volume_fraction, *ptr);
        }
    }


    openvdb::tools::foreach (volume_fraction->beginValueOn(),
                             [](auto const& iter) {
                                 float value = *iter;

                                 iter.setValue(value < .5 ? 1.0F : -1.0F);
                             });


    openvdb::tools::prune(volume_fraction->tree());

    {
        auto bb = volume_fraction->evalActiveVoxelBoundingBox();
        fmt::print("Volume fraction computed: {} {} {} x {} {} {}\n",
                   bb.min().x(),
                   bb.min().y(),
                   bb.min().z(),
                   bb.max().x(),
                   bb.max().y(),
                   bb.max().z());

        fmt::print("Volume fraction bytes {}\n", volume_fraction->memUsage());
    }


    return { volume_fraction, tf };
}
