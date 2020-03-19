/*
    Define squared Jinc function.

    See Pratt "Digital Image Processing" p.97 for Jinc/Bessel functions.
    http://mathworld.wolfram.com/JincFunction.html and page 11 of
    http://www.ph.ed.ac.uk/%7ewjh/teaching/mo/slides/lens/lens.pdf.

    Calculate Jinc function by Taylor series, asymptotic expansion
    or C++ special function, according to the different radius.
*/

#ifndef JINCFUNC_HPP_
#define JINCFUNC_HPP_

#include <cmath>

#ifndef M_PI // GCC seems to have it
constexpr double M_PI = 3.14159265358979323846;
#endif

// Taylor series coefficients of 2*BesselJ1(pi*x)/(pi*x) as (x^2) -> 0
double jinc_taylor_series[31] =
{
     1.0,
    -1.23370055013616982735431137,
     0.507339015802096027273126733,
    -0.104317403816764804365258186,
     0.0128696438477519721233840271,
    -0.00105848577966854543020422691,
     6.21835470803998638484476598e-05,
    -2.73985272294670461142756204e-06,
     9.38932725442064547796003405e-08,
    -2.57413737759717407304931036e-09,
     5.77402672521402031756429343e-11,
    -1.07930605263598241754572977e-12,
     1.70710316782347356046974552e-14,
    -2.31434518382749184406648762e-16,
     2.71924659665997312120515390e-18,
    -2.79561335187943028518083529e-20,
     2.53599244866299622352138464e-22,
    -2.04487273140961494085786452e-24,
     1.47529860450204338866792475e-26,
    -9.57935105257523453155043307e-29,
     5.62764317309979254140393917e-31,
    -3.00555258814860366342363867e-33,
     1.46559362903641161989338221e-35,
    -6.55110024064596600335624426e-38,
     2.69403199029404093412381643e-40,
    -1.02265499954159964097119923e-42,
     3.59444454568084324694180635e-45,
    -1.17313973900539982313119019e-47,
     3.56478606255557746426034301e-50,
    -1.01100655781438313239513538e-52,
     2.68232117541264485328658605e-55
};

double jinc_zeros[16] =
{
    1.2196698912665045,
    2.2331305943815286,
    3.2383154841662362,
    4.2410628637960699,
    5.2427643768701817,
    6.2439216898644877,
    7.2447598687199570,
    8.2453949139520427,
    9.2458926849494673,
    10.246293348754916,
    11.246622794877883,
    12.246898461138105,
    13.247132522181061,
    14.247333735806849,
    15.247508563037300,
    16.247661874700962
};

//  Modified from boost package math/tools/`rational.hpp`
//
//  (C) Copyright John Maddock 2006.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
double evaluate_rational(const double* num, const double* denom, double z, int count)
{
    double s1, s2;
    if (z <= 1.0)
    {
        s1 = num[count-1];
        s2 = denom[count-1];
        for (auto i = count - 2; i >= 0; --i)
        {
            s1 *= z;
            s2 *= z;
            s1 += num[i];
            s2 += denom[i];
        }
    }
    else
    {
        z = 1.0f / z;
        s1 = num[0];
        s2 = denom[0];
        for (auto i = 1; i < count; ++i)
        {
        s1 *= z;
        s2 *= z;
        s1 += num[i];
        s2 += denom[i];
        }
    }

    return s1 / s2;
}

//  Modified from boost package `BesselJ1.hpp`
//
//  Copyright (c) 2006 Xiaogang Zhang
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
double jinc_sqr_boost_l(double x2)
{
    static const double bPC[7] =
    {
        -4.4357578167941278571e+06,
        -9.9422465050776411957e+06,
        -6.6033732483649391093e+06,
        -1.5235293511811373833e+06,
        -1.0982405543459346727e+05,
        -1.6116166443246101165e+03,
        0.0
    };
    static const double bQC[7] =
    {
        -4.4357578167941278568e+06,
        -9.9341243899345856590e+06,
        -6.5853394797230870728e+06,
        -1.5118095066341608816e+06,
        -1.0726385991103820119e+05,
        -1.4550094401904961825e+03,
        1.0
    };
    static const double bPS[7] =
    {
        3.3220913409857223519e+04,
        8.5145160675335701966e+04,
        6.6178836581270835179e+04,
        1.8494262873223866797e+04,
        1.7063754290207680021e+03,
        3.5265133846636032186e+01,
        0.0
    };
    static const double bQS[7] =
    {
        7.0871281941028743574e+05,
        1.8194580422439972989e+06,
        1.4194606696037208929e+06,
        4.0029443582266975117e+05,
        3.7890229745772202641e+04,
        8.6383677696049909675e+02,
        1.0
    };

    auto y2 = M_PI * M_PI * x2;
    auto xp = std::sqrt(y2);
    auto y2p = 64.0 / y2;
    auto yp = 8.0 / xp;
    auto factor = std::sqrt(xp / M_PI) * 2.0 / y2;
    auto rc = evaluate_rational(bPC, bQC, y2p, 7);
    auto rs = evaluate_rational(bPS, bQS, y2p, 7);
    auto sx = std::sin(xp);
    auto cx = std::cos(xp);

    return factor * (rc * (sx - cx) + yp * rs * (sx + cx));
}

// jinc(sqrt(x2))
double jinc_sqr(double x2)
{
    if (x2 < 1.49)        // the 1-tap radius
    {
        double res = 0.0;
        for (auto j = 16; j > 0; --j)
            res = res * x2 + jinc_taylor_series[j - 1];
        return res;
    }
    else if (x2 < 4.97)   // the 2-tap radius
    {
        double res = 0.0;
        for (auto j = 21; j > 0; --j)
            res = res * x2 + jinc_taylor_series[j - 1];
        return res;
    }
    else if (x2 < 10.49)  // the 3-tap radius
    {
        double res = 0.0;
        for (auto j = 26; j > 0; --j)
            res = res * x2 + jinc_taylor_series[j - 1];
        return res;
    }
    else if (x2 < 17.99)  // the 4-tap radius
    {
        double res = 0.0;
        for (auto j = 31; j > 0; --j)
            res = res * x2 + jinc_taylor_series[j - 1];
        return res;
    }
    else if (x2 < 52.57)  // the 5~7-tap radius
    {
        auto x = M_PI * std::sqrt(x2);
        #if (defined(_MSC_VER) && _MSC_VER < 1914) || (defined(__GNUC__) && __GNUC__ < 7) || defined(__clang__)
            return 2.0 * j1(x) / x;
        #else
            return 2.0 * std::cyl_bessel_j(1, x) / x;
        #endif
    }
    else if (x2 < 68.07)  // the 8-tap radius // Modify from pull request #4
    {
        return jinc_sqr_boost_l(x2);
    }
    else                  // the 9~16-tap radius
    {
        auto x = M_PI * std::sqrt(x2);
        #if (defined(_MSC_VER) && _MSC_VER < 1914) || (defined(__GNUC__) && __GNUC__ < 7) || defined(__clang__)
            return 2.0 * j1(x) / x;
        #else
            return 2.0 * std::cyl_bessel_j(1, x) / x;
        #endif
    }
}

#endif
