#include <string>
#include <memory>

#include "../include/EWAResizer.hpp"

struct FilterData
{
    VSNodeRef* node;
    const VSVideoInfo* vi;
    int w, h;
    int peak;
    Lut* init_lut;
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
static void process_uint(const VSFrameRef* src, VSFrameRef* dst, const FilterData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept
{
    for (int plane = 0; plane < d->vi->format->numPlanes; plane++)
    {
        const T* srcp = reinterpret_cast<const T*>(vsapi->getReadPtr(src, plane));
        T* VS_RESTRICT dstp = reinterpret_cast<T*>(vsapi->getWritePtr(dst, plane));
        int src_stride = vsapi->getStride(src, plane) / sizeof(T);
        int dst_stride = vsapi->getStride(dst, plane) / sizeof(T);

        int dst_width = vsapi->getFrameWidth(dst, plane);
        int dst_height = vsapi->getFrameHeight(dst, plane);

        if (plane == 0)
            resize_plane_c(d->out_y, srcp, dstp, dst_width, dst_height, src_stride, dst_stride, d->peak);
        else if (plane == 1)
            resize_plane_c(d->out_u, srcp, dstp, dst_width, dst_height, src_stride, dst_stride, d->peak);
        else if (plane == 2)
            resize_plane_c(d->out_v, srcp, dstp, dst_width, dst_height, src_stride, dst_stride, d->peak);
    }
}

static void process_float(const VSFrameRef* src, VSFrameRef* dst, const FilterData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept
{
    for (int plane = 0; plane < d->vi->format->numPlanes; plane++)
    {
        const float* srcp = reinterpret_cast<const float*>(vsapi->getReadPtr(src, plane));
        float* VS_RESTRICT dstp = reinterpret_cast<float*>(vsapi->getWritePtr(dst, plane));
        int src_stride = vsapi->getStride(src, plane) / sizeof(float);
        int dst_stride = vsapi->getStride(dst, plane) / sizeof(float);

        int dst_width = vsapi->getFrameWidth(dst, plane);
        int dst_height = vsapi->getFrameHeight(dst, plane);

#if defined(USE_AVX2)
        if (plane == 0)
            resize_plane_avx2(d->out_y, srcp, dstp, dst_width, dst_height, src_stride, dst_stride);
        else if (plane == 1)
            resize_plane_avx2(d->out_u, srcp, dstp, dst_width, dst_height, src_stride, dst_stride);
        else if (plane == 2)
             resize_plane_avx2(d->out_v, srcp, dstp, dst_width, dst_height, src_stride, dst_stride);
#else
        if (plane == 0)
            resize_plane_c(d->out_y, srcp, dstp, dst_width, dst_height, src_stride, dst_stride);
        else if (plane == 1)
            resize_plane_c(d->out_u, srcp, dstp, dst_width, dst_height, src_stride, dst_stride);
        else if (plane == 2)
            resize_plane_c(d->out_v, srcp, dstp, dst_width, dst_height, src_stride, dst_stride);
#endif
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
            process_uint<uint8_t>(src, dst, d, vsapi);
        else if (d->vi->format->bytesPerSample == 2)
            process_uint<uint16_t>(src, dst, d, vsapi);
        else
            process_float(src, dst, d, vsapi);

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

    d->init_lut->DestroyLutTable();
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

        int tap = int64ToIntS(vsapi->propGetInt(in, "tap", 0, &err));
        if (err)
            tap = 3;

        if (tap < 1 || tap > 16)
            throw std::string{ "tap must be in the range of 1-16" };

        double radius = jinc_zeros[tap - 1];

        double blur = vsapi->propGetFloat(in, "blur", 0, &err);
        if (err)
            blur = 0.9812505644269356;

        double crop_left = vsapi->propGetFloat(in, "src_left", 0, &err);
        if (err)
            crop_left = 0.0;

        double crop_top = vsapi->propGetFloat(in, "src_top", 0, &err);
        if (err)
            crop_top = 0.0;

        double crop_width = vsapi->propGetFloat(in, "src_width", 0, &err);
        if (err)
            crop_width = static_cast<double>(d->vi->width);

        double crop_height = vsapi->propGetFloat(in, "src_height", 0, &err);
        if (err)
            crop_height = static_cast<double>(d->vi->height);

        int samples = 1024;  // should be a multiple of 4

        int quantize_x = int64ToIntS(vsapi->propGetInt(in, "quant_x", 0, &err));
        if (err)
            quantize_x = 256;
        int quantize_y = int64ToIntS(vsapi->propGetInt(in, "quant_y", 0, &err));
        if (err)
            quantize_y = 256;

        d->init_lut = new Lut();
        d->init_lut->InitLut(samples, radius, blur);
        d->out_y = new EWAPixelCoeff();

        generate_coeff_table_c(d->init_lut, d->out_y, quantize_x, quantize_y, samples, d->vi->width, d->vi->height,
            d->w, d->h, radius, crop_left, crop_top, crop_width, crop_height);

        if (d->vi->format->numPlanes > 1)
        {
            d->out_u = new EWAPixelCoeff();
            d->out_v = new EWAPixelCoeff();

            int sub_w = d->vi->format->subSamplingW;
            int sub_h = d->vi->format->subSamplingH;
            double div_w = static_cast<double>(1 << sub_w);
            double div_h = static_cast<double>(1 << sub_h);

            generate_coeff_table_c(d->init_lut, d->out_u, quantize_x, quantize_y, samples, d->vi->width >> sub_w, d->vi->height >> sub_h,
                d->w >> sub_w, d->h >> sub_h, radius, crop_left / div_w, crop_top / div_h, crop_width / div_w, crop_height / div_h);
            generate_coeff_table_c(d->init_lut, d->out_v, quantize_x, quantize_y, samples, d->vi->width >> sub_w, d->vi->height >> sub_h,
                d->w >> sub_w, d->h >> sub_h, radius, crop_left / div_w, crop_top / div_h, crop_width / div_w, crop_height / div_h);
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
        "src_left:float:opt;"
        "src_top:float:opt;"
        "src_width:float:opt;"
        "src_height:float:opt;"
        "quant_x:int:opt;"
        "quant_y:int:opt;"
        "blur:float:opt",
        filterCreate, 0, plugin);
}
