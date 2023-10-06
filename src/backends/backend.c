#include "backend.h"
#include "backends/flatpak.h"
#include <glib-2.0/gobject/gobject.h>
#include <string.h>

#ifdef BACKEND_FLATPAK
#include "flatpak.h"
#endif

void backend_init(GError **error) {
  g_assert(*error == NULL);
#ifdef BACKEND_FLATPAK
  backend_flatpak_init(error);
#endif
}

void backend_select(gchar *filename, GError **error) {
  g_assert(*error == NULL);
  gchar *mime_type = g_content_type_guess(filename, NULL, 0, NULL);
  if (strcmp(mime_type, "application/vnd.flatpak.ref") == 0) {
#ifdef BACKEND_FLATPAK
    backend_flatpak_install(filename, error);
#endif
    g_free(mime_type);
  }
}

void backend_destroy(void) {
#ifdef BACKEND_FLATPAK
  backend_flatpak_destroy();
#endif
}
