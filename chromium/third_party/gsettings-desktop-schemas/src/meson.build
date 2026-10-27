project(
  'gsettings-desktop-schemas', 'c',
  version: '48.0',
  default_options: [
    'warning_level=2'
  ],
  license: 'LGPL',
  meson_version: '>= 0.57.0'
)

gsettings_desktop_schemas_name = meson.project_name()
gsettings_desktop_schemas_version = meson.project_version()

# Make sure the version always has a trailing N.0 so that
# it can be sorted properly when comparing to N.alpha and so on.
#
# See https://gitlab.gnome.org/GNOME/gsettings-desktop-schemas/-/issues/66
assert(meson.project_version().split('.').length() > 1)

message('Building ' + gsettings_desktop_schemas_name + ' ' + gsettings_desktop_schemas_version)

cc = meson.get_compiler('c')
host_system = host_machine.system()
arch = host_machine.cpu_family()

prefix = get_option('prefix')
bindir = join_paths(prefix, get_option('bindir'))
libdir = join_paths(prefix, get_option('libdir'))
libexecdir = join_paths(prefix, get_option('libexecdir'))
includedir = join_paths(prefix, get_option('includedir'))
datadir = join_paths(prefix, get_option('datadir'))
sysconfdir = join_paths(prefix, get_option('sysconfdir'))
schemasdir = join_paths(datadir, 'glib-2.0/schemas')

gnome = import('gnome')

# Generate pc file
# Can't use pkgconfig helper https://github.com/mesonbuild/meson/issues/2253
conf = configuration_data()
conf.set('prefix', prefix)
conf.set('VERSION', meson.project_version())
conf.set('includedir', includedir)

pkg = configure_file(configuration: conf,
                     input: 'gsettings-desktop-schemas.pc.in',
                     install: true,
                     install_dir: join_paths(datadir, 'pkgconfig'),
                     output: 'gsettings-desktop-schemas.pc')

glib_mkenums = find_program('glib-mkenums')

subdir('headers')
subdir('schemas')
subdir('po')

gnome.post_install(
  glib_compile_schemas: true,
)
