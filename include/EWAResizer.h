/*
*                     EWA: Elliptical Weighted Averaging
*
*                 port from AviSynth jinc-resize, origin repo:
*    https://github.com/AviSynth/jinc-resize/blob/master/JincResize/EWAResizerStruct.h
*/


#ifndef EWARESIZER_H_
#define EWARESIZER_H_

struct EWAPixelCoeffMeta
{
    int start_x, start_y;
    int coeff_meta;
};

struct EWAPixelCoeff
{
    float* factor;
    EWAPixelCoeffMeta* meta;
    int* factor_map;
    int filter_size, quantize_x, quantize_y, coeff_stride;
};

#endif
