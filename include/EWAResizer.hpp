/*
*                     EWA: Elliptical Weighted Averaging
*
*                 port from AviSynth jinc-resize, origin repo:
*    https://github.com/AviSynth/jinc-resize/blob/master/JincResize/EWAResizer.h
*/


#ifndef EWARESIZER_HPP_
#define EWARESIZER_HPP_

#include <vector>

#include "vapoursynth/VapourSynth.h"
#include "vapoursynth/VSHelper.h"

#include "EWAResizer.h"
#include "Lut.hpp"

constexpr double DOUBLE_ROUND_MAGIC_NUMBER = 6755399441055744.0;

static void init_coeff_table(EWAPixelCoeff* out, int quantize_x, int quantize_y,
    int filter_size, int dst_width, int dst_height)
{
    out->filter_size = filter_size;
    out->quantize_x = quantize_x;
    out->quantize_y = quantize_y;
    out->coeff_stripe = ((filter_size + 7) / 8) * 8;

    // Allocate metadata
    out->meta = new EWAPixelCoeffMeta[dst_width * dst_height];

    // Alocate factor map
    if (quantize_x * quantize_y > 0)
        out->factor_map = new int[quantize_x * quantize_y];
    else
        out->factor_map = nullptr;

    // This will be reserved to exact size in coff generating procedure
    out->factor = nullptr;

    // Zeroed memory
    if (out->factor_map != nullptr)
        memset(out->factor_map, 0, quantize_x * quantize_y * sizeof(int));

    memset(out->meta, 0, dst_width * dst_height * sizeof(EWAPixelCoeffMeta));
}

void delete_coeff_table(EWAPixelCoeff* out)
{
    if(out == nullptr)
        return;

    vs_aligned_free(out->factor);
    delete[] out->meta;
    delete[] out->factor_map;
}

/* Coefficient table generation */
void generate_coeff_table_c(Lut* func, EWAPixelCoeff* out, int quantize_x, int quantize_y,
    int samples, int src_width, int src_height, int dst_width, int dst_height, double radius,
    double crop_left, double crop_top, double crop_width, double crop_height)
{
    const double filter_scale_x = (double)dst_width / crop_width;
    const double filter_scale_y = (double)dst_height / crop_height;

    const double filter_step_x = std::min(filter_scale_x, 1.0);
    const double filter_step_y = std::min(filter_scale_y, 1.0);

    const float filter_support_x = (float)radius / filter_step_x;
    const float filter_support_y = (float)radius / filter_step_y;

    const int filter_size_x = (int)ceil(filter_support_x * 2.0);
    const int filter_size_y = (int)ceil(filter_support_y * 2.0);

    const float filter_support = std::max(filter_support_x, filter_support_y);
    const int filter_size = std::max(filter_size_x, filter_size_y);

    const float start_x = (float)(crop_left + (crop_width - dst_width) / (dst_width * 2));
    const float start_y = (float)(crop_top + (crop_height - dst_height) / (dst_height * 2));

    const float x_step = (float)(crop_width / dst_width);
    const float y_step = (float)(crop_height / dst_height);

    float xpos = start_x;
    float ypos = start_y;

    // Initialize EWAPixelCoeff data structure
    init_coeff_table(out, quantize_x, quantize_y, filter_size, dst_width, dst_height);

    std::vector<float> tmp_array;
    int tmp_array_top = 0;

    // Use to advance the coeff pointer
    const int coeff_per_pixel = out->coeff_stripe * filter_size;

    for (int y = 0; y < dst_height; y++)
    {
        for (int x = 0; x < dst_width; x++)
        {
            bool is_border = false;

            EWAPixelCoeffMeta* meta = &out->meta[y * dst_width + x];

            // Here, the window_*** variable specified a begin/size/end
            // of EWA window to process.
            int window_end_x = int(xpos + filter_support);
            int window_end_y = int(ypos + filter_support);

            if (window_end_x >= src_width)
            {
                window_end_x = src_width - 1;
                is_border = true;
            }
            if (window_end_y >= src_height)
            {
                window_end_y = src_height - 1;
                is_border = true;
            }

            int window_begin_x = window_end_x - filter_size + 1;
            int window_begin_y = window_end_y - filter_size + 1;

            if (window_begin_x < 0)
            {
                window_begin_x = 0;
                is_border = true;
            }
            if (window_begin_y < 0)
            {
                window_begin_y = 0;
                is_border = true;
            }

            meta->start_x = window_begin_x;
            meta->start_y = window_begin_y;

            // Quantize xpos and ypos
            const int quantized_x_int = int((double)xpos * quantize_x + 0.5);
            const int quantized_y_int = int((double)ypos * quantize_y + 0.5);
            const int quantized_x_value = quantized_x_int % quantize_x;
            const int quantized_y_value = quantized_y_int % quantize_y;
            const float quantized_xpos = float(quantized_x_int) / quantize_x;
            const float quantized_ypos = float(quantized_y_int) / quantize_y;

            if (!is_border && out->factor_map[quantized_y_value * quantize_x + quantized_x_value] != 0)
            {
                // Not border pixel and already have coefficient calculated at this quantized position
                meta->coeff_meta = out->factor_map[quantized_y_value * quantize_x + quantized_x_value] - 1;
            }
            else
            {
                // then need computation
                float divider = 0.f;

                // This is the location of current target pixel in source pixel
                // Qunatized
                const float current_x = clamp(is_border ? xpos : quantized_xpos, 0.f, src_width - 1.f);
                const float current_y = clamp(is_border ? ypos : quantized_ypos, 0.f, src_height - 1.f);

                if (!is_border)
                {
                    // Change window position to quantized position
                    window_begin_x = int(quantized_xpos + filter_support) - filter_size + 1;
                    window_begin_y = int(quantized_ypos + filter_support) - filter_size + 1;
                }

                // Windowing positon
                int window_x = window_begin_x;
                int window_y = window_begin_y;

                // First loop calcuate coeff
                tmp_array.resize(tmp_array.size() + coeff_per_pixel, 0.f);
                int curr_factor_ptr = tmp_array_top;

                const double radius2 = radius * radius;
                for (int ly = 0; ly < filter_size; ly++)
                {
                    for (int lx = 0; lx < filter_size; lx++)
                    {
                        // Euclidean distance to sampling pixel
                        const float dx = (current_x - window_x) * filter_step_x;
                        const float dy = (current_y - window_y) * filter_step_y;
                        const float dist = dx * dx + dy * dy;
                        double index_d = round((samples - 1) * dist / radius2) + DOUBLE_ROUND_MAGIC_NUMBER;
                        int index = *reinterpret_cast<int*>(&index_d);

                        const float factor = func->GetFactor(index);

                        tmp_array[curr_factor_ptr + lx] = factor;
                        divider += factor;

                        window_x++;
                    }

                    curr_factor_ptr += out->coeff_stripe;

                    window_x = window_begin_x;
                    window_y++;
                }

                // Second loop to divide the coeff
                curr_factor_ptr = tmp_array_top;
                for (int ly = 0; ly < filter_size; ly++)
                {
                    for (int lx = 0; lx < filter_size; lx++)
                    {
                        tmp_array[curr_factor_ptr + lx] /= divider;
                    }

                    curr_factor_ptr += out->coeff_stripe;
                }

                // Save factor to table
                if (!is_border)
                    out->factor_map[quantized_y_value * quantize_x + quantized_x_value] = tmp_array_top + 1;

                meta->coeff_meta = tmp_array_top;
                tmp_array_top += coeff_per_pixel;
            }

            xpos += x_step;
        }

        ypos += y_step;
        xpos = start_x;
    }

    // Copy from tmp_array to real array
    const int tmp_array_size = tmp_array.size();
    out->factor = (float *)vs_aligned_malloc(tmp_array_size * sizeof(float), 64); // aligned to cache line
    memcpy(out->factor, &tmp_array[0], tmp_array_size * sizeof(float));
}

