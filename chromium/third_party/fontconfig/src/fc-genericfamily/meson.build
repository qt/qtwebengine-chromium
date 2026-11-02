fcgenericfamily_gperf = custom_target(
  'fcgenericfamily.gperf',
  output: 'fcgenericfamily.gperf',
  input: [
    'serif.txt',
    'sans-serif.txt',
    'monospace.txt',
    'cursive.txt',
    'fantasy.txt',
    'system-ui.txt',
    'ui-serif.txt',
    'ui-sans-serif.txt',
    'ui-monospace.txt',
    'ui-rounded.txt',
    'emoji.txt',
    'math.txt',
    'fangsong.txt',
  ],
  command: [
    find_program('fc-genericfamily.py'),
    '-d',
    '@0@'.format(meson.current_source_dir()),
    '-o',
    '@OUTPUT@',
  ],
)
fcgenericfamily_h = custom_target(
  'fcgenericfamily.h',
  input: fcgenericfamily_gperf,
  output: 'fcgenericfamily.h',
  command: [gperf, '--pic', '-m', '100', '@INPUT@', '--output-file', '@OUTPUT@'],
)
