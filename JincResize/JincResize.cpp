#include <cmath>
#include <string>
#include <memory>

#if !defined(_MSC_VER)
#include "immintrin.h"
#endif

#include "vapoursynth/VapourSynth.h"
#include "vapoursynth/VSHelper.h"

#include "include/JincFunc.hpp"
#include "include/Helper.hpp"

constexpr double JINC_ZERO_SQR = 1.48759464366204680005356;
constexpr double DOUBLE_ROUND_MAGIC_NUMBER = 6755399441055744.0;

// Doesn't double precision overkill?

struct FilterData
{
    VSNodeRef* node;
    const VSVideoInfo* vi;
    int w;
    int h;
    int antiring;
    double radius;
    int tap;
    double blur;
    double* lut;
    int samples;
};

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

        int ih = vsapi->getFrameHeight(src, plane);
        int iw = vsapi->getFrameWidth(src, plane);
        int oh = vsapi->getFrameHeight(dst, plane);
        int ow = vsapi->getFrameWidth(dst, plane);

        double radius2 = d->radius * d->radius;
        for (int y = 0; y < oh; y++)
        {
            for (int x = 0; x < ow; x++)
            {
                // reverse pixel mapping
                double rpm_x = (x + 0.5) * (iw) / (ow);
                double rpm_y = (y + 0.5) * (ih) / (oh);
                // who cares about border handling anyway ヽ( ﾟヮ・)ノ
                int window_x_lower = (int)std::max(ceil(rpm_x - d->radius + 0.5) - 1, 0.0);
                int window_x_upper = (int)std::min(floor(rpm_x + d->radius + 0.5) - 1, iw - 1.0);
                int window_y_lower = (int)std::max(ceil(rpm_y - d->radius + 0.5) - 1, 0.0);
                int window_y_upper = (int)std::min(floor(rpm_y + d->radius + 0.5) - 1, ih - 1.0);
                double pixel = 0;
                double normalizer = 0;

                for (int ewa_y = window_y_lower; ewa_y <= window_y_upper; ewa_y++)
                {
                    for (int ewa_x = window_x_lower; ewa_x <= window_x_upper; ewa_x++)
                    {
                        double distance = (rpm_x - ewa_x + 0.5) * (rpm_x - ewa_x + 0.5) + (rpm_y - ewa_y + 0.5) * (rpm_y - ewa_y + 0.5);
                        if (distance > radius2)
                            continue;
                        double index = round((d->samples - 1) * distance / radius2) + DOUBLE_ROUND_MAGIC_NUMBER;
                        double weight = d->lut[*reinterpret_cast<int*>(&(index))];
                        normalizer += weight;
                        pixel += weight * srcp[ewa_x + ewa_y * src_stride];
                    }
                }

                if (d->vi->format->sampleType == stInteger)
                    pixel = clamp(pixel / normalizer, 0.0, (1 << d->vi->format->bitsPerSample) - 1.0); // what is limited range ヽ( ﾟヮ・)ノ
                else
                    pixel = clamp(pixel / normalizer, -1.0, 1.0);

                dstp[x + y * dst_stride] = (T)pixel;
            }
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
    d->w = int64ToIntS(vsapi->propGetInt(in, "w", 0, &err));
    d->h = int64ToIntS(vsapi->propGetInt(in, "h", 0, &err));

    //probably add an RGB check because subpixel shifting is :effort:
    try
    {
        if (!isConstantFormat(d->vi) ||
            (d->vi->format->sampleType == stInteger && d->vi->format->bitsPerSample > 16) ||
            (d->vi->format->sampleType == stFloat && d->vi->format->bitsPerSample != 32))
            throw std::string{ "only constant format 8-16 bit integer and 32 bits float input supported" };

        d->antiring = int64ToIntS(vsapi->propGetInt(in, "antiring", 0, &err));
        if (err)
            d->antiring = 0; // will implement once I learn how to sort arrays in C without segfaulting ヽ( ﾟヮ・)ノ

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

        d->samples = 1000;  // should be a multiple of 4
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
    }
    catch (const std::string & error)
    {
        vsapi->setError(out, ("JincResize: " + error).c_str());
        vsapi->freeNode(d->node);
        return;
    }

    vsapi->createFilter(in, out, "JincResize", filterInit, filterGetFrame, filterFree, fmParallel, 0, d.release(), core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin* plugin)
{
    configFunc("com.vapoursynth.jincresize", "jinc", "VapourSynth EWA resampling", VAPOURSYNTH_API_VERSION, 1, plugin);

    registerFunc("JincResize",
        "clip:clip;"
        "w:int;"
        "h:int;"
        "tap:int:opt;"
        "blur:float:opt;"
        "antiring:int:opt",
        filterCreate, 0, plugin);
}
