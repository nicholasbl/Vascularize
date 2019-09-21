#ifndef GENERATE_VESSELS_H
#define GENERATE_VESSELS_H

#include "grid.h"
#include "simplegraph.h"


///
/// \brief Generate a vessel flow graph
/// \param volume_fraction Voxel grid representing volume fraction
///
SimpleGraph generate_vessels(Grid3D<bool> const& volume_fraction);


#endif // GENERATE_VESSELS_H
