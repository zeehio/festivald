project('festivald', 'cpp')

cpp = meson.get_compiler('cpp')
libsystemd = dependency('libsystemd', required: true)

festival = dependency('festival', version : '>=2.4.0', required: false)
libestools = dependency('libestools', version : '>=2.4.0', required: false)
libestbase = dependency('libestbase', version : '>=2.4.0', required: false)
libeststring = dependency('libeststring', version : '>=2.4.0', required: false)


festivald_inc = [include_directories('/usr/include/systemd')]
festivald_deps = [libsystemd]

festivald_client_deps = []


if libestbase.found()
    # We have pkg-config for libestbase!
    festivald_deps += libestbase
    festivald_client_deps += libestbase
else
    libestbase_inc = include_directories('/usr/include/speech_tools')
    libestbase_lib = cpp.find_library('libestbase')
    #libestbase = declare_dependency(dependencies: libestbase_lib,
    #                                include_directories: libestbase_inc)
    festivald_client_deps += libestbase_lib
    festivald_deps += libestbase_lib
    festivald_inc += libestbase_inc
endif

if libeststring.found()
    # We have pkg-config for libeststring!
    festivald_deps += libeststring
    festivald_client_deps += libeststring
else
    libeststring_inc = include_directories('/usr/include/speech_tools')
    libeststring_lib = cpp.find_library('libeststring')
    #libeststring = declare_dependency(dependencies: libeststring_lib,
    #                                include_directories: libeststring_inc)
    festivald_client_deps += libeststring_lib
    festivald_deps += libeststring_lib
    festivald_inc += libeststring_inc
endif


if libestools.found()
    # We have pkg-config for libestools!
    festivald_deps += libestools
    festivald_client_deps += libestools
else
    libestools_inc = include_directories('/usr/include/speech_tools')
    libestools_lib = cpp.find_library('libestools')
    #libestools = declare_dependency(dependencies: libestools_lib,
    #                                include_directories: libestools_inc)
    festivald_client_deps += libestools_lib
    festivald_deps += libestools_lib
    festivald_inc += libestools_inc
endif

if festival.found()
    # We have pkg-config for festival!
    festivald_deps += festival
else
    festival_inc = include_directories('/usr/include/festival')
    festival_lib = cpp.find_library('Festival', dependencies: libestools)
    #festival = declare_dependency(dependencies : festival_lib, 
    #                              include_directories : festival_inc)
    festivald_deps += festival_lib
    festivald_inc += festival_inc
endif

festivald = executable('festivald', ['festivald.cc'],
           dependencies: festivald_deps,
           include_directories: festivald_inc)

festivald_client = executable('festivald_client', ['festivald_client.cc'],
           dependencies: festivald_deps,
           include_directories: festivald_inc)

