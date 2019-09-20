#ifndef MESH_WRITE_H
#define MESH_WRITE_H

#include "simplegraph.h"

#include <filesystem>

struct SimpleTransform;

void write_mesh_to(SimpleGraph&                 G,
                   SimpleTransform const&       tf,
                   std::filesystem::path const& path);

#endif // MESH_WRITE_H
