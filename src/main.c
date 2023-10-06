#include "appdrag.h"
#include "backends/backend.h"
#include <flatpak/flatpak.h>
#include <stdio.h>
#ifdef linux
#include "platforms/linux.h"
#endif

#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))

void drop_root_privileges(GError **err) {
  gid_t gid;
  uid_t uid;
  if (geteuid() != 0) {
    return;
  }
  if ((uid = getuid()) == 0) {
    char *tmp_uid = getenv("SUDO_UID");
    if (!tmp_uid) {
      goto handle_error;
    }
    uid = strtoll(tmp_uid, NULL, 10);
  }
  if ((gid = getgid()) == 0) {
    char *tmp_gid = getenv("SUDO_GID");
    if (!tmp_gid) {
      goto handle_error;
    }
    gid = strtoll(tmp_gid, NULL, 10);
  }
  if (setgid(gid) != 0) {
    goto handle_error;
  }
  if (setuid(uid) != 0) {
    goto handle_error;
  }
handle_error : {
  g_set_error(err, APPDRAG_ERROR, APPDRAG_ERROR_ROOT, "Failed to drop root: %s. Do not run the daemon as root!", strerror(errno));
  return;
}
}

void log_error(GError *error, GLogLevelFlags flags, const gchar *domain) {
  if (error != NULL) {
    g_log(domain, flags, "%s", error->message);
    g_error_free(error);
  }
}

int main(int argc, char **argv) {
  GError *error = NULL;
  GApplication *application = g_application_new("io.github.meisme.AppDrag", G_APPLICATION_DEFAULT_FLAGS);

  g_application_register(application, NULL, &error);
  if (error != NULL) {
    log_error(error, G_LOG_LEVEL_ERROR, "AppDrag-Core");
  }

  drop_root_privileges(&error);
  if (error != NULL) {
    log_error(error, G_LOG_LEVEL_ERROR, "AppDrag-Core");
  }

  backend_init(&error);
  if (error != NULL) {
    log_error(error, G_LOG_LEVEL_ERROR, "AppDrag-Backend");
  }

  int option = 0;
  while ((option = getopt(argc, argv, "f:h")) != -1) {
    switch (option) {
      case 'f':
        return monitor(strdup(optarg), application);
      case '?':
      case 'h':
      default:
        break;
    }
  }
  printf("Usage: %s -f <filename>\n", argv[0]);
  g_object_unref(application);
  return 0;
}
