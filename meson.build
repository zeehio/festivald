project('festivald', 'cpp')

cpp = meson.get_compiler('cpp')

## Variables to store dependencies:
festivald_cargs = []
festivald_deps = []
festivald_client_deps = []

## systemd dependency:
if get_option('with-systemd')
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
    libeststring_lib = cpp.find_library('libeststring')
    libeststring = declare_dependency(include_directories: libeststring_inc,
                                      dependencies : libeststring_lib)
    festivald_deps += libeststring
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
    libestbase_lib = cpp.find_library('libestbase')
    libestbase = declare_dependency(include_directories: libestbase_inc,
                                    dependencies : [libestbase_lib, libeststring])
    festivald_deps += libestbase
    festivald_client_deps += libestbase
endif

libestools = dependency('libestools', version : '>=2.4.0', required: false)

if libestools.found()
    # We have pkg-config for libestools!
    festivald_deps += libestools
    festivald_client_deps += libestools
else
    libestools_inc = include_directories(get_option('speech-tools-include'))
    libestools_lib = cpp.find_library('libestools')
    libestools = declare_dependency(include_directories: libestools_inc,
                                    dependencies : [libestools_lib, libestbase, libeststring])
    festivald_deps += libestools
endif



festival = dependency('festival', version : '>=2.4.0', required: false)

if festival.found()
    # We have pkg-config for festival!
    festivald_deps += festival
else
    festival_inc = include_directories(get_option('festival-include'))
    festival_lib = cpp.find_library('Festival')
    festival = declare_dependency(include_directories: festival_inc,
                                  dependencies: [festival_lib, libestbase, libestools, libeststring])
    festivald_deps += festival
endif

festivald = executable('festivald', ['festivald.cc'],
           dependencies: festivald_deps,
           cpp_args: festivald_cargs,
           install: true)

festivald_client = executable('festivald_client', ['festivald_client.cc'],
           dependencies: festivald_client_deps,
           install: true)



festivald_conf_vars = configuration_data()

bindir = join_paths(get_option('prefix'), get_option('bindir'))
festivald_conf_vars.set('bindir', bindir)

datadir = join_paths(get_option('prefix'), get_option('datadir'))
festivald_conf_vars.set('datadir', datadir)


sysconfdir = join_paths(get_option('prefix'), get_option('sysconfdir'))
festivald_conf_dir = join_paths(sysconfdir, 'festivald')
festivald_conf_vars.set('festivald_conf_dir', festivald_conf_dir)

# This is the directory where the systemd managed festivald socket will be created: 
runstatedir = join_paths(get_option('prefix'), get_option('runstatedir'))
festivald_conf_vars.set('runstatedir', runstatedir)

if get_option('with-systemd')
  ## The systemd unit service and socket files:

  foreach key : ['systemd-socket-user', 'systemd-socket-group', 'systemd-socket-perm']
    festivald_conf_vars.set(key, get_option(key))
  endforeach

  unit = configure_file(
    input: 'festivald.service.in',
    output: 'festivald.service',
    configuration: festivald_conf_vars,
    install_dir: systemdsystemunitdir
  )

  socket = configure_file(
    input: 'festivald.socket.in',
    output: 'festivald.socket',
    configuration: festivald_conf_vars,
    install_dir: systemdsystemunitdir
  )
endif

festivald_conf = configure_file(
  input: 'festivald.conf.in',
  output: 'festivald.conf',
  configuration: festivald_conf_vars,
  install_dir: festivald_conf_dir
)

