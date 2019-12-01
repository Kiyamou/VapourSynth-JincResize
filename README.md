# Description

JincResize works by Jinc (EWA Lanczos) algorithms.

Modified from EWA-Resampling-VS: https://github.com/Lypheo/EWA-Resampling-VS. Added the 8bit support.

I'm a beginner for C++. The plugin needs improvement. I will try to modify in the future.

# Usage

```python
core.jinc.JincResize(clip clip, int width, int height, int tap, float blur)
```

* ***clip***
    * Required parameter.
    * Clip to process,  integer sample type of 8-16 bit depth is supported.
* ***width***
    * Required parameter.
    * The width of output.
* ***height***
    * Required parameter.
    * The height of output.
* ***tap***
    * Optional parameter. Range: 1–16. *Default: 3.*
    * Corresponding to different zero points of Jinc function.
    * The recommended value is 3, 4, 6, 8, which is similar to the [AviSynth functions](https://github.com/AviSynth/jinc-resize),  ` Jinc36Resize `, ` Jinc64Resize `, ` Jinc128Resize `, ` Jinc256Resize `.
* ***blur***
    * Optional parameter. *Default: 0.9812505644269356.*
    * Blur processing, it can reduce side effects.
    * To achieve blur, the value should less than 1.
    * If don't have relevant knowledge or experience, had better not modify the parameter.

# Compilation

The currently released dynamic link library is compiled in the following way.

```
x86_64-w64-mingw32-g++ -shared -o JincResize.dll -O2 -static JincResize.cpp
```

(`VapourSynth.h` and `VSHelper.h` need be in the same folder. You can get them from [here](https://github.com/vapoursynth/vapoursynth/tree/master/include).)

# Acknowledgement

Thanks to [Lypheo]( https://github.com/Lypheo ), the original developer of EWA-Resampling-VS. I know nothing of algorithm implementation, only make a little modification in the grammar. If you think the plugin is useful, make a star for his original [repositories]( https://github.com/Lypheo/EWA-Resampling-VS ).
