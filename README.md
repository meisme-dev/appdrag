# AppDrag, a sweet and simple solution to app installation on Linux!
AppDrag is a deamon that will watch a directory, for example `$HOME/Apps`, for new files. Drag any .flatpakref file into the folder to install it!

## Command line usage
`/path/to/exe -f <folder>`

## Build dependencies
`glib2-devel` for notifications and interfacting with libflatpak

`flatpak-devel` for installing flatpaks

`pkg-config` for integrating the libraries

`meson` and `ninja` for compiling the program

## Runtime dependencies
`glib2` and `flatpak`