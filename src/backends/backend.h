#pragma once
#include <glib-2.0/gio/gio.h>

enum OperationType {
  INSTALL,
  UNINSTALL
};

void backend_init(GError **error);
void backend_select(gchar *filename, GError **error, enum OperationType optype);
void backend_destroy(void);
