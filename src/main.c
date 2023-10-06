#include "appdrag.h"
#include "backends/backend.h"
#include <flatpak/flatpak.h>
#include <glib-2.0/gio/gio.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))

int monitor(char *filename, GApplication *application);

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
  while ((option = getopt(argc, argv, "f:h")) != -1) { // TODO: Help message
    switch (option) {
      case 'f':
        return monitor(strdup(optarg), application);
      case '?':
      default:
        break;
    }
  }

  g_object_unref(application);
  return 0;
}

int monitor(char *filename, GApplication *application) {
  char buf[BUF_LEN] __attribute__((aligned(8)));
  char *ptr;

  int inotify_fd = inotify_init();
  if (inotify_fd == -1) {
    perror("Inotify init failed");
    return -1;
  }

  int watch = inotify_add_watch(inotify_fd, filename, IN_CREATE | IN_MODIFY | IN_MOVED_TO | IN_ISDIR);
  if (watch == -1) {
    perror("Failed to watch directory");
    return -1;
  }

  GNotification *notification = g_notification_new("");
  GIcon *icon = g_themed_icon_new("dialog-information");

  GError *init_error = NULL;
  backend_init(&init_error);
  if (init_error != NULL) {
    g_notification_set_title(notification, "Failed to initialize backend(s)");
    g_notification_set_body(notification, init_error->message);
    g_notification_set_icon(notification, icon);
    g_notification_set_priority(notification, G_NOTIFICATION_PRIORITY_HIGH);
    g_application_send_notification(application, NULL, notification);
    return -1;
  }

  gint err_count = 0;

  for (;;) {
    int number_read = read(inotify_fd, buf, BUF_LEN);
    if (number_read == -1) {
      perror("Failed to read inotify descriptor");
      return -1;
    }

    for (ptr = buf; ptr < buf + number_read;) {
      struct inotify_event *event = (struct inotify_event *)ptr;
      if ((event->mask & IN_CREATE || event->mask & IN_MOVED_TO || event->mask & IN_MODIFY) && !(event->mask & IN_ISDIR)) {
        char *path = calloc(strlen(filename) + strlen(event->name) + 1, sizeof(char));
        sprintf(path, "%s/%s", filename, event->name);

        g_notification_set_title(notification, "Installing application");
        g_notification_set_body(notification, path);
        g_notification_set_icon(notification, icon);
        g_notification_set_priority(notification, G_NOTIFICATION_PRIORITY_NORMAL);
        g_application_send_notification(application, NULL, notification);

        GError *backend_error = NULL;

        backend_select(path, &backend_error);

        if (backend_error != NULL) {
          g_notification_set_title(notification, "Failed to use backend");
          g_notification_set_body(notification, backend_error->message);
          g_notification_set_icon(notification, icon);
          g_notification_set_priority(notification, G_NOTIFICATION_PRIORITY_HIGH);
          g_application_send_notification(application, NULL, notification);
          err_count++;
          if (err_count >= 3) {
            err_count = 0;
            ptr += sizeof(struct inotify_event) + event->len;
          }
          continue;
        }
        
        err_count = 0;

        g_notification_set_title(notification, "Installed application");
        g_notification_set_body(notification, path);
        g_notification_set_icon(notification, icon);
        g_notification_set_priority(notification, G_NOTIFICATION_PRIORITY_NORMAL);
        g_application_send_notification(application, NULL, notification);
      }
      ptr += sizeof(struct inotify_event) + event->len;
    }
  }
  g_object_unref(icon);
  g_object_unref(notification);
  backend_destroy();
  return 0;
}
