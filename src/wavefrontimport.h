#ifndef WAVEFRONTIMPORT_H
#define WAVEFRONTIMPORT_H

#include "mutable_mesh.h"

#include "glm_include.h"

#include <filesystem>
#include <vector>


struct MutableObject {
    std::string              name;
    std::vector<MutableMesh> meshes;
};


struct ImportedMesh {
    std::vector<MutableObject> objects;
};

///
/// \brief Read in a wavefront object from disk.
///
ImportedMesh import_wavefront(std::filesystem::path const&);

#endif // WAVEFRONTIMPORT_H
