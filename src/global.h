#ifndef GLOBAL_H
#define GLOBAL_H

#include <filesystem>

struct Configuration {
    std::filesystem::path mesh_path;
    double                cube_size = 1;
    std::filesystem::path output_path;

    int   prune_rounds = 3;
    float prune_flow   = 0;
};

bool parse_arguments(int argc, char* argv[]);

Configuration const& global_configuration();

#endif // GLOBAL_H
