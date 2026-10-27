src_libutil = files(
    'util-bits.c',
    'util-io.c',
    'util-list.c',
    'util-logger.c',
    'util-memfile.c',
    'util-memmap.c',
    'util-sources.c',
    'util-strings.c',
)

lib_util = static_library('util',
    src_libutil,
    include_directories: [inc_builddir],
    dependencies: [dep_math, dep_epoll],
    gnu_symbol_visibility: 'hidden',
)

dep_libutil = declare_dependency(link_with: lib_util)

proto_c_template = files('ei-proto.c.tmpl')
proto_h_template = files('ei-proto.h.tmpl')
brei_proto_h_template = files('brei-proto.h.tmpl')

brei_proto_headers = custom_target('brei-proto-headers',
                                   input: protocol_xml,
                                   output: ['brei-proto.h'],
                                   command: [scanner, '--component=brei', '--output=@OUTPUT@', '@INPUT@', brei_proto_h_template])

if build_libei
    ei_proto_headers = custom_target('ei-proto-headers',
                                     input: protocol_xml,
                                     output: ['ei-proto.h'],
                                     command: [scanner, '--component=ei', '--output=@OUTPUT@', '@INPUT@', proto_h_template])
    ei_proto_sources = custom_target('ei-proto-sources',
                                     input: protocol_xml,
                                     output: ['ei-proto.c'],
                                     command: [scanner, '--component=ei', '--output=@OUTPUT@', '@INPUT@',
                                               '--jinja-extra-data={"headerfile": "ei-proto.h"}', proto_c_template])
    src_libei = files(
        'brei-shared.c',
        'libei.c',
        'libei-button.c',
        'libei-callback.c',
        'libei-connection.c',
        'libei-device.c',
        'libei-event.c',
        'libei-fd.c',
        'libei-handshake.c',
        'libei-keyboard.c',
        'libei-log.c',
        'libei-pingpong.c',
        'libei-ping.c',
        'libei-pointer-absolute.c',
        'libei-pointer.c',
        'libei-region.c',
        'libei-region.c',
        'libei-scroll.c',
        'libei-seat.c',
        'libei-socket.c',
        'libei-touchscreen.c',
    ) + [brei_proto_headers, ei_proto_headers, ei_proto_sources]

    deps_libei = [
        dep_libutil,
    ]

    lib_libei = library('ei',
        src_libei,
        dependencies: deps_libei,
        include_directories: [inc_builddir],
        gnu_symbol_visibility: 'hidden',
        version: soname,
        install: true
    )
    libei_headers = files('libei.h')
    install_headers(libei_headers, subdir: libei_api_dir)

    dep_libei = declare_dependency(link_with: lib_libei,
                                    include_directories: [inc_src])
    meson.override_dependency('libei', dep_libei)

    pkgconfig.generate(lib_libei,
        filebase: 'libei-@0@'.format(libei_api_version),
        name: 'libEI',
        description: 'Emulated Input client library',
        subdirs: libei_api_dir,
        version: meson.project_version(),
        libraries: lib_libei,
    )
endif

if build_libeis
    eis_proto_headers = custom_target('eis-proto-headers',
                                      input: protocol_xml,
                                      output: ['eis-proto.h'],
                                      command: [scanner, '--component=eis', '--output=@OUTPUT@', '@INPUT@', proto_h_template])
    eis_proto_sources = custom_target('eis-proto-sources',
                                      input: protocol_xml,
                                      output: ['eis-proto.c'],
                                      command: [scanner, '--component=eis', '--output=@OUTPUT@', '@INPUT@',
                                               '--jinja-extra-data={"headerfile": "eis-proto.h"}', proto_c_template])

    src_libeis = files(
        'brei-shared.c',
        'libeis-button.c',
        'libeis-callback.c',
        'libeis-client.c',
        'libeis-connection.c',
        'libeis-device.c',
        'libeis-event.c',
        'libeis-fd.c',
        'libeis-handshake.c',
        'libeis-keyboard.c',
        'libeis-log.c',
        'libeis-pingpong.c',
        'libeis-ping.c',
        'libeis-pointer-absolute.c',
        'libeis-pointer.c',
        'libeis-region.c',
        'libeis-scroll.c',
        'libeis-seat.c',
        'libeis-socket.c',
        'libeis-touchscreen.c',
        'libeis.c',
    ) + [brei_proto_headers, eis_proto_headers, eis_proto_sources]

    lib_libeis = library('eis',
        src_libeis,
        dependencies: [dep_libutil],
        include_directories: [inc_builddir],
        gnu_symbol_visibility: 'hidden',
        version: soname,
        install: true
    )
    libeis_headers = files('libeis.h')
    install_headers(libeis_headers, subdir: libei_api_dir)

    dep_libeis = declare_dependency(link_with: lib_libeis,
                                    include_directories: [inc_src])
    meson.override_dependency('libeis', dep_libeis)

    pkgconfig.generate(lib_libeis,
        filebase: 'libeis-@0@'.format(libei_api_version),
        name: 'libEIS',
        description: 'Emulated Input server library',
        subdirs: libei_api_dir,
        version: meson.project_version(),
        libraries: lib_libeis,
    )
endif

if build_oeffis
    src_liboeffis = files('liboeffis.c')
    deps_liboeffis = [dep_libutil, dep_sdbus]

    lib_liboeffis = library('oeffis',
        src_liboeffis,
        include_directories: [inc_builddir],
        dependencies: deps_liboeffis,
        gnu_symbol_visibility: 'hidden',
        version: soname,
        install: true
    )
    liboeffis_headers = files('liboeffis.h')
    install_headers(liboeffis_headers, subdir: libei_api_dir)

    dep_liboeffis = declare_dependency(link_with: lib_liboeffis,
                                       include_directories: [inc_src])
    meson.override_dependency('liboeffis', dep_liboeffis)

    pkgconfig.generate(lib_liboeffis,
        filebase: 'liboeffis-@0@'.format(libei_api_version),
        name: 'libOeffis',
        description: 'RemoteDesktop portal DBus helper library',
        subdirs: libei_api_dir,
        version: meson.project_version(),
        libraries: lib_liboeffis,
    )

endif

summary({
    'liboeffis': build_oeffis,
    'libei': build_libei,
    'libeis': build_libeis,
}, section: 'Conditional Features', bool_yn: true)
