if 'protocol' not in get_option('documentation')
    subdir_done()
endif

hugo = find_program('hugo', required : false)
if not hugo.found()
	error('Program "hugo" not found or not executable. Try building with -Ddocumentation=false')
endif

src_script = files('generate-protocol-docs.sh')

hugo_script = find_program(src_script)

src_hugo = files(
    'config.toml',
    'interface.md.tmpl',
) + src_script

custom_target('hugo',
	      input : src_hugo + [protocol_xml],
	      output : [ 'doc' ],
          command : [ hugo_script, '--git-repo', meson.project_source_root(), '--output-dir', meson.current_build_dir() ],
	      install : false,
	      build_by_default : true,
)
