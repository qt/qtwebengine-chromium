schemas = [
  'org.gnome.desktop.default-applications.gschema.xml',
  'org.gnome.desktop.interface.gschema.xml',
  'org.gnome.desktop.lockdown.gschema.xml',
  'org.gnome.system.locale.gschema.xml',
  'org.gnome.system.location.gschema.xml',
  'org.gnome.system.proxy.gschema.xml',
  'org.gnome.desktop.sound.gschema.xml',
  'org.gnome.desktop.thumbnail-cache.gschema.xml',
  'org.gnome.desktop.a11y.gschema.xml',
  'org.gnome.desktop.a11y.keyboard.gschema.xml',
  'org.gnome.desktop.a11y.applications.gschema.xml',
  'org.gnome.desktop.a11y.interface.gschema.xml',
  'org.gnome.desktop.a11y.magnifier.gschema.xml',
  'org.gnome.desktop.a11y.mouse.gschema.xml',
  'org.gnome.desktop.thumbnailers.gschema.xml',
  'org.gnome.desktop.session.gschema.xml',
  'org.gnome.desktop.background.gschema.xml',
  'org.gnome.desktop.break-reminders.gschema.xml',
  'org.gnome.desktop.calendar.gschema.xml',
  'org.gnome.desktop.datetime.gschema.xml',
  'org.gnome.desktop.media-handling.gschema.xml',
  'org.gnome.desktop.screensaver.gschema.xml',
  'org.gnome.desktop.screen-time-limits.gschema.xml',
  'org.gnome.desktop.search-providers.gschema.xml',
  'org.gnome.desktop.wm.keybindings.gschema.xml',
  'org.gnome.desktop.wm.preferences.gschema.xml',
  'org.gnome.desktop.input-sources.gschema.xml',
  'org.gnome.desktop.privacy.gschema.xml',
  'org.gnome.desktop.notifications.gschema.xml',
  'org.gnome.desktop.app-folders.gschema.xml',
  'org.gnome.desktop.peripherals.gschema.xml',
]

conf = configuration_data()
conf.set('prefix', prefix)
conf.set('exec_prefix', prefix)
conf.set('libdir', libdir)
conf.set('bindir', bindir)
conf.set('libexecdir', libexecdir)
conf.set('datarootdir', datadir)
conf.set('datadir', datadir)
conf.set('includedir', includedir)

generated_schemas = []
foreach schema: schemas
  generated_schemas += configure_file(
    output: schema,
    input: schema + '.in',
    configuration: conf,
  )
endforeach

install_data(generated_schemas, install_dir: schemasdir)

install_data(
  'gsettings-desktop-schemas.convert',
  'wm-schemas.convert',
  install_dir: join_paths(datadir, 'GConf', 'gsettings'),
)
