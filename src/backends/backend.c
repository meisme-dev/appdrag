#include "backend.h"
#include "appdrag.h"
#include <string.h>

#ifdef BACKEND_FLATPAK
#include "flatpak.h"
#endif

static gint backend_count = 0;

void backend_init(GError **error) {
  g_assert(*error == NULL);
#ifdef BACKEND_FLATPAK
  backend_flatpak_init(error);
  backend_count++;
#endif
  if (backend_count <= 0) {
    g_set_error(error, APPDRAG_ERROR, APPDRAG_ERROR_BACKENDS, "At least one backend is required");
    return;
  }
}

void backend_select(gchar *filename, GError **error, enum OperationType optype) {
  g_assert(*error == NULL);
  gchar *mime_type = g_content_type_guess(filename, NULL, 0, NULL);
  if (strcmp(mime_type, "application/vnd.flatpak.ref") == 0) {
#ifdef BACKEND_FLATPAK
    switch (optype) {
      case INSTALL:
      backend_flatpak_install(filename, error);
      break;
      case UNINSTALL:
      backend_flatpak_uninstall(filename, error);
      break;
    }
#endif
    g_free(mime_type);
  }
}

void backend_destroy(void) {
#ifdef BACKEND_FLATPAK
  backend_flatpak_destroy();
#endif
}
