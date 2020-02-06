project('JincResize', 'cpp',
  default_options : ['buildtype=release', 'b_ndebug=if-release', 'cpp_std=c++17'],
  meson_version : '>=0.48.0',
  version : '5'
)

sources = ['JincResize/JincResize.cpp']

vapoursynth_dep = dependency('vapoursynth').partial_dependency(compile_args : true, includes : true)

add_project_arguments(language : 'cpp')

shared_module('jincresize', sources,
  dependencies : vapoursynth_dep,
  include_directories : include_directories('JincResize/include'),
  cpp_args: ['-march=native'],
  install : true,
  install_dir : join_paths(vapoursynth_dep.get_pkgconfig_variable('libdir'), 'vapoursynth'),
  gnu_symbol_visibility : 'hidden'
)
