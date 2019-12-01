# Description

JincResize works by Jinc algorithms (EWA Lanczos).

Modified from EWA-Resampling-VS:  https://github.com/Lypheo/EWA-Resampling-VS . Added the 8bit support.

I'm a beginner for C++. The plugin needs improvement. I will try to modify in the future.

# Usage

```python
core.jinc.JincResize(clip clip, int width, int height, int tap, float blur)
```

* *clip*: Clip to process,  integer sample type of 8-16 bit depth is supported.
* *width*: The width of output.
* *height*: The height of output.
* *tap*: Corresponding to different zeros of Jinc function (Range: 1–16). The recommended parameters is 3, 4, 6, 8, which is similar to the [AviSynth functions](https://github.com/AviSynth/jinc-resize),  ` Jinc36Resize `, ` Jinc64Resize `, ` Jinc128Resize `, ` Jinc256Resize `. *(Default: 3)*

# Compilation

The currently released dynamic link library is compiled in the following way.

```
x86_64-w64-mingw32-g++ -shared -o JincResize.dll -O2 -static JincResize.cpp
```

(`VapourSynth.h` and `VSHelper.h` need be in the same folder. You can get them from [here](https://github.com/vapoursynth/vapoursynth/tree/master/include).)

# Acknowledgement

Thanks to [Lypheo]( https://github.com/Lypheo ), the original developer of EWA-Resampling-VS. I know nothing of algorithm implementation, only make a little modification in the grammar. If you think the plugin is useful, make a star for his original [repositories]( https://github.com/Lypheo/EWA-Resampling-VS ).
