#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <string>

#include <memory>

#include "VapourSynth.h"
#include "VSHelper.h"

constexpr double JINC_ZERO_SQR = 1.48759464366204680005356;
#ifndef M_PI // GCC seems to have it
constexpr double M_PI = 3.14159265358979323846;
#endif

// Doesn't double precision overkill?

// Taylor series coefficients of 2*BesselJ1(pi*x)/(pi*x) as (x^2) -> 0
static double jinc_taylor_series[31] = {
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

static double jinc_zeros[16] = {
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

// jinc(sqrt(x2))
double jinc_sqr(double x2) {
	if (x2 < 1.49) {
		double res = 0.0;
		for (auto j = 16; j > 0; --j)
			res = res * x2 + jinc_taylor_series[j - 1];
		return res;
	}
	else if (x2 < 4.97) {
		double res = 0.0;
		for (auto j = 21; j > 0; --j)
			res = res * x2 + jinc_taylor_series[j - 1];
		return res;
	}
	else if (x2 < 10.49) { // the 3-tap radius
		double res = 0.0;
		for (auto j = 26; j > 0; --j)
			res = res * x2 + jinc_taylor_series[j - 1];
		return res;
	}
	else if (x2 < 17.99) { // the 4-tap radius
		double res = 0.0;
		for (auto j = 31; j > 0; --j)
			res = res * x2 + jinc_taylor_series[j - 1];
		return res;
	}
	else {
		auto x = M_PI * std::sqrt(x2);
#if defined(_MSC_VER) // maybe not necessary
		return 2.0 * j1(x) / x;
#else
		return 2.0 * std::cyl_bessel_j(1, x) / x;
#endif
	}
}

double sample_sqr(double (*filter)(double), double x2, double blur2, double radius2) {
	if (blur2 > 0.0)
		x2 /= blur2;
	if (x2 < radius2)
		return filter(x2);
	return 0.0;
}

typedef struct {
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
} FilterData;

static void VS_CC filterInit(VSMap* in, VSMap* out, void** instanceData, VSNode* node, VSCore* core, const VSAPI* vsapi) {
	FilterData* d = static_cast<FilterData*>(*instanceData);
	VSVideoInfo new_vi = (VSVideoInfo) * (d->vi);
	new_vi.width = d->w;
	new_vi.height = d->h;
	vsapi->setVideoInfo(&new_vi, 1, node);
}

template<typename T>
static void process(const VSFrameRef* frame, VSFrameRef* dst, const FilterData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept {
	for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
		const T* framep = reinterpret_cast<const T*>(vsapi->getReadPtr(frame, plane));
		T* VS_RESTRICT dstp = reinterpret_cast<T*>(vsapi->getWritePtr(dst, plane));
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
				int window_x_upper = (int)std::min(floor(rpm_x + d->radius + 0.5) - 1, iw - 1.0);
				int window_y_lower = (int)std::max(ceil(rpm_y - d->radius + 0.5) - 1, 0.0);
				int window_y_upper = (int)std::min(floor(rpm_y + d->radius + 0.5) - 1, ih - 1.0);
				double pixel = 0;
				double normalizer = 0;

				for (int ewa_y = window_y_lower; ewa_y <= window_y_upper; ewa_y++) {
					for (int ewa_x = window_x_lower; ewa_x <= window_x_upper; ewa_x++) {
						double distance = pow(rpm_x - (ewa_x + 0.5), 2.0) + pow(rpm_y - (ewa_y + 0.5), 2.0);
						if (distance > radius2)
							continue;
						double index = round((d->samples - 1) * distance / radius2) + 6755399441055744.0;  // Magic number for "double to int"
						double weight = d->lut[*reinterpret_cast<int*>(&(index))];
						normalizer += weight;
						pixel += weight * (double)framep[ewa_x + ewa_y * frame_stride];
					}
				}

				if (d->vi->format->sampleType == stInteger)
					pixel = std::max(std::min(pixel / normalizer, (1 << d->vi->format->bitsPerSample) - 1.0), 0.0); // what is limited range ヽ( ﾟヮ・)ノ
				else
					pixel = std::max(std::min(pixel / normalizer, 1.0), -1.0);

				dstp[x + y * dst_stride] = (T)pixel;
			}
		}
	}
}

static const VSFrameRef* VS_CC filterGetFrame(int n, int activationReason, void** instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi) {
	const FilterData* d = static_cast<const FilterData*>(*instanceData);

	if (activationReason == arInitial) {
		vsapi->requestFrameFilter(n, d->node, frameCtx);
	}
	else if (activationReason == arAllFramesReady) {
		const VSFrameRef* frame = vsapi->getFrameFilter(n, d->node, frameCtx);
		const VSFormat* fi = d->vi->format;
		VSFrameRef* dst = vsapi->newVideoFrame(fi, d->w, d->h, frame, core);

		if (d->vi->format->bytesPerSample == 1)
			process<uint8_t>(frame, dst, d, vsapi);
		else if (d->vi->format->bytesPerSample == 2)
			process<uint16_t>(frame, dst, d, vsapi);
		else
			process<float>(frame, dst, d, vsapi);

		vsapi->freeFrame(frame);
		return dst;
	}

	return 0;
}

static void VS_CC filterFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
	FilterData* d = static_cast<FilterData*>(instanceData);
	vsapi->freeNode(d->node);
	delete[] d->lut;
	delete d;
}

static void VS_CC filterCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi) {
	std::unique_ptr<FilterData> d = std::make_unique<FilterData>();
	int err;

	d->node = vsapi->propGetNode(in, "clip", 0, 0);
	d->vi = vsapi->getVideoInfo(d->node);
	d->w = int64ToIntS(vsapi->propGetInt(in, "w", 0, &err));
	d->h = int64ToIntS(vsapi->propGetInt(in, "h", 0, &err));

	//probably add an RGB check because subpixel shifting is :effort:
	try {
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

		if (d->w / d->vi->width < 1 || d->h / d->vi->height < 1) {
			double scale = std::min((double)d->vi->width / d->w, (double)d->vi->height / d->h); // an ellipse would be :effort:
			d->radius = d->radius * scale;
			d->blur = d->blur * scale;
		}

		d->samples = 1000;

		double* lut = new double[d->samples];
		double radius2 = d->radius * d->radius;
		for (auto i = 0; i < d->samples; ++i) {
			double t2 = i / (d->samples - 1.0);
			double filter = sample_sqr(jinc_sqr, radius2 * t2, d->blur * d->blur, radius2);
			double window = sample_sqr(jinc_sqr, JINC_ZERO_SQR * t2, 1.0, radius2);
			lut[i] = filter * window;
		}
		d->lut = lut;
	}
	catch (const std::string & error) {
		vsapi->setError(out, ("JincResize: " + error).c_str());
		vsapi->freeNode(d->node);
		return;
	}

	vsapi->createFilter(in, out, "JincResize", filterInit, filterGetFrame, filterFree, fmParallel, 0, d.release(), core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin* plugin) {
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
