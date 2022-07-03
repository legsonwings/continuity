# continuity
 collection of templated n-d : bezier utilities, math utilities and algorithms
 
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
