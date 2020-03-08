#ifndef HELPER_HPP_
#define HELPER_HPP_

#include <algorithm>

template <typename T>
inline T clamp(T input, T range_min, T range_max)
{
    return std::min(std::max(input, range_min), range_max);
}

double sample_sqr(double (*filter)(double), double x2, double blur2, double radius2)
{
    if (blur2 > 0.0)
        x2 /= blur2;

    if (x2 < radius2)
        return filter(x2);

    return 0.0;
}

#endif
