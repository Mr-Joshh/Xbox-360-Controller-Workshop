// shape.cpp - see shape.h
#include "shape.h"

namespace
{
    float Cross(float ax, float ay, float bx, float by, float cx, float cy)
    {
        return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    }

    bool InTriangle(float px, float py, float ax, float ay, float bx, float by, float cx, float cy)
    {
        float d1 = Cross(ax, ay, bx, by, px, py);
        float d2 = Cross(bx, by, cx, cy, px, py);
        float d3 = Cross(cx, cy, ax, ay, px, py);
        bool neg = d1 < 0 || d2 < 0 || d3 < 0;
        bool pos = d1 > 0 || d2 > 0 || d3 > 0;
        return !(neg && pos);
    }
}

namespace Shape
{
    int Spline(const float* x, const float* y, int n, int steps, float* outX, float* outY)
    {
        int count = 0;
        for (int i = 0; i < n; i++)
        {
            int i0 = (i + n - 1) % n, i1 = i, i2 = (i + 1) % n, i3 = (i + 2) % n;
            for (int s = 0; s < steps; s++)
            {
                float t = (float)s / steps, t2 = t * t, t3 = t2 * t;
                float b0 = -0.5f * t3 + t2 - 0.5f * t;
                float b1 = 1.5f * t3 - 2.5f * t2 + 1.0f;
                float b2 = -1.5f * t3 + 2.0f * t2 + 0.5f * t;
                float b3 = 0.5f * t3 - 0.5f * t2;
                outX[count] = b0 * x[i0] + b1 * x[i1] + b2 * x[i2] + b3 * x[i3];
                outY[count] = b0 * y[i0] + b1 * y[i1] + b2 * y[i2] + b3 * y[i3];
                count++;
            }
        }
        return count;
    }

    int Triangulate(const float* x, const float* y, int n, unsigned short* out)
    {
        const int MAXN = 1024;
        if (n < 3 || n > MAXN)
            return 0;
        unsigned short idx[MAXN];
        for (int i = 0; i < n; i++)
            idx[i] = (unsigned short)i;

        float area = 0;
        for (int i = 0; i < n; i++)
        {
            int j = (i + 1) % n;
            area += x[i] * y[j] - x[j] * y[i];
        }
        float sign = area > 0 ? 1.0f : -1.0f;

        int tris = 0, remaining = n, guard = 0;
        int i = 0;
        while (remaining > 3 && guard < remaining * 2)
        {
            int a = idx[(i + remaining - 1) % remaining], b = idx[i % remaining], c = idx[(i + 1) % remaining];
            bool ear = sign * Cross(x[a], y[a], x[b], y[b], x[c], y[c]) > 0;
            for (int k = 0; ear && k < remaining; k++)
            {
                int p = idx[k];
                if (p != a && p != b && p != c && InTriangle(x[p], y[p], x[a], y[a], x[b], y[b], x[c], y[c]))
                    ear = false;
            }
            if (ear)
            {
                out[tris * 3] = (unsigned short)a;
                out[tris * 3 + 1] = (unsigned short)b;
                out[tris * 3 + 2] = (unsigned short)c;
                tris++;
                for (int k = i % remaining; k < remaining - 1; k++)
                    idx[k] = idx[k + 1];
                remaining--;
                guard = 0;
            }
            else
            {
                i++;
                guard++;
            }
            i %= remaining;
        }
        if (remaining == 3)
        {
            out[tris * 3] = idx[0];
            out[tris * 3 + 1] = idx[1];
            out[tris * 3 + 2] = idx[2];
            tris++;
        }
        return tris;
    }
}
