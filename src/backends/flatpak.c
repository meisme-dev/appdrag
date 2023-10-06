#include "flatpak.h"
#include <appdrag.h>
#include <flatpak/flatpak.h>

static FlatpakInstallation *installation = NULL;

void backend_flatpak_init(GError **error) {
  g_assert(*error == NULL);
  installation = flatpak_installation_new_system_with_id("default", NULL, error);
}

void backend_flatpak_install(gchar *filename, GError **error) {
  g_assert(*error == NULL);
  g_assert(installation != NULL);

  gchar *data = NULL;
  GBytes *bytes = NULL;

  FlatpakTransaction *transaction = flatpak_transaction_new_for_installation(installation, NULL, error);
  if (*error != NULL) {
    goto done;
  }

  g_file_get_contents(filename, &data, NULL, error);
  if (*error != NULL) {
    goto done;
  }

  bytes = g_bytes_new(data, strlen(data));

  flatpak_transaction_add_install_flatpakref(transaction, bytes, error);
  if (*error != NULL) {
    goto done;
  }

  flatpak_transaction_run(transaction, NULL, error);
  if (*error != NULL) {
    goto done;
  }

  goto done;

done : {
  g_free(data);
  g_free(bytes);
  g_object_unref(transaction);
  return;
}
}

void backend_flatpak_destroy(void) {
  g_object_unref(installation);
}
