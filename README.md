# Vascularize
Turn a volume into vessels

## Building

Requirements:

| Supported Compiler | Version >= |
| --------:| :---:| 
| GCC | 8.1 |
| Clang | 7 |
| Apple Xcode | 11 |

| Required Libraries / Packages  | Version >= |
| -------- | ---- |
| [glm](https://glm.g-truc.net/)      | 0.9.8 |
| [fmtlib](https://fmt.dev/latest/index.html)   | 5.3.0 |
| [OpenVDB](https://www.openvdb.org/) | 6.1.0 |
| qmake<sup>1</sup> | - |

<sup>1</sup>A full Qt install not required, only qmake.

To build, create a `build` directory somewhere, `cd` into it, and run `qmake <path-to-vasc.pro>`. You should then be able to run `make`.

## Running

As an example, look at the `bunny` case directory. Inside is a source wavefront object and a control file. 

The control file has the following options:

| Option | Description |
| --- | --- | 
| `mesh` | Path to the wavefront object to consume. |
| `voxel_size` |  Size of voxels in mesh coordinate space. |
| `output` | Name of the output vascular mesh. |
| `position_randomness` | Vessel position randomness. |
| `prune` | Number of rounds of vessel leaves to prune. |
| `prune_flow` | Vessel sizes less than this value will be pruned. |
| `dump_voxels` | Write voxels to case directory, will appear as a csv. |

To start the run, pass the control file as the only argument to the `vascularize` executable.

## Process

To build the vascularized structure, the following procedure is used.

### Voxellization
Input mesh is read in, and a voxel grid is created with a resolution based on `voxel_size`. 

Each voxel is then given a value based on the inverse distance to the edge of the mesh, i.e. edge voxels will have a high value, center voxels will have a low value. Randomness is added.

### Flow

A flow graph is created where high valued voxels are connected to low value voxels. A minimum spanning tree is built from this graph; weights for edges are the delta in voxel value. A root is selected (at this time, just picks the lowest voxel), and flow is computed from that point. Flow, in this case, is just the count of all downstream nodes.

### Meshing

The graph is then turned into a mesh. We first prune parts of the graph, leaves of the graph, and areas where the flow is too small. At this point the nodes of the graph have a position that is regularized on a grid, so positions are perturbed by randomness, and then the positions are relaxed to smooth out hard edges. Each edge is turned into a tube (poorly), and emitted.

