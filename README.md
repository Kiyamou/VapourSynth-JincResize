# VapourSynth-JincResize

[![Build Status](https://api.travis-ci.org/Kiyamou/VapourSynth-JincResize.svg?branch=master)](https://travis-ci.org/github/Kiyamou/VapourSynth-JincResize)

## Description

JincResize is a resizer plugin for VapourSynth, works by Jinc function and elliptical weighted averaging (EWA). Support 8-16 bit and 32 bit sample type. Support YUV color family.

The repo is ported from [AviSynth plugin](https://github.com/AviSynth/jinc-resize) and based on [EWA-Resampling-VS](https://github.com/Lypheo/EWA-Resampling-VS).

If want to learn more about Jinc, you can read the [post](https://zhuanlan.zhihu.com/p/103910606) (Simplified Chinese / 简体中文).

## Usage

```python
core.jinc.JincResize(clip clip, int width, int height[, int tap, float src_left, float src_top,
                     float src_width, float src_height, int quant_x, int quant_y, float blur])
```

* ***clip***
    * Required parameter.
    * Clip to process.
    * Integer sample type of 8-16 bit depth and float sample type of 32 bit depth is supported.
* ***width***
    * Required parameter.
    * The width of output.
* ***height***
    * Required parameter.
    * The height of output.
* ***tap***
    * Optional parameter. Range: 1–16. *Default: 3*.
    * Corresponding to different zero points of Jinc function.
    * The recommended value is 3, 4, 6, 8, which is similar to the [AviSynth plugin](https://github.com/AviSynth/jinc-resize),  ` Jinc36Resize `, ` Jinc64Resize `, ` Jinc128Resize `, ` Jinc256Resize `.
* ***src_left***
  * Optional parameter. *Default: 0.0*.
  * Cropping of the left edge respectively, in pixels, before resizing.
* ***src_top***
  * Optional parameter. *Default: 0.0*.
  * Cropping of the top edge respectively, in pixels, before resizing.
* ***src_width***
  * Optional parameter. *Default: the width of input*.
  * If  > 0, setting the width of the clip before resizing.
  * If <= 0, setting the cropping of the right edge respectively, before resizing.
* ***src_height***
  * Optional parameter. *Default: the height of input*.
  * If  > 0, setting the height of the clip before resizing.
  * If <= 0, setting the cropping of the bottom edge respectively, before resizing.
* ***quant_x***
* ***quant_y***
  * Optional parameter. *Default: 256*.
  * Controls sub-pixel quantization.
* ***blur***
    * Optional parameter. *Default: 0.9812505644269356*.
    * Blur processing, it can reduce side effects.
    * To achieve blur, the value should less than 1.
    * If don't have relevant knowledge or experience, had better not modify the parameter.

## Tips

JincResize will lead to ringing. A solution is to use de-ringing as post-processing, such as `HQDeringmod()` in [havsfunc](https://github.com/HomeOfVapourSynthEvolution/havsfunc). A simple example as follows.

```python
from vapoursynth import core
import havfunc as haf

# src is a 720p clip
dst = core.jinc.JincResize(src, 1920, 1080)
dst = haf.HQDeringmod(dst)
```

## Compilation

### Windows

```bash
x86_64-w64-mingw32-g++ -shared -static -std=c++17 -O3 -march=native JincResize.cpp -o JincResize.dll
```

`VapourSynth.h` and `VSHelper.h` is need. You can get them from [here](https://github.com/vapoursynth/vapoursynth/tree/master/include) or your VapourSynth installation directory (`VapourSynth/sdk/include/vapoursynth`).

### Linux

```bash
meson build
ninja -C build
```
or dircetly

```bash
g++ -shared -fPIC -std=c++17 -O3 -march=native JincResize.cpp -o JincResize.so
```

## Acknowledgement

EWA-Resampling-VS: https://github.com/Lypheo/EWA-Resampling-VS

AviSynth plugin: https://github.com/AviSynth/jinc-resize
