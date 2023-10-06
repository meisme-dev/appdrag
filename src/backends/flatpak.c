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

void backend_flatpak_uninstall(gchar *filename, GError **error) {
  g_assert(*error == NULL);
  g_assert(installation != NULL);

  gchar *data = NULL;
  gchar *name = NULL;
  gchar *branch = NULL;
  gchar *ref_string = NULL;
  GKeyFile *key_file = g_key_file_new();

  FlatpakTransaction *transaction = flatpak_transaction_new_for_installation(installation, NULL, error);
  if (*error != NULL) {
    goto done;
  }

  g_file_get_contents(filename, &data, NULL, error);
  if (*error != NULL) {
    goto done;
  }

  g_key_file_load_from_data(key_file, data, -1, G_KEY_FILE_NONE, error);
  if (*error != NULL) {
    goto done;
  }

  name = g_key_file_get_string(key_file, "Flatpak Ref", "Name", error);
  if (*error != NULL) {
    goto done;
  }

  branch = g_key_file_get_string(key_file, "Flatpak Ref", "Branch", error);
  if (*error != NULL) {
    goto done;
  }

  ref_string = g_strconcat("app/", name, "/", flatpak_get_default_arch(), "/", branch, NULL);
  if (*error != NULL) {
    goto done;
  }

  flatpak_transaction_add_uninstall(transaction, ref_string, error);
  if (*error != NULL) {
    goto done;
  }

  flatpak_transaction_run(transaction, NULL, error);
  if (*error != NULL) {
    goto done;
  }

  goto done;

done : {
  g_object_unref(transaction);
  g_free(data);
  g_free(name);
  g_free(branch);
  g_free(ref_string);
  g_free(key_file);
  return;
}
}

void backend_flatpak_destroy(void) {
  g_object_unref(installation);
}
