#include <string>
#include <memory>

#if !defined(_MSC_VER)
#include "immintrin.h"
#endif

#include "../include/JincFunc.hpp"
#include "../include/EWAResizer.hpp"

constexpr double JINC_ZERO_SQR = 1.48759464366204680005356;

struct FilterData
{
    VSNodeRef* node;
    const VSVideoInfo* vi;
    int w, h;
    int peak;
    double radius;
    int tap;
    double blur;
    double* lut;
    int samples;
    EWAPixelCoeff* out_y;
    EWAPixelCoeff* out_u;
    EWAPixelCoeff* out_v;
};

// Doesn't double precision overkill?

static void VS_CC filterInit(VSMap* in, VSMap* out, void** instanceData, VSNode* node, VSCore* core, const VSAPI* vsapi)
{
    FilterData* d = static_cast<FilterData*>(*instanceData);
    VSVideoInfo new_vi = (VSVideoInfo) * (d->vi);
    new_vi.width = d->w;
    new_vi.height = d->h;
    vsapi->setVideoInfo(&new_vi, 1, node);
}

template<typename T>
static void process(const VSFrameRef* src, VSFrameRef* dst, const FilterData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept
{
    for (int plane = 0; plane < d->vi->format->numPlanes; plane++)
    {
        const T* srcp = reinterpret_cast<const T*>(vsapi->getReadPtr(src, plane));
        T* VS_RESTRICT dstp = reinterpret_cast<T*>(vsapi->getWritePtr(dst, plane));
        int src_stride = vsapi->getStride(src, plane) / sizeof(T);
        int dst_stride = vsapi->getStride(dst, plane) / sizeof(T);

        int dst_width = vsapi->getFrameWidth(dst, plane);
        int dst_height = vsapi->getFrameHeight(dst, plane);

        if (d->vi->format->bytesPerSample <= 2)
        {
            if (plane == 0)
                resize_plane_c(d->out_y, srcp, dstp, dst_width, dst_height, src_stride, dst_stride, d->peak);
            else if (plane == 1)
                resize_plane_c(d->out_u, srcp, dstp, dst_width, dst_height, src_stride, dst_stride, d->peak);
            else if (plane == 2)
                resize_plane_c(d->out_v, srcp, dstp, dst_width, dst_height, src_stride, dst_stride, d->peak);
        }
        else
        {
            if (plane == 0)
                resize_plane_c(d->out_y, srcp, dstp, dst_width, dst_height, src_stride, dst_stride, 65536); // any number more than 65535 is OK
            else if (plane == 1)
                resize_plane_c(d->out_u, srcp, dstp, dst_width, dst_height, src_stride, dst_stride, 65536); // only distinguish 32bit and 8-16bit
            else if (plane == 2)
                resize_plane_c(d->out_v, srcp, dstp, dst_width, dst_height, src_stride, dst_stride, 65536);
        }
    }
}

static const VSFrameRef* VS_CC filterGetFrame(int n, int activationReason, void** instanceData,
    void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    const FilterData* d = static_cast<const FilterData*>(*instanceData);

    if (activationReason == arInitial)
    {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    }
    else if (activationReason == arAllFramesReady)
    {
        const VSFrameRef* src = vsapi->getFrameFilter(n, d->node, frameCtx);
        VSFrameRef* dst = vsapi->newVideoFrame(d->vi->format, d->w, d->h, src, core);

        if (d->vi->format->bytesPerSample == 1)
            process<uint8_t>(src, dst, d, vsapi);
        else if (d->vi->format->bytesPerSample == 2)
            process<uint16_t>(src, dst, d, vsapi);
        else
            process<float>(src, dst, d, vsapi);

        vsapi->freeFrame(src);
        return dst;
    }

    return 0;
}

static void VS_CC filterFree(void* instanceData, VSCore* core, const VSAPI* vsapi)
{
    FilterData* d = static_cast<FilterData*>(instanceData);
    vsapi->freeNode(d->node);

    delete_coeff_table(d->out_y);
    if (d->vi->format->numPlanes > 1)
    {
        delete_coeff_table(d->out_u);
        delete_coeff_table(d->out_v);
    }

#if defined(_MSC_VER)
    delete[] d->lut;
#else
    _mm_free(d->lut);
#endif
    delete d;
}

static void VS_CC filterCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
{
    std::unique_ptr<FilterData> d = std::make_unique<FilterData>();
    int err;

    d->node = vsapi->propGetNode(in, "clip", 0, 0);
    d->vi = vsapi->getVideoInfo(d->node);
    d->w = int64ToIntS(vsapi->propGetInt(in, "width", 0, &err));
    d->h = int64ToIntS(vsapi->propGetInt(in, "height", 0, &err));

    if (d->vi->format->bytesPerSample <= 2)
        d->peak = (1 << d->vi->format->bitsPerSample) - 1;

    //probably add an RGB check because subpixel shifting is :effort:
    try
    {
        if (!isConstantFormat(d->vi) ||
            (d->vi->format->sampleType == stInteger && d->vi->format->bitsPerSample > 16) ||
            (d->vi->format->sampleType == stFloat && d->vi->format->bitsPerSample != 32))
            throw std::string{ "only constant format 8-16 bit integer and 32 bits float input supported" };

        d->tap = int64ToIntS(vsapi->propGetInt(in, "tap", 0, &err));
        if (err)
            d->tap = 3;

        if (d->tap < 1 || d->tap > 16)
            throw std::string{ "tap must be in the range of 1-16" };

        d->radius = jinc_zeros[d->tap - 1];

        d->blur = vsapi->propGetFloat(in, "blur", 0, &err);
        if (err)
            d->blur = 0.9812505644269356;

        if (d->w / d->vi->width < 1 || d->h / d->vi->height < 1)
        {
            double scale = std::min((double)d->vi->width / d->w, (double)d->vi->height / d->h); // an ellipse would be :effort:
            d->radius = d->radius * scale;
            d->blur = d->blur * scale;
        }

        double crop_top = vsapi->propGetFloat(in, "crop_top", 0, &err);
        if (err)
            crop_top = 0.0;

        double crop_left = vsapi->propGetFloat(in, "crop_left", 0, &err);
        if (err)
            crop_left = 0.0;

        double crop_width = vsapi->propGetFloat(in, "crop_width", 0, &err);
        if (err)
            crop_width = (double)d->vi->width;

        double crop_height = vsapi->propGetFloat(in, "crop_height", 0, &err);
        if (err)
            crop_height = (double)d->vi->height;

        d->samples = 1024;  // should be a multiple of 4
#if defined(_MSC_VER)
        double* lut = new double[d->samples];
        double radius2 = d->radius * d->radius;
        double blur2 = d->blur * d->blur;
        for (auto i = 0; i < d->samples; ++i)
        {
            double t2 = i / (d->samples - 1.0);
            double filter = sample_sqr(jinc_sqr, radius2 * t2, blur2, radius2);
            double window = sample_sqr(jinc_sqr, JINC_ZERO_SQR * t2, 1.0, radius2);
            lut[i] = filter * window;
        }
#else
        double* lut = (double *)_mm_malloc(sizeof(double) * d->samples, 64);
        double* filters = (double *)_mm_malloc(sizeof(double) * d->samples, 64);
        double* windows = (double *)_mm_malloc(sizeof(double) * d->samples, 64);
        auto radius2 = d->radius * d->radius;
        auto blur2 = d->blur * d->blur;
        for (auto i = 0; i < d->samples; i++)
        {
            auto t2 = i / (d->samples - 1.0);
            filters[i] = sample_sqr(jinc_sqr, radius2 * t2, blur2, radius2);
            windows[i] = sample_sqr(jinc_sqr, JINC_ZERO_SQR * t2, 1.0, radius2);
        }
        for (auto j = 0; j < d->samples / 4; ++j)
        {
            auto rf = _mm256_load_pd(filters + j * 4);
            auto rw = _mm256_load_pd(windows + j * 4);
            auto rl = _mm256_mul_pd(rf, rw);
            _mm256_store_pd(lut + j * 4, rl);
        }
        _mm_free(filters);
        _mm_free(windows);
#endif
        d->lut = lut;

        int quantize_x = 256;
        int quantize_y = 256;

        d->out_y = new EWAPixelCoeff();

        generate_coeff_table_c(d->lut, d->out_y, quantize_x, quantize_y, d->vi->width, d->vi->height,
            d->w, d->h, d->radius, crop_left, crop_top, crop_width, crop_height);

        if (d->vi->format->numPlanes > 1)
        {
            d->out_u = new EWAPixelCoeff();
            d->out_v = new EWAPixelCoeff();

            int width_uv = d->vi->format->subSamplingW;
            int height_uv = d->vi->format->subSamplingH;
            double div_w = 1 << width_uv;
            double div_h = 1 << height_uv;

            generate_coeff_table_c(d->lut, d->out_u, quantize_x, quantize_y, d->vi->width >> width_uv, d->vi->height >> height_uv,
                d->w >> width_uv, d->h >> height_uv, d->radius, crop_left / div_w, crop_top / div_h, crop_width / div_w, crop_height / div_h);
            generate_coeff_table_c(d->lut, d->out_v, quantize_x, quantize_y, d->vi->width >> width_uv, d->vi->height >> height_uv,
                d->w >> width_uv, d->h >> height_uv, d->radius, crop_left / div_w, crop_top / div_h, crop_width / div_w, crop_height / div_h);
        }
    }
    catch (const std::string & error)
    {
        vsapi->setError(out, ("JincResize: " + error).c_str());
        vsapi->freeNode(d->node);
        return;
    }

    vsapi->createFilter(in, out, "JincResize", filterInit, filterGetFrame, filterFree, fmParallel, 0, d.release(), core);
}

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin* plugin)
{
    configFunc("com.vapoursynth.jincresize", "jinc", "VapourSynth EWA resampling", VAPOURSYNTH_API_VERSION, 1, plugin);

    registerFunc("JincResize",
        "clip:clip;"
        "width:int;"
        "height:int;"
        "tap:int:opt;"
        "crop_top:float:opt;"
        "crop_left:float:opt;"
        "crop_width:float:opt;"
        "crop_height:float:opt;"
        "blur:float:opt",
        filterCreate, 0, plugin);
}