/* Planar resampling with coeff table */
/* 8-16 bit */
//#pragma intel optimization_parameter target_arch=sse
template<typename T>
void resize_plane_c(EWAPixelCoeff* coeff, const T* srcp, T* VS_RESTRICT dstp,
    int dst_width, int dst_height, int src_stride, int dst_stride, int peak)
{
    EWAPixelCoeffMeta* meta = coeff->meta;

    for (int y = 0; y < dst_height; y++)
    {
        for (int x = 0; x < dst_width; x++)
        {
            const T* src_ptr = srcp + meta->start_y * src_stride + meta->start_x;
            const float* coeff_ptr = coeff->factor + meta->coeff_meta;

            float result = 0.f;
            for (int ly = 0; ly < coeff->filter_size; ly++)
            {
                for (int lx = 0; lx < coeff->filter_size; lx++)
                {
                    result += src_ptr[lx] * coeff_ptr[lx];
                }
                coeff_ptr += coeff->coeff_stripe;
                src_ptr += src_stride;
            }

            dstp[x] = static_cast<T>(clamp(result, 0.f, peak + 1.f));

            meta++;
        }

        dstp += dst_stride;
    }
}

/* Planar resampling with coeff table */
/* 32 bit */
template<typename T>
void resize_plane_c(EWAPixelCoeff* coeff, const T* srcp, T* VS_RESTRICT dstp,
    int dst_width, int dst_height, int src_stride, int dst_stride)
{
    EWAPixelCoeffMeta* meta = coeff->meta;

    for (int y = 0; y < dst_height; y++)
    {
        for (int x = 0; x < dst_width; x++)
        {
            const T* src_ptr = srcp + meta->start_y * src_stride + meta->start_x;
            const float* coeff_ptr = coeff->factor + meta->coeff_meta;

            float result = 0.f;
            for (int ly = 0; ly < coeff->filter_size; ly++)
            {
                for (int lx = 0; lx < coeff->filter_size; lx++)
                {
                    result += src_ptr[lx] * coeff_ptr[lx];
                }
                coeff_ptr += coeff->coeff_stripe;
                src_ptr += src_stride;
            }

            dstp[x] = clamp(result, -1.f, 1.f);

            meta++;
        }

        dstp += dst_stride;
    }
}

#endif
