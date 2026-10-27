scanner_source = files('ei-scanner')
scanner = find_program(scanner_source)
protocol_xml_path = meson.current_source_dir() / 'protocol.xml'
protocol_dtd_path = meson.current_source_dir() / 'protocol.dtd'
protocol_xml = files(protocol_xml_path)
protocol_dtd = files(protocol_dtd_path)

xmllint = find_program('xmllint', required: false)
if xmllint.found()
    test('dtdcheck', xmllint,
         args: ['--dtdvalid', protocol_dtd, protocol_xml]
         )
endif

pymod = import('python')
required_python_modules = ['jinja2']
python = pymod.find_installation('python3', modules: required_python_modules)
if python.language_version().version_compare('<3.9')
    error('Python 3.9 or later required')
endif
