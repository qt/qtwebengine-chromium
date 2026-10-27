headers = files(
  'gdesktop-enums.h'
)

install_headers(headers, subdir: gsettings_desktop_schemas_name)

enums_xml = custom_target(
  'org.gnome.desktop.enums.xml',
  input: headers,
  output: 'org.gnome.desktop.enums.xml',
  command: [glib_mkenums,
            '--comments', '<!-- @comment@ -->',
            '--fhead', '<schemalist>',
            '--vhead', '<@type@ id="org.gnome.desktop.@EnumName@">',
            '--vprod', '    <value nick="@valuenick@" value="@valuenum@"/>',
            '--vtail', '  </@type@>',
            '--ftail', '</schemalist>', '@INPUT@'],
  capture: true,
  install: true,
  install_dir: schemasdir)

if get_option('introspection')
  gir_sources = headers

  # Use a dummy .c source for Visual Studio builds-it is not enough
  # to send in just a header file to build a dummy library on MSVC
  if cc.get_id() == 'msvc'
    gir_sources += ['dummy-msvc.c']
  endif

  noinst_lib = shared_library('noinst',
                              gir_sources,
                              install: false)

  gnome.generate_gir(noinst_lib,
    sources: headers,
    namespace: 'GDesktopEnums',
    identifier_prefix: 'GDesktop',
    nsversion : '3.0',
    install: true,
    extra_args: [
      '--header-only',
    ]
  )
endif
