#ifndef MESH_WRITE_H
#define MESH_WRITE_H

#include "simplegraph.h"

#include <filesystem>

class SimpleTransform;

///
/// \brief Create a mesh from a flow graph and write it to a path
/// \param G Flow graph
/// \param tf Transform from mesh to grid space
/// \param path Mesh output file
///
void write_mesh_to(SimpleGraph&                 G,
                   SimpleTransform const&       tf,
                   std::filesystem::path const& path);

#endif // MESH_WRITE_H
