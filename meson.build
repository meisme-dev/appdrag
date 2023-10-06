project('appdrag', 'c',
  version : '0.1',
  default_options : ['warning_level=3'])

args = [
  '-Wall',
  '-Wextra',
  '-Wpedantic',  
  '-Werror'
]

flatpak_dep = dependency('flatpak')

exe = executable('appdrag', 'src/main.c', c_args : args, dependencies : [flatpak_dep], install : true)
