project('redsea', 'cpp', default_options : ['warning_level=3', 'buildtype=release'],
        version: '1.0-SNAPSHOT')

# Store version number to be compiled in
conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
configure_file(output : 'config.h',
               configuration : conf)

### Compiler options ###

cc = meson.get_compiler('cpp')
add_project_arguments(cc.get_supported_arguments([
    '-Wno-unknown-pragmas']), language: 'cpp')

if get_option('buildtype') == 'release'
  add_project_arguments('-O3', language : 'cpp')
endif

### Dependencies ###

iconv = dependency('iconv')
sndfile = dependency('sndfile')

# Detecting liquid-dsp in a portable fashion
if build_machine.system() == 'darwin'
  fs = import('fs')
  # Homebrew system
  if fs.is_dir('/opt/homebrew/lib')
    liquid_lib = cc.find_library('liquid',
                 dirs : ['/opt/homebrew/lib'])
    liquid_inc = include_directories('/opt/homebrew/include')
  # MacPorts system
  else
    liquid_lib = cc.find_library('liquid',
                 dirs : ['/opt/local/lib'])
    liquid_inc = include_directories('/opt/local/include')
  endif
  liquid = declare_dependency(dependencies : liquid_lib,
                              include_directories : liquid_inc)
else
  liquid = cc.find_library('liquid')
endif

# We want to use M_PI on Windows
if build_machine.system() == 'windows'
  add_project_arguments('-D_USE_MATH_DEFINES=1', language : 'cpp')
endif

# Explicit GNU extensions on Cygwin
if build_machine.system() == 'cygwin'
  add_project_arguments('-std=gnu++14', language : 'cpp')
else
  add_project_arguments('-std=c++14', language : 'cpp')
endif

sources = [
  'ext/json/jsoncpp.cpp',
  'src/block_sync.cc',
  'src/channel.cc',
  'src/common.cc',
  'src/groups.cc',
  'src/input.cc',
  'src/liquid_wrappers.cc',
  'src/options.cc',
  'src/rdsstring.cc',
  'src/redsea.cc',
  'src/subcarrier.cc',
  'src/tables.cc',
  'src/tmc/tmc.cc',
  'src/tmc/locationdb.cc',
  'src/util.cc'
]

executable('redsea', sources, dependencies: [iconv, sndfile, liquid])
