project('libei', 'c',
  version: '1.4.0',
  license: 'MIT',
  default_options: [ 'c_std=gnu11', 'warning_level=2' ],
  meson_version: '>= 0.57.0')

libei_version = meson.project_version().split('.')
libei_version_major = libei_version[0].to_int()
libei_version_minor = libei_version[1].to_int()
if libei_version.length() > 2
    libei_version_patchlevel = libei_version[2].to_int()
else
    libei_version_patchlevel = 0
endif

# Our headers are in libei-1.0 until we hit 2.0 etc. libei's API is
# backwards-compatible until the major version bump.
libei_api_version = '1.0'
libei_api_dir = 'libei-@0@'.format(libei_api_version)

# We use the same soname across all our libraries and they track the project
# version. If we have ABI incompatible changes, bump the project major version.
soname = meson.project_version()

pkgconfig = import('pkgconfig')
fs = import('fs')

cc = meson.get_compiler('c')
cflags =[
    '-Wno-unused-parameter',
    '-Wmissing-prototypes',
    '-Wno-missing-field-initializers',
    '-Wstrict-prototypes',
    '-Wstrict-prototypes',
    '-Wlogical-op',
    '-Wpointer-arith',
    '-Wuninitialized',
    '-Winit-self',
    '-Wstrict-prototypes',
    '-Wimplicit-fallthrough',
    '-Wredundant-decls',
    '-Wincompatible-pointer-types',
    '-Wformat=2',
    '-Wformat-overflow=2',
    '-Wformat-signedness',
    '-Wformat-truncation=2',
    '-Wmissing-declarations',
    '-Wshift-overflow=2',
    '-Wstrict-overflow=2',
    '-Wswitch',
]

if cc.get_id() == 'clang'
    cflags += [
        # clang doesn't think just using _unref_ is a use of the variable
        #       _unref_(foo) *bar = something_that_gives_a_ref
        # but we make heavy use of that in the test suite for convenience
        # of events we know must exist but we don't care about specifically
        '-Wno-unused-variable',
    ]
endif

add_project_arguments(cc.get_supported_arguments(cflags), language: 'c')

inc_builddir = include_directories('.')
inc_src = include_directories('src')

config_h = configuration_data()
config_h.set('_GNU_SOURCE', '1')
config_h.set_quoted('EI_VERSION', meson.project_version())
config_h.set_quoted('EIS_VERSION', meson.project_version())
if cc.has_function('memfd_create', prefix: '#define _GNU_SOURCE\n#include <sys/mman.h>')
  config_h.set10('HAVE_MEMFD_CREATE', true)
endif

dep_math = cc.find_library('m', required: false)
dep_epoll = dependency('epoll-shim', required: false)
dep_libxkbcommon = dependency('xkbcommon', required: false)
config_h.set10('HAVE_LIBXKBCOMMON', dep_libxkbcommon.found())
dep_libevdev = dependency('libevdev', required: false)
config_h.set10('HAVE_LIBEVDEV', dep_libevdev.found())

if not get_option('liboeffis').disabled()
    sd_bus_provider = get_option('sd-bus-provider')
    if sd_bus_provider == 'auto'
        message('Trying to find an sd-bus provider...')
        providers = ['libsystemd', 'libelogind', 'basu']
        foreach provider: providers
            if dependency(provider, required: false).found()
                message('Using sd-bus provider "@0@"'.format(provider))
                sd_bus_provider = provider
                break
            endif
        endforeach
        if sd_bus_provider  == 'auto'
            error('Failed to find an sd-bus provider, tried @0@'.format(', '.join(providers)))
        endif
        summary({'sd-bus-provider': sd_bus_provider}, section: 'Conditional Features')
    endif
    dep_sdbus = dependency(sd_bus_provider, required: get_option('liboeffis'))
else
    dep_sdbus = dependency('', required: false)
endif

build_oeffis = dep_sdbus.found()
build_libeis = not get_option('libeis').disabled()
build_libei = not get_option('libei').disabled()

config_h.set10('HAVE_LIBSYSTEMD', dep_sdbus.found() and dep_sdbus.name() == 'libsystemd')
config_h.set10('HAVE_LIBELOGIND', dep_sdbus.found() and dep_sdbus.name() == 'libelogind')
config_h.set10('HAVE_BASU', dep_sdbus.found() and dep_sdbus.name() == 'basu')

configure_file(output: 'config.h', configuration: config_h)

subdir('proto')
subdir('src')
subdir('tools')
subdir('test')
subdir('doc')

ruff = find_program('ruff', required: false)
if ruff.found()
    test('python-ruff', ruff,
         args: ['check', '--ignore=E741,E501', meson.project_source_root(), meson.project_source_root() / 'proto' / 'ei-scanner'],
         suite: 'python',
    )
    test('python-ruff-format', ruff,
         args: ['format', meson.project_source_root(), meson.project_source_root() / 'proto' / 'ei-scanner'],
         suite: 'python',
    )
endif
