#ifndef GLOBAL_H
#define GLOBAL_H

#include <filesystem>

struct Configuration {
    std::filesystem::path mesh_path;     ///< Path to input mesh
    double                cube_size = 1; ///< voxel size
    std::filesystem::path output_path;   ///< Output mesh path

    float position_randomness = .5; ///< Random perturbation to node points

    int   prune_rounds = 3; ///< Rounds of pruning to execute
    float prune_flow   = 0; ///< Flow size <= we prune

    bool dump_voxels = false; ///< Dump voxels for debugging
};

///
/// \brief Parse program arguments, fill in global config
///
bool parse_arguments(int argc, char* argv[]);

///
/// \brief Get global configuration
///
Configuration const& global_configuration();

#endif // GLOBAL_H
