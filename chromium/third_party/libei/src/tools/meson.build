if build_libeis
    src_eis_demo_server = files(
        'eis-demo-server.c',
    )
    if dep_libevdev.found()
        src_eis_demo_server += files(
            'eis-demo-server-uinput.c',
        )
    endif

    eis_demo_server = executable('eis-demo-server',
               src_eis_demo_server,
               dependencies: [
                   dep_libutil,
                   dep_libeis,
                   dep_libxkbcommon,
                   dep_libevdev
               ],
               include_directories: [inc_builddir],
    )
endif

if build_libei
    executable('ei-demo-client',
               'ei-demo-client.c',
               dependencies: [dep_libutil, dep_libei, dep_libxkbcommon],
               include_directories: [inc_builddir],
    )

    executable('ei-debug-events',
               'ei-debug-events.c',
               dependencies: [dep_libutil, dep_libei, dep_libevdev],
               include_directories: [inc_builddir],
               install: true
    )
endif

if build_oeffis
    executable('oeffis-demo-tool',
               'oeffis-demo-tool.c',
               c_args: ['-DMESON_BUILDDIR="@0@"'.format(meson.current_build_dir())],
               include_directories: [inc_builddir],
               dependencies: [dep_libutil, dep_liboeffis],
    )
endif
