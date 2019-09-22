#ifndef GENERATE_VESSELS_H
#define GENERATE_VESSELS_H

#include "grid.h"
#include "simplegraph.h"

class SimpleTransform;

///
/// \brief Generate a vessel flow graph
/// \param volume_fraction Voxel grid representing volume fraction
/// \param transform Mesh to grid transform
///
SimpleGraph generate_vessels(Grid3D<bool> const&    volume_fraction,
                             SimpleTransform const& transform);


#endif // GENERATE_VESSELS_H
