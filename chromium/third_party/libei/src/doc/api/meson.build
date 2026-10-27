if 'api' not in get_option('documentation')
    subdir_done()
endif

doxygen = find_program('doxygen', required : false)
if not doxygen.found()
	error('Program "doxygen" not found or not executable. Try building with -Ddocumentation=false')
endif

mainpage = vcs_tag(command : ['git', 'log', '-1', '--format=%h'],
		 fallback : 'unknown',
		 input : 'mainpage.dox',
		 output : 'mainpage.dox',
		 replace_string: '__GIT_VERSION__')

src_doxygen = files(
	# style files
	'doxygen-awesome.css',
) + [libei_headers, libeis_headers, liboeffis_headers]

doxyfiles = []
foreach f : src_doxygen
	df = configure_file(input: f,
			    output: '@PLAINNAME@',
			    copy : true)
	doxyfiles += [ df ]
endforeach

doc_config = configuration_data()
doc_config.set('PACKAGE_NAME', meson.project_name())
doc_config.set('PACKAGE_VERSION', meson.project_version())
doc_config.set('builddir', meson.current_build_dir())

doxyfile = configure_file(input : 'libei.doxygen.in',
			              output : 'libei.doxygen',
                          configuration : doc_config)

custom_target('doxygen',
	      input : [ doxyfile, mainpage ] + src_doxygen + doxyfiles,
	      output : [ 'doc' ],
	      command : [ doxygen, doxyfile ],
	      install : false,
	      depends: [ mainpage ],
	      build_by_default : true)
