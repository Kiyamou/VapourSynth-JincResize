/*
*    Help function:
*      1.clamp():
*          Return the middle value, which is similar with np.clip() in Python
*          Called in EWAResizer.hpp and JincResize.cpp
*      2.sample_sqr():
*          Data preprocessing for jinc_sqr() in JincFunc.hpp
*          Called in JincResize.cpp
*/


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
