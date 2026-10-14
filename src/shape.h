// shape.h - smooth closed outlines and polygon triangulation, for the controller silhouette.
#pragma once

namespace Shape
{
    // Samples a closed Catmull-Rom spline through n control points, 'steps' points per segment.
    // Writes n * steps points to outX/outY and returns that count.
    int Spline(const float* x, const float* y, int n, int steps, float* outX, float* outY);

    // Ear-clipping triangulation of a simple polygon (either winding).
    // Writes 3 indices per triangle to 'out' (room for 3 * (n - 2)) and returns the triangle count.
    int Triangulate(const float* x, const float* y, int n, unsigned short* out);
}
