# Description

JincResize works by Jinc algorithms (EWA Lanczos).

Modified from EWA-Resampling-VS:  https://github.com/Lypheo/EWA-Resampling-VS . Added the 8bit support.

I'm a beginner for C++. The plugin is very slow now (slower than the original version). I will try to modify in the future.

# Usage

```python
core.ewa.Lanczos(clip clip, int width, int height, float radius, float blur)
```

* clip: Clip to process,  integer sample type of 8-16 bit depth is supported.

# Acknowledgement

Thanks to [Lypheo]( https://github.com/Lypheo ), the original developer of EWA-Resampling-VS. I know nothing of algorithm implementation, only make a little modification in the grammar. If you think the plugin is useful, make a star for his original [repositories]( https://github.com/Lypheo/EWA-Resampling-VS ).