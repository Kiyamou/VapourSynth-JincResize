#include <cstdlib>
#include <cmath>
#include <algorithm>

#include <VapourSynth.h>
#include <VSHelper.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define JINC_ZERO 1.2196698912665045
#define M_PI      3.14159265358979323846

typedef struct {
	VSNodeRef* node;
	const VSVideoInfo* vi;
	int w;
	int h;
	int antiring;
	double radius;
	double blur;
	double* lut;
	int samples;
} FilterData;

static void VS_CC filterInit(VSMap* in, VSMap* out, void** instanceData, VSNode* node, VSCore* core, const VSAPI* vsapi) {
	FilterData* d = (FilterData*)* instanceData;
	VSVideoInfo new_vi = (VSVideoInfo) * (d->vi);
	new_vi.width = d->w;
	new_vi.height = d->h;
	vsapi->setVideoInfo(&new_vi, 1, node);
}

double jinc(double x) {
	if (fabs(x) < 1e-8)
		return 1.0;
	x *= M_PI;
	return 2.0 * j1(x) / x;
}

double sample(double (*filter)(double), double x, double blur, double radius) {
	x = fabs(x);
	x = blur > 0.0 ? x / blur : x;

	if (x < radius)
		return filter(x);

	return 0.0;
}

template<typename T>
static void process(const VSFrameRef* frame, VSFrameRef* dst, const FilterData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept {
	for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
		const T* framep = reinterpret_cast<const T*>(vsapi->getReadPtr(frame, plane));
		T* VS_RESTRICT dstp = reinterpret_cast<T*>(vsapi->getWritePtr(dst, plane));
		const VSFormat* fi = d->vi->format;
		int in_height = vsapi->getFrameHeight(frame, 0);
		int in_width = vsapi->getFrameWidth(frame, 0);

		int frame_stride = vsapi->getStride(frame, plane) / sizeof(T);
		int dst_stride = vsapi->getStride(dst, plane) / sizeof(T);

		int ih = vsapi->getFrameHeight(frame, plane);
		int iw = vsapi->getFrameWidth(frame, plane);
		int oh = d->h * ih / in_height;
		int ow = d->w * iw / in_width;

		double radius2 = pow(d->radius, 2);
		for (int y = 0; y < oh; y++) {
			for (int x = 0; x < ow; x++) {
				// reverse pixel mapping
				double rpm_x = (x + 0.5) * (iw) / (ow);
				double rpm_y = (y + 0.5) * (ih) / (oh);
				// who cares about border handling anyway ヽ( ﾟヮ・)ノ
				int window_x_lower = (int)std::max(ceil(rpm_x - d->radius + 0.5) - 1, 0.0);
				int window_x_upper = (int)MIN(floor(rpm_x + d->radius + 0.5) - 1, iw - 1);
				int window_y_lower = (int)std::max(ceil(rpm_y - d->radius + 0.5) - 1, 0.0);
				int window_y_upper = (int)MIN(floor(rpm_y + d->radius + 0.5) - 1, ih - 1);
				double pixel = 0;
				double normalizer = 0;

				for (int ewa_y = window_y_lower; ewa_y <= window_y_upper; ewa_y++) {
					for (int ewa_x = window_x_lower; ewa_x <= window_x_upper; ewa_x++) {
						double distance = pow(rpm_x - (ewa_x + 0.5), 2.0) + pow(rpm_y - (ewa_y + 0.5), 2.0);
						if (distance > radius2)
							continue;
						double weight = d->lut[(int)round((d->samples - 1) * distance / radius2)];
						normalizer += weight;
						T src_value = framep[ewa_x + ewa_y * frame_stride];
						pixel += weight * src_value;
					}
				}
				pixel = MAX(MIN(pixel / normalizer, (1 << d->vi->format->bitsPerSample) - 1), 0); // what is limited range ヽ( ﾟヮ・)ノ

				dstp[x + y * dst_stride] = (T)pixel;
			}
		}
	}
}

static const VSFrameRef* VS_CC filterGetFrame(int n, int activationReason, void** instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi) {
	FilterData* d = (FilterData*)* instanceData;

	if (activationReason == arInitial) {
		vsapi->requestFrameFilter(n, d->node, frameCtx);
	}
	else if (activationReason == arAllFramesReady) {
		const VSFrameRef* frame = vsapi->getFrameFilter(n, d->node, frameCtx);
		const VSFormat* fi = d->vi->format;
		VSFrameRef* dst = vsapi->newVideoFrame(fi, d->w, d->h, frame, core);

		if (d->vi->format->bytesPerSample == 1)
			process<uint8_t>(frame, dst, d, vsapi);
		else
			process<uint16_t>(frame, dst, d, vsapi);

		vsapi->freeFrame(frame);
		return dst;
	}

	return 0;
}

static void VS_CC filterFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
	FilterData* d = (FilterData*)instanceData;
	vsapi->freeNode(d->node);
	free(d->lut);
	free(d);
}

static void VS_CC filterCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi) {
	FilterData d;
	FilterData* data;
	int err;

	d.node = vsapi->propGetNode(in, "clip", 0, 0);
	d.vi = vsapi->getVideoInfo(d.node);
	d.w = (int)vsapi->propGetInt(in, "w", 0, &err);
	d.h = (int)vsapi->propGetInt(in, "h", 0, &err);

	//probably add an RGB check because subpixel shifting is :effort:

	d.antiring = (int)vsapi->propGetInt(in, "antiring", 0, &err);
	if (err)
		d.antiring = 0; // will implement once I learn how to sort arrays in C without segfaulting ヽ( ﾟヮ・)ノ

	d.radius = vsapi->propGetFloat(in, "radius", 0, &err);
	if (err)
		d.radius = 3.2383154841662362;

	d.blur = vsapi->propGetFloat(in, "blur", 0, &err);
	if (err)
		d.blur = 0.9812505644269356;

	if (d.w / d.vi->width < 1 || d.h / d.vi->height < 1) {
		double scale = std::min((double)d.vi->width / d.w, (double)d.vi->height / d.h); // an ellipse would be :effort:
		d.radius = d.radius * scale;
		d.blur = d.blur * scale;
	}

	d.samples = 1000;

	double* lut = (double*)malloc(sizeof(double) * d.samples);
	for (int i = 0; i < d.samples; ++i) {
		double filter = sample(jinc, d.radius * sqrt((double)i / (d.samples - 1)), d.blur, d.radius); // saving the sqrt during filtering
		double window = sample(jinc, JINC_ZERO * sqrt((double)i / (d.samples - 1)), 1, d.radius);
		lut[i] = filter * window;
	}
	d.lut = lut;

	data = (FilterData*)malloc(sizeof(d));
	*data = d;

	//FilterData* data = new FilterData{ d };  // no help for speed

	vsapi->createFilter(in, out, "Lanczos", filterInit, filterGetFrame, filterFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin* plugin) {
	configFunc("com.ewa.resampling", "ewa", "VapourSynth EWA resampling", VAPOURSYNTH_API_VERSION, 1, plugin);
	registerFunc("Lanczos", "clip:clip;w:int;h:int;radius:float:opt;blur:float:opt;antiring:int:opt", filterCreate, 0, plugin);
}
