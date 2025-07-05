module;

#include "simplemath/simplemath.h"

module sphintro;

import stdx;
import vec;
import std;

namespace sample_creator
{

template <>
std::unique_ptr<sample_base> create_instance<samples::sphintro>(view_data const& data)
{
	return std::move(std::make_unique<sphfluidintro>(data));
}

}

std::random_device rd;
static std::mt19937 re(rd());

using namespace DirectX;

typedef struct {
    vector3 p[3];
    vector3 n[3];
} mctriangle;

typedef struct {
    vector3 p[8];
    vector3 n[8];
    float val[8];
} mcgridcell;

//Linearly interpolate the position where an isosurface cuts
//an edge between two vertices, each with their own scalar value
std::pair<vector3, vector3> VertexInterp(float isolevel, vector3 p1, vector3 p2, vector3 n1, vector3 n2, float valp1, float valp2)
{
    float mu;
    vector3 p, n;

    if (std::abs(isolevel - valp1) < 0.00001)
        return { p1, n1 };
    if (std::abs(isolevel - valp2) < 0.00001)
        return { p2, n2 };
    if (std::abs(valp1 - valp2) < 0.00001)
        return { p1, n1 };
    
    mu = (isolevel - valp1) / (valp2 - valp1);
    
    p.x = p1.x + mu * (p2.x - p1.x);
    p.y = p1.y + mu * (p2.y - p1.y);
    p.z = p1.z + mu * (p2.z - p1.z);

    n.x = n1.x + mu * (n2.x - n1.x);
    n.y = n1.y + mu * (n2.y - n1.y);
    n.z = n1.z + mu * (n2.z - n1.z);

    return {p, n.Normalized()};
}

