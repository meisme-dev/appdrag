#pragma once
#include <glib-2.0/gio/gio.h>

void backend_init(GError **error);
void backend_select(gchar *filename, GError **error);
void backend_destroy(void);
