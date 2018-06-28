project('festivald', 'cpp')

cpp = meson.get_compiler('cpp')

## Variables to store include directories and dependencies:

# - For festivald:
festivald_cargs = []
festivald_inc = []
festivald_deps = []

# - For festivald_client:
festivald_client_inc = []
festivald_client_deps = []

## systemd dependency:
if get_option('systemd')
    libsystemd = dependency('libsystemd', required: true)
    festivald_deps += libsystemd
    festivald_cargs += ['-DWITH_SYSTEMD']
    # This is used to find the path where the .service and .socket files should be installed.
    if get_option('systemdsystemunitdir') == 'from_pkgconfig'
      systemd = dependency('systemd')
      systemdsystemunitdir = systemd.get_pkgconfig_variable('systemdsystemunitdir')
    else
      systemdsystemunitdir = get_option('systemdsystemunitdir')
    endif
    message('festivald.service and festivald.socket will be installed to: ' + systemdsystemunitdir)
else
    message('While it is possible to use festivald without systemd, you will need to ' + \
            'provide an init script that is able to cope with the needed security measures.\n' + \
            'Festival was not designed with a modern security model, anyone with access ' + \
            'to the festivald socket (any festivald client) can run arbitrary system commands like ' + \
            '"(system "/bin/echo \'hello world\'")". While festivald should be improved' + \
            'to prevent this by providing a fine grained API for speech ' + \
            'synthesis, at least festivald uses UNIX sockets instead of INET sockets as festival does in --server mode, ' + \
            'making impossible the connection to remote users (localhost INET as used by festival is not ' + \
            'safe due to DNS rebinding attacks).')
endif

## Speech Tools dependency
# The speech tools consists of three libraries. It does not have pkg-config support,
# but we look for it anyway and we fall back to a dependency

# - libeststring
libeststring = dependency('libeststring', version : '>=2.4.0', required: false)
if libeststring.found()
    # We have pkg-config for libeststring!
    festivald_deps += libeststring
    festivald_client_deps += libeststring
else
    libeststring_inc = include_directories(get_option('speech-tools-include'))
    libeststring = cpp.find_library('libeststring')
    festivald_inc += libeststring_inc
    festivald_deps += libeststring
    festivald_client_inc += libeststring_inc
    festivald_client_deps += libeststring
endif


# - libestbase:
libestbase = dependency('libestbase', version : '>=2.4.0', required: false)

if libestbase.found()
    # We had pkg-config for libestbase!
    festivald_deps += libestbase
    festivald_client_deps += libestbase
else
    libestbase_inc = include_directories(get_option('speech-tools-include'))
    libestbase = cpp.find_library('libestbase', dependencies: libeststring)
    festivald_inc += libestbase_inc
    festivald_deps += libestbase
    festivald_client_inc += libestbase_inc
    festivald_client_deps += libestbase
endif

libestools = dependency('libestools', version : '>=2.4.0', required: false)

if libestools.found()
    # We have pkg-config for libestools!
    festivald_deps += libestools
    festivald_client_deps += libestools
else
    libestools_inc = include_directories(get_option('speech-tools-include'))
    libestools = cpp.find_library('libestools', dependencies: [libestbase, libeststring])
    festivald_inc += libestools_inc
    festivald_deps += libestools
endif



festival = dependency('festival', version : '>=2.4.0', required: false)

if festival.found()
    # We have pkg-config for festival!
    festivald_deps += festival
else
    festival_inc = include_directories(get_option('festival-include'))
    festival = cpp.find_library('Festival', dependencies: [libestbase, libestools, libeststring])
    festivald_inc += festival_inc
    festivald_deps += festival
endif

festivald = executable('festivald', ['festivald.cc'],
           dependencies: festivald_deps,
           include_directories: festivald_inc,
           cpp_args: festivald_cargs,
           install: true)

festivald_client = executable('festivald_client', ['festivald_client.cc'],
           dependencies: festivald_client_deps,
           include_directories: festivald_client_inc,
           install: true)


if get_option('systemd')
  ## The systemd unit service and socket files:
  bindir = join_paths(get_option('prefix'), get_option('bindir'))
  sysconfdir = join_paths(get_option('prefix'), get_option('sysconfdir'))
  sharedstatedir = join_paths(get_option('prefix'), get_option('sharedstatedir'))
  # This is the directory where the systemd managed festivald socket will be created: 
  runstatedir = join_paths(get_option('prefix'), get_option('runstatedir'))
  systemd_unit_conf = configuration_data()
  systemd_unit_conf.set('bindir', bindir)
  systemd_unit_conf.set('sysconfdir', sysconfdir)
  systemd_unit_conf.set('runstatedir', runstatedir)

  unit = configure_file(
    input: 'festivald.service.in',
    output: 'festivald.service',
    configuration: systemd_unit_conf,
    install_dir: systemdsystemunitdir
  )

  socket = configure_file(
    input: 'festivald.socket.in',
    output: 'festivald.socket',
    configuration: systemd_unit_conf,
    install_dir: systemdsystemunitdir
  )
endif

