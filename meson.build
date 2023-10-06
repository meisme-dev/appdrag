project('appdrag', 'c',
  version : '0.1',
  default_options : ['warning_level=3'])

args = [
  '-Wall',
  '-Wextra',
  '-Wpedantic',  
  '-Werror'
]

include_dirs = [
  'src'
]

sources = [
  'src/main.c',
  'src/backends/flatpak.c',
  'src/backends/backend.c',
  'src/platforms/linux.c',
]

flatpak_dep = dependency('flatpak', required : false)

if flatpak_dep.found()
  args += '-DBACKEND_FLATPAK'
endif

exe = executable('appdrag', sources, c_args : args, dependencies : [flatpak_dep], include_directories : include_dirs, install : true)
