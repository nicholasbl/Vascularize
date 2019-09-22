#include "generate_vessels.h"
#include "global.h"
#include "mesh_write.h"
#include "voxelmesh.h"
#include "wavefrontimport.h"
#include "xrange.h"

#include <fmt/printf.h>

int main(int argc, char* argv[]) {

    if (!parse_arguments(argc, argv)) {
        return EXIT_FAILURE;
    }

    fmt::print("Loading mesh {}, dicing at {}\n",
               global_configuration().mesh_path,
               global_configuration().cube_size);

    auto imported_mesh = import_wavefront(global_configuration().mesh_path);

    fmt::print("Mesh imported, creating voxels...\n");

    auto [voxels, tf] = voxelize(std::move(imported_mesh.objects),
                                 global_configuration().cube_size);

    fmt::print("Finished voxel grid, building flow graph\n");

    auto flow_graph = generate_vessels(voxels, tf);

    auto out_path = global_configuration().output_path;

    fmt::print("Writing mesh to {}\n", out_path);

    write_mesh_to(flow_graph, tf, out_path);

    return 0;
}
