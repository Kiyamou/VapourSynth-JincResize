#ifndef LUT_HPP_
#define LUT_HPP_

#if !defined(_MSC_VER)
#include "immintrin.h"
#endif

#include "Helper.hpp"
#include "JincFunc.hpp"

constexpr double JINC_ZERO_SQR = 1.48759464366204680005356;

class Lut
{
public:
    Lut();
    ~Lut();

    double* InitLut(int lut_size, double radius, double blur);
    void DestroyLutTable();

    double* lut;

private:
    int lut_size;
    double radius;
    double blur;
};

Lut::Lut()
{
#if defined(_MSC_VER)
    lut = new double[lut_size];
#else
    lut = (double*)_mm_malloc(sizeof(double) * lut_size, 64);
#endif
}

double* Lut::InitLut(int lut_size, double radius, double blur)
{
#if defined(_MSC_VER)
    auto radius2 = radius * radius;
    auto blur2 = blur * blur;
    for (auto i = 0; i < lut_size; ++i)
    {
        double t2 = i / (lut_size - 1.0);
        double filter = sample_sqr(jinc_sqr, radius2 * t2, blur2, radius2);
        double window = sample_sqr(jinc_sqr, JINC_ZERO_SQR * t2, 1.0, radius2);
        lut[i] = filter * window;
    }
#else
    double* filters = (double *)_mm_malloc(sizeof(double) * lut_size, 64);
    double* windows = (double *)_mm_malloc(sizeof(double) * lut_size, 64);
    auto radius2 = radius * radius;
    auto blur2 = blur * blur;
    for (auto i = 0; i < lut_size; i++)
    {
        auto t2 = i / (lut_size - 1.0);
        filters[i] = sample_sqr(jinc_sqr, radius2 * t2, blur2, radius2);
        windows[i] = sample_sqr(jinc_sqr, JINC_ZERO_SQR * t2, 1.0, radius2);
    }
    for (auto j = 0; j < lut_size / 4; ++j)
    {
        auto rf = _mm256_load_pd(filters + j * 4);
        auto rw = _mm256_load_pd(windows + j * 4);
        auto rl = _mm256_mul_pd(rf, rw);
        _mm256_store_pd(lut + j * 4, rl);
    }
    _mm_free(filters);
    _mm_free(windows);
#endif
    return lut;
}

void Lut::DestroyLutTable()
{
#if defined(_MSC_VER)
    delete[] lut;
#else
    _mm_free(lut);
#endif
}

#endif
