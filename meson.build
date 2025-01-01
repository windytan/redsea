project(
  'redsea',
  'cpp',
  default_options: [
    'warning_level=3',
    'buildtype=release',
    'optimization=3',
    'prefix=/usr/local',
    'cpp_std=c++14',
  ],
  version: '1.1.1-SNAPSHOT',
)

# Store version number to be compiled in
conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
configure_file(output: 'config.h', configuration: conf)

########################
### Compiler options ###
########################

cc = meson.get_compiler('cpp')
add_project_arguments(cc.get_supported_arguments(['-Wno-unknown-pragmas']), language: 'cpp')

# We want to use M_PI on Windows
if build_machine.system() == 'windows'
  add_project_arguments('-D_USE_MATH_DEFINES=1', language: 'cpp')
endif

# Explicit GNU extensions on Cygwin
if build_machine.system() == 'cygwin'
  override_options = ['cpp_std=gnu++14']
else
  override_options = []
endif

####################
### Dependencies ###
####################

# Find libsndfile
sndfile = dependency('sndfile')

# Find nlohmann's json
json = dependency('nlohmann_json', version: '>=3.9.0')

# Find iconv; may require -liconv
foreach linker_args : [['-liconv'], []]
  if cc.links(
    '''
    #include <iconv.h>
    int main() {
      iconv_open("UTF-8", "ISO-8859-1");
    }''',
    args: linker_args,
  )
    iconv = declare_dependency(link_args: linker_args)
    break
  endif
endforeach
if not iconv.found()
  # Last resort
  iconv = dependency('iconv')
endif

# Find liquid-dsp
liquid = cc.find_library('liquid', required: false)
# macOS: The above mechanism sometimes fails, so let's look deeper
if not liquid.found() and build_machine.system() == 'darwin'
  fs = import('fs')
  brew = find_program('brew', required: false)
  if brew.found()
    # Homebrew system
    liquid_prefix = run_command(brew, '--prefix', 'liquid-dsp', check: true).stdout().strip()
    liquid_lib = cc.find_library('liquid', dirs: [liquid_prefix + '/lib'])
    liquid_inc = include_directories(liquid_prefix + '/include')
    liquid = declare_dependency(dependencies: liquid_lib, include_directories: liquid_inc)
  elif fs.is_dir('/opt/local/lib')
    # MacPorts system
    liquid_lib = cc.find_library('liquid', dirs: ['/opt/local/lib'])
    liquid_inc = include_directories('/opt/local/include')
    liquid = declare_dependency(dependencies: liquid_lib, include_directories: liquid_inc)
  endif
endif
# API for modem/modemcf changed recently, but we can deal with either
if liquid.found() and cc.has_function('modemcf_create', prefix: '#include <liquid/liquid.h>', dependencies: liquid)
  add_project_arguments('-DMODEM_IS_MODEMCF', language: 'cpp')
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
  'src/tmc/events_data.cc',
  'src/tmc/tmc.cc',
  'src/tmc/locationdb.cc',
  'src/util.cc',
]

executable(
  'redsea',
  [sources_no_main, 'src/redsea.cc'],
  dependencies: [iconv, json, liquid, sndfile],
  install: true,
  override_options: override_options,
)

##################
### Unit tests ###
##################

build_tests = get_option('build_tests')

if build_tests
  catch2 = dependency('catch2-with-main', required: true)

  unit_test_exe = executable(
    'redsea-test-unit',
    [
      sources_no_main,
      'test/unit.cc',
    ],
    dependencies: [iconv, json, liquid, sndfile, catch2],
    override_options: override_options,
  )
  mpx_test_exe = executable(
    'redsea-test-mpx',
    [sources_no_main, 'test/mpx.cc'],
    dependencies: [iconv, json, liquid, sndfile, catch2],
    override_options: override_options,
  )
  test('Unit tests', unit_test_exe)
  test('MPX tests', mpx_test_exe)
endif