//Given a grid cell and an isolevel, calculate the triangular
//facets required to represent the isosurface through the cell.
//Return the number of triangular facets, the array "triangles"
//will be loaded up with the vertices at most 5 triangular facets.
// 0 will be returned if the grid cell is either totally above
//of totally below the isolevel
int polygonize(mcgridcell grid, float isolevel, mctriangle* triangles)
{
    int cubeindex;
    std::pair<vector3, vector3> vertlist[12];

    int edgeTable[256] = {
    0x0  , 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
    0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
    0x190, 0x99 , 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
    0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
    0x230, 0x339, 0x33 , 0x13a, 0x636, 0x73f, 0x435, 0x53c,
    0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
    0x3a0, 0x2a9, 0x1a3, 0xaa , 0x7a6, 0x6af, 0x5a5, 0x4ac,
    0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
    0x460, 0x569, 0x663, 0x76a, 0x66 , 0x16f, 0x265, 0x36c,
    0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
    0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0xff , 0x3f5, 0x2fc,
    0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
    0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x55 , 0x15c,
    0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
    0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0xcc ,
    0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
    0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
    0xcc , 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
    0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
    0x15c, 0x55 , 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
    0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
    0x2fc, 0x3f5, 0xff , 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
    0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
    0x36c, 0x265, 0x16f, 0x66 , 0x76a, 0x663, 0x569, 0x460,
    0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
    0x4ac, 0x5a5, 0x6af, 0x7a6, 0xaa , 0x1a3, 0x2a9, 0x3a0,
    0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
    0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x33 , 0x339, 0x230,
    0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
    0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x99 , 0x190,
    0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
    0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x0 };

    int triTable[256][16] =
    { {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 8, 3, 9, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 2, 10, 0, 2, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 8, 3, 2, 10, 8, 10, 9, 8, -1, -1, -1, -1, -1, -1, -1},
    {3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 11, 2, 8, 11, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 9, 0, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 11, 2, 1, 9, 11, 9, 8, 11, -1, -1, -1, -1, -1, -1, -1},
    {3, 10, 1, 11, 10, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 10, 1, 0, 8, 10, 8, 11, 10, -1, -1, -1, -1, -1, -1, -1},
    {3, 9, 0, 3, 11, 9, 11, 10, 9, -1, -1, -1, -1, -1, -1, -1},
    {9, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 3, 0, 7, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 9, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 1, 9, 4, 7, 1, 7, 3, 1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 4, 7, 3, 0, 4, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1},
    {9, 2, 10, 9, 0, 2, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
    {2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1, -1, -1, -1},
    {8, 4, 7, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11, 4, 7, 11, 2, 4, 2, 0, 4, -1, -1, -1, -1, -1, -1, -1},
    {9, 0, 1, 8, 4, 7, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
    {4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1, -1, -1, -1},
    {3, 10, 1, 3, 11, 10, 7, 8, 4, -1, -1, -1, -1, -1, -1, -1},
    {1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1, -1, -1, -1},
    {4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1, -1, -1, -1},
    {4, 7, 11, 4, 11, 9, 9, 11, 10, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 4, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 5, 4, 1, 5, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {8, 5, 4, 8, 3, 5, 3, 1, 5, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 8, 1, 2, 10, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
    {5, 2, 10, 5, 4, 2, 4, 0, 2, -1, -1, -1, -1, -1, -1, -1},
    {2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1, -1, -1, -1},
    {9, 5, 4, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 11, 2, 0, 8, 11, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
    {0, 5, 4, 0, 1, 5, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
    {2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1, -1, -1, -1},
    {10, 3, 11, 10, 1, 3, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1},
    {4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1, -1, -1, -1},
    {5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1, -1, -1, -1},
    {5, 4, 8, 5, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1},
    {9, 7, 8, 5, 7, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 3, 0, 9, 5, 3, 5, 7, 3, -1, -1, -1, -1, -1, -1, -1},
    {0, 7, 8, 0, 1, 7, 1, 5, 7, -1, -1, -1, -1, -1, -1, -1},
    {1, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 7, 8, 9, 5, 7, 10, 1, 2, -1, -1, -1, -1, -1, -1, -1},
    {10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1, -1, -1, -1},
    {8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1, -1, -1, -1},
    {2, 10, 5, 2, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1},
    {7, 9, 5, 7, 8, 9, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1, -1, -1, -1},
    {2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1, -1, -1, -1},
    {11, 2, 1, 11, 1, 7, 7, 1, 5, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1, -1, -1, -1},
    {5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1},
    {11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1},
    {11, 10, 5, 7, 11, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 0, 1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 8, 3, 1, 9, 8, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
    {1, 6, 5, 2, 6, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 6, 5, 1, 2, 6, 3, 0, 8, -1, -1, -1, -1, -1, -1, -1},
    {9, 6, 5, 9, 0, 6, 0, 2, 6, -1, -1, -1, -1, -1, -1, -1},
    {5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1, -1, -1, -1},
    {2, 3, 11, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11, 0, 8, 11, 2, 0, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 9, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
    {5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1, -1, -1, -1},
    {6, 3, 11, 6, 5, 3, 5, 1, 3, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1, -1, -1, -1},
    {3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1, -1, -1, -1},
    {6, 5, 9, 6, 9, 11, 11, 9, 8, -1, -1, -1, -1, -1, -1, -1},
    {5, 10, 6, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 3, 0, 4, 7, 3, 6, 5, 10, -1, -1, -1, -1, -1, -1, -1},
    {1, 9, 0, 5, 10, 6, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
    {10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1, -1, -1, -1},
    {6, 1, 2, 6, 5, 1, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1, -1, -1, -1},
    {8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1, -1, -1, -1},
    {7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1},
    {3, 11, 2, 7, 8, 4, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
    {5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1, -1, -1, -1},
    {0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1},
    {9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1},
    {8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1, -1, -1, -1},
    {5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1},
    {0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1},
    {6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1, -1, -1, -1},
    {10, 4, 9, 6, 4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 10, 6, 4, 9, 10, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1},
    {10, 0, 1, 10, 6, 0, 6, 4, 0, -1, -1, -1, -1, -1, -1, -1},
    {8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1, -1, -1, -1},
    {1, 4, 9, 1, 2, 4, 2, 6, 4, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1, -1, -1, -1},
    {0, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {8, 3, 2, 8, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1},
    {10, 4, 9, 10, 6, 4, 11, 2, 3, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1, -1, -1, -1},
    {3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1, -1, -1, -1},
    {6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1},
    {9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1, -1, -1, -1},
    {8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1},
    {3, 11, 6, 3, 6, 0, 0, 6, 4, -1, -1, -1, -1, -1, -1, -1},
    {6, 4, 8, 11, 6, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {7, 10, 6, 7, 8, 10, 8, 9, 10, -1, -1, -1, -1, -1, -1, -1},
    {0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1, -1, -1, -1},
    {10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1, -1, -1, -1},
    {10, 6, 7, 10, 7, 1, 1, 7, 3, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1, -1, -1, -1},
    {2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1},
    {7, 8, 0, 7, 0, 6, 6, 0, 2, -1, -1, -1, -1, -1, -1, -1},
    {7, 3, 2, 6, 7, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1, -1, -1, -1},
    {2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1},
    {1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1},
    {11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1, -1, -1, -1},
    {8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1},
    {0, 9, 1, 11, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1, -1, -1, -1},
    {7, 11, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 8, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 9, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {8, 1, 9, 8, 3, 1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
    {10, 1, 2, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 3, 0, 8, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
    {2, 9, 0, 2, 10, 9, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
    {6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1, -1, -1, -1},
    {7, 2, 3, 6, 2, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {7, 0, 8, 7, 6, 0, 6, 2, 0, -1, -1, -1, -1, -1, -1, -1},
    {2, 7, 6, 2, 3, 7, 0, 1, 9, -1, -1, -1, -1, -1, -1, -1},
    {1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1, -1, -1, -1},
    {10, 7, 6, 10, 1, 7, 1, 3, 7, -1, -1, -1, -1, -1, -1, -1},
    {10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1, -1, -1, -1},
    {0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1, -1, -1, -1},
    {7, 6, 10, 7, 10, 8, 8, 10, 9, -1, -1, -1, -1, -1, -1, -1},
    {6, 8, 4, 11, 8, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 6, 11, 3, 0, 6, 0, 4, 6, -1, -1, -1, -1, -1, -1, -1},
    {8, 6, 11, 8, 4, 6, 9, 0, 1, -1, -1, -1, -1, -1, -1, -1},
    {9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1, -1, -1, -1},
    {6, 8, 4, 6, 11, 8, 2, 10, 1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1, -1, -1, -1},
    {4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1, -1, -1, -1},
    {10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1},
    {8, 2, 3, 8, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1},
    {0, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1, -1, -1, -1},
    {1, 9, 4, 1, 4, 2, 2, 4, 6, -1, -1, -1, -1, -1, -1, -1},
    {8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1, -1, -1, -1},
    {10, 1, 0, 10, 0, 6, 6, 0, 4, -1, -1, -1, -1, -1, -1, -1},
    {4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1},
    {10, 9, 4, 6, 10, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 9, 5, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 4, 9, 5, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
    {5, 0, 1, 5, 4, 0, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
    {11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1, -1, -1, -1},
    {9, 5, 4, 10, 1, 2, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
    {6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1, -1, -1, -1},
    {7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1, -1, -1, -1},
    {3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1},
    {7, 2, 3, 7, 6, 2, 5, 4, 9, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1, -1, -1, -1},
    {3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1, -1, -1, -1},
    {6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1},
    {9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1, -1, -1, -1},
    {1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1},
    {4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1},
    {7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1, -1, -1, -1},
    {6, 9, 5, 6, 11, 9, 11, 8, 9, -1, -1, -1, -1, -1, -1, -1},
    {3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1, -1, -1, -1},
    {0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1, -1, -1, -1},
    {6, 11, 3, 6, 3, 5, 5, 3, 1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1, -1, -1, -1},
    {0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1},
    {11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1},
    {6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1, -1, -1, -1},
    {5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1, -1, -1, -1},
    {9, 5, 6, 9, 6, 0, 0, 6, 2, -1, -1, -1, -1, -1, -1, -1},
    {1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1},
    {1, 5, 6, 2, 1, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1},
    {10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1, -1, -1, -1},
    {0, 3, 8, 5, 6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {10, 5, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11, 5, 10, 7, 5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11, 5, 10, 11, 7, 5, 8, 3, 0, -1, -1, -1, -1, -1, -1, -1},
    {5, 11, 7, 5, 10, 11, 1, 9, 0, -1, -1, -1, -1, -1, -1, -1},
    {10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1, -1, -1, -1},
    {11, 1, 2, 11, 7, 1, 7, 5, 1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1, -1, -1, -1},
    {9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1, -1, -1, -1},
    {7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1},
    {2, 5, 10, 2, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1},
    {8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1, -1, -1, -1},
    {9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1, -1, -1, -1},
    {9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1},
    {1, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 7, 0, 7, 1, 1, 7, 5, -1, -1, -1, -1, -1, -1, -1},
    {9, 0, 3, 9, 3, 5, 5, 3, 7, -1, -1, -1, -1, -1, -1, -1},
    {9, 8, 7, 5, 9, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {5, 8, 4, 5, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1},
    {5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1, -1, -1, -1},
    {0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1, -1, -1, -1},
    {10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1},
    {2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1, -1, -1, -1},
    {0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1},
    {0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1},
    {9, 4, 5, 2, 11, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1, -1, -1, -1},
    {5, 10, 2, 5, 2, 4, 4, 2, 0, -1, -1, -1, -1, -1, -1, -1},
    {3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1},
    {5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1, -1, -1, -1},
    {8, 4, 5, 8, 5, 3, 3, 5, 1, -1, -1, -1, -1, -1, -1, -1},
    {0, 4, 5, 1, 0, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1, -1, -1, -1},
    {9, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 11, 7, 4, 9, 11, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1, -1, -1, -1},
    {1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1, -1, -1, -1},
    {3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1},
    {4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1, -1, -1, -1},
    {9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1},
    {11, 7, 4, 11, 4, 2, 2, 4, 0, -1, -1, -1, -1, -1, -1, -1},
    {11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1, -1, -1, -1},
    {2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1, -1, -1, -1},
    {9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1},
    {3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1},
    {1, 10, 2, 8, 7, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 9, 1, 4, 1, 7, 7, 1, 3, -1, -1, -1, -1, -1, -1, -1},
    {4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1, -1, -1, -1},
    {4, 0, 3, 7, 4, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 8, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 9, 3, 9, 11, 11, 9, 10, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 10, 0, 10, 8, 8, 10, 11, -1, -1, -1, -1, -1, -1, -1},
    {3, 1, 10, 11, 3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 11, 1, 11, 9, 9, 11, 8, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1, -1, -1, -1},
    {0, 2, 11, 8, 0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 3, 8, 2, 8, 10, 10, 8, 9, -1, -1, -1, -1, -1, -1, -1},
    {9, 10, 2, 0, 9, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1, -1, -1, -1},
    {1, 10, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 3, 8, 9, 1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 9, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 3, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1} };

    /*
       Determine the index into the edge table which
       tells us which vertices are inside of the surface
    */
    cubeindex = 0;
    if (grid.val[0] < isolevel) cubeindex |= 1;
    if (grid.val[1] < isolevel) cubeindex |= 2;
    if (grid.val[2] < isolevel) cubeindex |= 4;
    if (grid.val[3] < isolevel) cubeindex |= 8;
    if (grid.val[4] < isolevel) cubeindex |= 16;
    if (grid.val[5] < isolevel) cubeindex |= 32;
    if (grid.val[6] < isolevel) cubeindex |= 64;
    if (grid.val[7] < isolevel) cubeindex |= 128;

    /* Cube is entirely in/out of the surface */
    if (edgeTable[cubeindex] == 0)
        return 0;

    /* Find the vertices where the surface intersects the cube */
    if (edgeTable[cubeindex] & 1)
        vertlist[0] =
        VertexInterp(isolevel, grid.p[0], grid.p[1], grid.n[0], grid.n[1], grid.val[0], grid.val[1]);
    if (edgeTable[cubeindex] & 2)
        vertlist[1] =
        VertexInterp(isolevel, grid.p[1], grid.p[2], grid.n[1], grid.n[2], grid.val[1], grid.val[2]);
    if (edgeTable[cubeindex] & 4)
        vertlist[2] =
        VertexInterp(isolevel, grid.p[2], grid.p[3], grid.n[2], grid.n[3], grid.val[2], grid.val[3]);
    if (edgeTable[cubeindex] & 8)
        vertlist[3] =
        VertexInterp(isolevel, grid.p[3], grid.p[0], grid.n[3], grid.n[0], grid.val[3], grid.val[0]);
    if (edgeTable[cubeindex] & 16)
        vertlist[4] =
        VertexInterp(isolevel, grid.p[4], grid.p[5], grid.n[4], grid.n[5], grid.val[4], grid.val[5]);
    if (edgeTable[cubeindex] & 32)
        vertlist[5] =
        VertexInterp(isolevel, grid.p[5], grid.p[6], grid.n[5], grid.n[6], grid.val[5], grid.val[6]);
    if (edgeTable[cubeindex] & 64)
        vertlist[6] =
        VertexInterp(isolevel, grid.p[6], grid.p[7], grid.n[6], grid.n[7], grid.val[6], grid.val[7]);
    if (edgeTable[cubeindex] & 128)
        vertlist[7] =
        VertexInterp(isolevel, grid.p[7], grid.p[4], grid.n[7], grid.n[4], grid.val[7], grid.val[4]);
    if (edgeTable[cubeindex] & 256)
        vertlist[8] =
        VertexInterp(isolevel, grid.p[0], grid.p[4], grid.n[0], grid.n[4], grid.val[0], grid.val[4]);
    if (edgeTable[cubeindex] & 512)
        vertlist[9] =
        VertexInterp(isolevel, grid.p[1], grid.p[5], grid.n[1], grid.n[5], grid.val[1], grid.val[5]);
    if (edgeTable[cubeindex] & 1024)
        vertlist[10] =
        VertexInterp(isolevel, grid.p[2], grid.p[6], grid.n[2], grid.n[6], grid.val[2], grid.val[6]);
    if (edgeTable[cubeindex] & 2048)
        vertlist[11] =
        VertexInterp(isolevel, grid.p[3], grid.p[7], grid.n[3], grid.n[7], grid.val[3], grid.val[7]);

    /* Create the triangle */
    int ntriang = 0;
    for (int i = 0; triTable[cubeindex][i] != -1; i += 3) {
        triangles[ntriang].p[0] = vertlist[triTable[cubeindex][i]].first;
        triangles[ntriang].p[1] = vertlist[triTable[cubeindex][i + 1]].first;
        triangles[ntriang].p[2] = vertlist[triTable[cubeindex][i + 2]].first;
        triangles[ntriang].n[0] = vertlist[triTable[cubeindex][i]].second;
        triangles[ntriang].n[1] = vertlist[triTable[cubeindex][i + 1]].second;
        triangles[ntriang].n[2] = vertlist[triTable[cubeindex][i + 2]].second;
        ntriang++;
    }

    return ntriang;
}

std::vector<stdx::vec3> fillwithspheres(geometry::aabb const& box, uint count, float radius)
{
    std::unordered_set<uint> occupied;
    std::vector<stdx::vec3> spheres;

    // todo : tolerance with fold expressions for container types
    auto const roomspan = box.span() - stdx::vec3::filled(1e-5f);

    stdx::cassert(roomspan[0] > 0.f);
    stdx::cassert(roomspan[1] > 0.f);
    stdx::cassert(roomspan[2] > 0.f);

    uint const degree = static_cast<uint>(std::ceil(std::cbrt(count))) - 1;
    uint const numcells = (degree + 1) * (degree + 1) * (degree + 1);
    float const gridvol = numcells * 8 * (radius * radius * radius);

    // find the size of largest cube that can be fit into box
    float const cubelen = std::min({ roomspan[0], roomspan[1], roomspan[2] });
    float const celld = cubelen / (degree + 1);
    float const cellr = celld / 2.f;;

    // check if this box can contain all cells
    stdx::cassert(cubelen * cubelen * cubelen > gridvol);

    auto const gridorigin = box.center() - stdx::vec3::filled(cubelen / 2.f) + stdx::vec3{ 0.0f, -0.5f, 0.0f };
    for (auto i : stdx::range(0u, count))
    {
        static std::uniform_int_distribution<uint> distvoxel(0u, numcells - 1);

        uint emptycell = 0;
        while (occupied.find(emptycell) != occupied.end()) emptycell = distvoxel(re);

        occupied.insert(emptycell);

        auto const thecell = stdx::grididx<2>::from1d(degree, emptycell);
        spheres.emplace_back(stdx::vec3{ thecell[0] * celld, thecell[2] * celld, thecell[1] * celld } + stdx::vec3::filled(cellr) + gridorigin);
    }

    return spheres;
}

// params
static constexpr uint numparticles = 3;
static constexpr float roomextents = 1.6f;
static constexpr float particleradius = 0.1f;
static constexpr float h = 0.2f; // smoothing kernel constant
static constexpr float hsqr = h * h;
static constexpr float mass = 1.0f;   // particle mass
static constexpr float k = 200.0f;  // pressure constant
static constexpr float rho0 = 1.0f; // reference density
static constexpr float viscosityconstant = 1.4f;
static constexpr float fixedtimestep = 0.04f;
static constexpr float surfthreshold = 0.000001f;
static constexpr float surfthresholdsqr = surfthreshold * surfthreshold;
static constexpr float isolevel = 0.000001f;
static constexpr float isolevelsqr = isolevel * isolevel;
static constexpr float maxacc = 100.0;
static constexpr float maxspeed = 20.0f;
static constexpr float sqrt2 = 1.41421356237f;

sphfluid::sphfluid(geometry::aabb const& _bounds) : container(_bounds)
{
    particlegeometry = geometry::sphere{ {0.0f, 0.0f, 0.0f }, particleradius };
   
    //auto const& redmaterial = gfx::globalresources::get().mat("ball");
    particleparams.reserve(numparticles);
    
    static std::uniform_real_distribution<float> distvel(-0.1f, 0.1f);
    static std::uniform_real_distribution<float> distacc(-0.1f, 0.1f);

    auto const intialhalfspan = _bounds.span() / (2.0f);
    geometry::aabb initial_bounds = { _bounds.center() - intialhalfspan, _bounds.center() + intialhalfspan };

    //geometry::aabb initial_bounds = { vector3(-2.0f), vector3(2.0f) };
    for (auto const& center : fillwithspheres(initial_bounds, numparticles, particleradius))
    {
        particleparams.emplace_back();
        particleparams.back().p = vector3(center.data());

        // todo : expose this limit of 5000 somewhere
        // better yet make it a template parameter of the generator function
        stdx::cassert(numparticles <= 5000u);
        
        // todo : material id
        //particleparams.back().matname = gfx::generaterandom_matcolor(redmaterial);

        particleparams.back().v = particleparams.back().a = vector3::Zero;
        // give small random acceleration and velocity to particles
        //particleparams.back().v = vector3(distvel(re), distvel(re), distvel(re));
        //particleparams.back().a = vector3(distacc(re), distacc(re), distacc(re));

        // compute velocity at -half time step
        particleparams.back().vp = particleparams.back().v - (0.5f * fixedtimestep) * particleparams.back().a;
    }
}

float speedofsoundsqr(float density, float pressure)
{
    static constexpr float specificheatratio = 1.0f;
    return density < 0.000000001f ? 0.0f : specificheatratio * pressure / density;
}

float sphfluid::computetimestep() const
{
    static constexpr float mintimestep = 1.0f / 240.0f;
    static constexpr float counrant_safetyconst = 1.0f;

    float maxcsqr = 0.0f;
    float maxvsqr = 0.0f;
    float maxasqr = 0.0f;

    for (auto const& p : particleparams)
    {
        float const c = speedofsoundsqr(p.rho, p.pr);
        maxcsqr = std::max(c * c, maxcsqr);
        maxvsqr = std::max(p.v.Dot(p.v), maxvsqr);
        maxasqr = std::max(p.a.Dot(p.a), maxasqr);
    }

    float maxv = std::max(0.000001f, std::sqrt(maxvsqr));
    float maxa = std::max(0.000001f, std::sqrt(maxasqr));
    float maxc = std::max(0.000001f, std::sqrt(maxcsqr));

    return std::max(mintimestep, std::min({ counrant_safetyconst * h / maxv, std::sqrt(h / maxa), counrant_safetyconst * h / maxc }));
}

vector3 sphfluid::center()
{
    return vector3::Zero;
}

std::vector<uint8_t> sphfluid::texturedata() const
{
    return std::vector<uint8_t>(4);
}

std::vector<gfx::vertex> sphfluid::vertices() const
{
    return fluidsurface;
}

std::vector<uint32> const& sphfluid::indices() const
{
    return fluidsurfaceindices;
}

std::vector<gfx::instance_data> sphfluid::instancedata() const
{
    std::vector<gfx::instance_data> particles_instancedata;
    for (auto const& particleparam : particleparams)
    {
        particles_instancedata.emplace_back(matrix::CreateTranslation(particleparam.p), gfx::globalresources::get().view());
    }

    return particles_instancedata;
}

std::vector<gfx::vertex> sphfluid::particlevertices() const
{
    return particlegeometry.vertices();
}

constexpr float poly6kernelcoeff()
{
    return 315.0f/(64.0f * XM_PI * stdx::pown(h, 9u));
}

constexpr float poly6gradcoeff()
{
    return -1890.0f / (64.0f * XM_PI * stdx::pown(h, 9u));
}

constexpr float spikykernelcoeff()
{
    return -45.0f / (XM_PI * stdx::pown(h, 6u));
}

constexpr float viscositylaplaciancoeff()
{
    return 45.0f / (XM_PI * stdx::pown(h, 6u));
}

// optimized distance computation that exploits symmetry of square
//void computegridval(std::vector<sphfluid::params> const& particleparams, vector3* pos, float* o_val)
//{
//    static constexpr float halfextent = 0.1f / 2.0f;
//    static constexpr float a2 = 3.0f * halfextent * halfextent;
//    vector3 const cellcenter = pos[0] + vector3(halfextent);
//
//    for (auto param : particleparams)
//    {
//        float rcp_rho = 1.0f / param.rho;
//        vector3 const c = param.p - cellcenter;
//        float const c2 = c.LengthSquared();
//
//        float const t0 = a2 + c2;
//        static constexpr float t1 = 2.0f;
//
//        vector3 ac = c * halfextent;
//
//        // note : the vertices should be sorted to form a rectangular contour
//        o_val[0] += stdx::pown(std::max(0.0f, hsqr - (t0 + t1 * (ac.x + ac.y + ac.z))), 3) * rcp_rho;
//        o_val[1] += stdx::pown(std::max(0.0f, hsqr - (t0 - t1 * (ac.x - ac.y - ac.z))), 3) * rcp_rho;
//        o_val[2] += stdx::pown(std::max(0.0f, hsqr - (t0 - t1 * (ac.x + ac.y - ac.z))), 3) * rcp_rho;
//        o_val[3] += stdx::pown(std::max(0.0f, hsqr - (t0 - t1 * (ac.y - ac.z - ac.x))), 3) * rcp_rho;
//
//        o_val[4] += stdx::pown(std::max(0.0f, hsqr - (t0 - t1 * (ac.z - ac.x - ac.y))), 3) * rcp_rho;
//        o_val[5] += stdx::pown(std::max(0.0f, hsqr - (t0 - t1 * (ac.x - ac.y + ac.z))), 3) * rcp_rho;
//        o_val[6] += stdx::pown(std::max(0.0f, hsqr - (t0 - t1 * (ac.x + ac.y + ac.z))), 3) * rcp_rho;
//        o_val[7] += stdx::pown(std::max(0.0f, hsqr - (t0 - t1 * (ac.y + ac.z - ac.x))), 3) * rcp_rho;
//    }
//
//    for (uint j(0u); j < 8; ++j)
//    {
//        o_val[j] *= poly6kernelcoeff();
//    }
//}

void sphfluid::update(float dt)
{
    float remainingtime = dt;
    //while (remainingtime > 0.0f)
    {
        float timestep = dt;// std::min(computetimestep(), remainingtime);
        //remainingtime -= timestep;

        // compute pressure and densities
        for (auto& particleparam : particleparams)
        {
            particleparam.rho = 0.0f;
            for (auto neighbourparam : particleparams)
            {
                float const distsqr = vector3::DistanceSquared(particleparam.p, neighbourparam.p);

                if (distsqr < hsqr)
                {
                    float const term = stdx::pown(hsqr - distsqr, 3u);
                    particleparam.rho += poly6kernelcoeff() * term;
                }
            }

            // prevent density smaller than reference density to avoid negative pressure
            particleparam.rho = std::max(particleparam.rho, rho0);
            particleparam.pr = k * (particleparam.rho - rho0);
        }
        
        static constexpr float repulsion_force = 1.0f;
        auto const& containercenter = container.center();
        auto const& halfextents = container.span() / 2.0f;

        // compute acceleration
        for (auto& particleparam : particleparams)
        {
            auto const& v = particleparam.v;
            auto const& rho = particleparam.rho;
            auto const& pr = particleparam.pr;

            particleparam.a = vector3::Zero;

            for (auto const& neighbourparam : particleparams)
            {
                auto const& nv = neighbourparam.v;
                auto const& nrho = neighbourparam.rho;
                auto const& npr = neighbourparam.pr;

                auto toneighbour = neighbourparam.p - particleparam.p;
                float const dist = toneighbour.Length();

                float const diff = h - dist;

                if (diff > 0)
                {
                    if (dist > 0)
                    {
                        toneighbour.Normalize();
                    }

                    // acceleration due to pressure
                    particleparam.a += (spikykernelcoeff() * (pr + npr) * stdx::pown(diff, 2u) / (2.0f * rho * nrho)) * toneighbour;

                    // accleration due to viscosity
                    particleparam.a += (viscosityconstant * (nv - v) * viscositylaplaciancoeff() * diff) / nrho;
                }
            }

            // add gravity
            particleparam.a += vector3(0.0f, -2.0f, 0.0f);

            // collisions(just reflect velocity) and leap frog integration
            {
                auto const& localpt = particleparam.p - vector3(containercenter.data());
                auto const localpt_abs = vector3{ std::abs(localpt.x), std::abs(localpt.y), std::abs(localpt.z) };

                vector3 normal = vector3::Zero;
                if (localpt_abs.x > halfextents[0])
                {
                    normal += -stdx::sign(localpt.x) * vector3::UnitX;
                }

                if (localpt_abs.y > halfextents[1])
                {
                    normal += -stdx::sign(localpt.y) * vector3::UnitY;
                }

                if (localpt_abs.z > halfextents[2])
                {
                    normal += -stdx::sign(localpt.z) * vector3::UnitZ;
                }

                if (normal != vector3::Zero)
                {
                    normal.Normalize();

                    auto const& penetration = localpt_abs - vector3(halfextents.data());
                    auto const impulsealongnormal = particleparam.v.Dot(-normal);

                    static constexpr float restitution = 0.3f;
                    particleparam.p += penetration * normal;
                    particleparam.v += (1.0f + restitution) * impulsealongnormal * normal;
                }

                if (particleparam.a.LengthSquared() > maxacc * maxacc)
                {
                    particleparam.a = particleparam.a.Normalized() * maxacc;
                }

                if (particleparam.v.LengthSquared() > maxspeed * maxspeed)
                {
                    particleparam.v = particleparam.v.Normalized() * maxspeed;
                }

                particleparam.vp = particleparam.v + particleparam.a * timestep;
                particleparam.p += particleparam.vp * timestep;
                particleparam.v = particleparam.vp + particleparam.a * (0.5f * timestep);
            }
        }
    }

    fluidsurface.clear();
    fluidsurfaceindices.clear();

    static constexpr float marchingcube_size = 0.1f;

    // assume container it is a cube
    auto container_len = container.span()[0];
    auto nummarches_perdim = stdx::ceil(container_len / marchingcube_size) + 2;
    auto nummarches = nummarches_perdim * nummarches_perdim * nummarches_perdim;
    auto offset = container.min_pt - stdx::vec3::filled(marchingcube_size);
    for (uint i = 0; i < nummarches; ++i)
    {
        auto const cell = stdx::grididx<2>::from1d(nummarches_perdim - 1u, i);
        mcgridcell gridcell;

        // the vertices should be sorted to form a rectangular contour
        gridcell.p[0] = vector3(cell[0] * marchingcube_size, cell[1] * marchingcube_size, cell[2] * marchingcube_size) + vector3(offset.data());
        gridcell.p[1] = gridcell.p[0] + vector3(marchingcube_size, 0.0f, 0.0f);
        gridcell.p[2] = gridcell.p[0] + vector3(marchingcube_size, marchingcube_size, 0.0f);
        gridcell.p[3] = gridcell.p[0] + vector3(0.0f, marchingcube_size, 0.0f);
        
        gridcell.p[4] = gridcell.p[0] + vector3(0.0f, 0.0f, marchingcube_size);
        gridcell.p[5] = gridcell.p[0] + vector3(marchingcube_size, 0.0f, marchingcube_size);
        gridcell.p[6] = gridcell.p[0] + vector3(marchingcube_size, marchingcube_size, marchingcube_size);
        gridcell.p[7] = gridcell.p[0] + vector3(0.0f, marchingcube_size, marchingcube_size);

        for (uint j(0u); j < 8; ++j)
        {
            float c = 0.0f;
            vector3 g = vector3::Zero;
            for (auto neighbourparam : particleparams)
            {
                auto toneighbour = neighbourparam.p - gridcell.p[j];
                float const distsqr = toneighbour.LengthSquared();

                float const diffsqr = std::max(0.0f, hsqr - distsqr);

                if (diffsqr > 0.0f)
                {
                    // note: colour field is also its density field atm, since it is also one
                    c += poly6kernelcoeff() * stdx::pown(diffsqr, 3u) / neighbourparam.rho;
                }

                // use marching cube diagonal size as smoothing radius for normals, since otherwise gradient can be zero at cube corners if no particles are present within smoothing radius
                // multiply by two since the isolevel could be at the marching cube outside the one containing surface particles
                // the second statement needs more thought?
                static constexpr float normalh = 2.0f * marchingcube_size * sqrt2;
                static constexpr float normalhsqr = normalh * normalh;
                float const diffsqr_normalsmoothing = std::max(0.0f, normalhsqr - distsqr);

                if (diffsqr_normalsmoothing > 0.0f)
                {
                    if (toneighbour.LengthSquared() > 0.0f)
                        toneighbour.Normalize();

                    g += poly6gradcoeff() * stdx::pown(diffsqr_normalsmoothing, 2u) * toneighbour / neighbourparam.rho;
                }
            }

            gridcell.val[j] = c;
            gridcell.n[j] = g.Normalized();
        }

        std::array<mctriangle, 5u> triangles;

        // if 0 then either all verts are outside the surface or inside,
        if (auto numtris = polygonize(gridcell, isolevel, triangles.data()))
        {
            for (uint j(0); j < numtris; ++j)
            {
                fluidsurface.push_back(gfx::vertex{ triangles[j].p[0], triangles[j].n[0] });
                fluidsurface.push_back(gfx::vertex{ triangles[j].p[1], triangles[j].n[1] });
                fluidsurface.push_back(gfx::vertex{ triangles[j].p[2], triangles[j].n[2] });

                // todo : populate fluidsurfaceindices
            }
        }
    }
}

sphfluidintro::sphfluidintro(view_data const& viewdata) : sample_base(viewdata)
{
	camera.Init({ 0.f, 0.f, -30.f });
	camera.SetMoveSpeed(10.0f);
}

gfx::resourcelist sphfluidintro::create_resources()
{
    using geometry::cube;
    using geometry::sphere;
    using gfx::bodyparams;
    using gfx::material;

    auto& globalres = gfx::globalresources::get();
    auto& globals = globalres.cbuffer().data();

    // initialize lights
    globals.numdirlights = 1;
    globals.numpointlights = 2;

    globals.ambient = { 0.1f, 0.1f, 0.1f, 1.0f };
    globals.lights[0].direction = vector3{ 0.3f, -0.27f, 0.57735f }.Normalized();
    globals.lights[0].color = { 0.2f, 0.2f, 0.2f };

    globals.lights[1].position = { -15.f, 15.f, -15.f };
    globals.lights[1].color = { 1.f, 1.f, 1.f };
    globals.lights[1].range = 40.f;

    globals.lights[2].position = { 15.f, 15.f, -15.f };
    globals.lights[2].color = { 1.f, 1.f, 1.f };
    globals.lights[2].range = 40.f;

    globalres.cbuffer().updateresource();

    // since these use static vertex buffers, just send 0 as maxverts
    boxes.emplace_back(cube{ vector3{0.f, 0.f, 0.f}, vector3{roomextents} }, &cube::vertices_flipped, &cube::instancedata, bodyparams{ 0, 1, "instanced" });
    fluid.emplace_back(sphfluid(boxes[0]->bbox()), bodyparams{ 20000, 1, "default_twosided", 0});
    fluidparticles.emplace_back(fluid.back().get(), &sphfluid::particlevertices, &sphfluid::instancedata, bodyparams{0, numparticles, "instanced"});

    gfx::resourcelist res;
    for (auto b : stdx::makejoin<gfx::bodyinterface>(boxes, fluid, fluidparticles)) { stdx::append(b->create_resources(), res); };
    return res;
}

void sphfluidintro::update(float dt)
{
    sample_base::update(dt);
    for (auto b : stdx::makejoin<gfx::bodyinterface>(boxes, fluid)) b->update(dt);
}

void sphfluidintro::render(float dt)
{
    static constexpr bool vizparticles = true;
    if (vizparticles)
    {
        for (auto b : stdx::makejoin<gfx::bodyinterface>(boxes, fluidparticles, fluid)) b->render(dt, { true });
    }
    else
    {
        for (auto b : stdx::makejoin<gfx::bodyinterface>(boxes, fluid)) b->render(dt, { false });
    }
}
