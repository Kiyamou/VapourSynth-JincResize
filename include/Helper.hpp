/*
*    Help function:
*      1.clamp():
*          Return the middle value, which is similar with np.clip() in Python
*          Called in EWAResizer.hpp
*      2.sample_sqr():
*          Data preprocessing for jinc_sqr() in JincFunc.hpp
*          Called in Lut.cpp
*      3.reduce():
*          Horizontal sum of 8 packed 32bit floats
*          By Z boson
*          Form https://stackoverflow.com/questions/13879609/horizontal-sum-of-8-packed-32bit-floats/18616679#18616679
*          Called in EWAResizer.hpp
*/


#ifndef HELPER_HPP_
#define HELPER_HPP_

// For simplicity, only use AVX2 with GCC compiler
#if !defined(_MSC_VER)
#define USE_AVX2
#endif

#include <algorithm>

#if defined(USE_AVX2)
#include "immintrin.h"
#endif

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

#if defined(USE_AVX2)
inline float reduce(__m256 a)
{
	__m256 t1 = _mm256_hadd_ps(a, a);
	__m256 t2 = _mm256_hadd_ps(t1, t1);
	__m128 t3 = _mm256_extractf128_ps(t2, 1);
	__m128 t4 = _mm_add_ss(_mm256_castps256_ps128(t2), t3);
	return _mm_cvtss_f32(t4);        
}
#endif

#endif
