#ifndef GENERATE_VESSELS_H
#define GENERATE_VESSELS_H

#include "grid.h"
#include "simplegraph.h"

SimpleGraph generate_vessels(Grid3D<bool> const& volume_fraction);


#endif // GENERATE_VESSELS_H
