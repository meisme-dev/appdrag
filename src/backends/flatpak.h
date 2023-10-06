#pragma once
#include <glib-2.0/gio/gio.h>

void backend_flatpak_init(GError **error);
void backend_flatpak_install(gchar *filename, GError **error);
void backend_flatpak_destroy(void);
