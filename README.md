# continuity
toy renderer
 
## beziermaths

Bezier maths is a min-library containing useful bezier evaluation and other algorithms
planarbezier is a generic bezier construct that can be used to create bezier constructs of arbitrary dimension and degree
Bezier maths also has bezier surface defined on **triangular domain** and legacy structures for planar curve, surfaces and volume, but these are to be replaced by planarbezier.

## stdx

**stdx** has a vector mini library for vectors of arbitrary dimension and generic functions/algorithms

**join**

Join is a class that allows iteration of a sequence of containers storing heterogenous types using a base pointer. This is handy with range based loops because of its much nicer syntax to avoid code duplication.

**triidx and grididx**

triidx represents a index into a triangular array and grididx represents index into a n-d array(square). Grid index can represent arbitrary dimensions and degree.

**ext**

ext allows extension of objects. It stores two objects of specified types. The base object can be used/accessed more naturally than with a std::pair, which makes this a convenient option in some cases.

**nlerp**

nlerp is multivariate interpolation(in arbitrary dimensions). It generalizes linear, bilinear, trilinear, ... etc interploations


## configuration:

**Linking with stdx**:
Upon building stdx *stdx64.lib* will be exported to *continuity/libs* and *stxcore.ixxx.ifc*, *stxcore.ixxx.ifc* and *vec.ixxx.ifc* will be copied to *continuity/metadata*
Note: Both debug and release libs are named stdx64, so enusre the built library was built with correct config.

To link use following add the follwing configs:

<AdditionalDependencies>d3d12.lib;dxgi.lib;dxguid.lib;stdxx64.lib;%(AdditionalDependencies)</AdditionalDependencies>
<AdditionalModuleDependencies>$(SolutionDir)..\metadata\stdxcore.ixx.ifc;$(SolutionDir)..\metadata\vec.ixx.ifc;$(SolutionDir)..\metadata\stdx.ixx.ifc</AdditionalModuleDependencies>
<AdditionalLibraryDirectories>$(SolutionDir)\..\libs;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
