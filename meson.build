project('redsea', 'cpp', default_options : ['warning_level=3', 'buildtype=release', 'optimization=3'],
        version: '1.0-SNAPSHOT')

# Store version number to be compiled in
conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
configure_file(output : 'config.h',
               configuration : conf)


########################
### Compiler options ###
########################


cc = meson.get_compiler('cpp')
add_project_arguments(cc.get_supported_arguments([
    '-Wno-unknown-pragmas']), language: 'cpp')

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


####################
### Dependencies ###
####################


# Find libsndfile
sndfile = dependency('sndfile')

# Find nlohmann's json
json = dependency('nlohmann_json')

# Find iconv; may require -liconv
foreach linker_args : [ ['-liconv'], [] ]
  if cc.links('''
    #include <iconv.h>
    int main() {
      iconv_open("UTF-8", "ISO-8859-1");
    }''', args: linker_args)
    iconv = declare_dependency(link_args: linker_args)
    break
  endif
endforeach
if not iconv.found()
  # Last resort
  iconv = dependency('iconv')
endif

# Find liquid-dsp
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
if cc.has_function('modemcf_create',
   prefix : '#include <liquid/liquid.h>',
   dependencies : liquid)
  add_project_arguments('-DMODEM_IS_MODEMCF', language : 'cpp')
endif


############################
### Sources & Executable ###
############################


sources_no_main = [
  'src/block_sync.cc',
  'src/channel.cc',
  'src/dsp/liquid_wrappers.cc',
  'src/dsp/subcarrier.cc',
  'src/groups.cc',
  'src/input.cc',
  'src/options.cc',
  'src/rdsstring.cc',
  'src/tables.cc',
  'src/tmc/csv.cc',
  'src/tmc/tmc.cc',
  'src/tmc/locationdb.cc',
  'src/util.cc'
]

executable('redsea', [sources_no_main, 'src/redsea.cc'], dependencies: [iconv, json, liquid, sndfile],
           install: true)


##################
### Unit tests ###
##################

catch2 = dependency('catch2-with-main', required: false)

if catch2.found()
  test_exec = executable('redsea-test', [sources_no_main, 'test/unit.cc'],
                         dependencies: [iconv, json, liquid, sndfile, catch2])
  test('Smoke tests', test_exec)
endif
