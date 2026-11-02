# SPDX-FileCopyrightText: 2022 Endless OS Foundation, LLC
# SPDX-License-Identifier: LGPL-2.1-or-later

project('gvdb', 'c',
    version: '0.0',
    meson_version: '>=1.1',
)

cc = meson.get_compiler('c')

libgvdb_includes = include_directories('.')

# In order for a consumer of gvdb to build & link to a libgvdb static lib, the
# headers need to be exported separately, as they are not exposed by `link_with`.
# See https://github.com/mesonbuild/meson/issues/10543
gvdb_include = declare_dependency(
  include_directories: libgvdb_includes,
)
meson.override_dependency('gvdb-include', gvdb_include)

libgvdb_sources = files(
    'gvdb/gvdb-builder.c',
    'gvdb/gvdb-reader.c',
)

gvdb_dep = declare_dependency(
  sources: libgvdb_sources,
  include_directories: libgvdb_includes,
)
meson.override_dependency('gvdb', gvdb_dep)

if get_option('tests')
  subdir('gvdb/tests')
endif

if get_option('build-tools')
  gvdb_dump_sources = files(
      'gvdb/gvdb-reader.c',
      'tools/gvdb-dump.c',
  )

  glib_dep = dependency('glib-2.0')

  executable('gvdb-dump',
             sources: gvdb_dump_sources,
             dependencies: glib_dep,
             install: true)

  install_man('man/gvdb-dump.1')
endif
