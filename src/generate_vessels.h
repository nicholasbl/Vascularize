#ifndef GENERATE_VESSELS_H
#define GENERATE_VESSELS_H

#include "simplegraph.h"

#include <openvdb/openvdb.h>

class SimpleTransform;

///
/// \brief Generate a vessel flow graph
/// \param volume_fraction Voxel grid representing volume fraction
/// \param transform Mesh to grid transform
///
SimpleGraph generate_vessels(openvdb::FloatGrid::Ptr const& volume_fraction,
                             SimpleTransform const&         transform);


#endif // GENERATE_VESSELS_H
