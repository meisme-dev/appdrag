#include <flatpak/flatpak.h>
#include <glib-2.0/gio/gio.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))
#define APPDRAG_ERROR 1
#define APPDRAG_ERROR_ROOT 1

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
      goto error;
    }
    uid = strtoll(tmp_uid, NULL, 10);
  }
  if ((gid = getgid()) == 0) {
    char *tmp_gid = getenv("SUDO_GID");
    if (!tmp_gid) {
      goto error;
    }
    gid = strtoll(tmp_gid, NULL, 10);
  }
  if (setgid(gid) != 0) {
    goto error;
  }
  if (setuid(uid) != 0) {
    goto error;
  }
error : {
  g_set_error(err, APPDRAG_ERROR, APPDRAG_ERROR_ROOT, "Failed to drop root: %s. Do not run the daemon as root!", strerror(errno));
  return;
}
}

void log_error(GError *error, GLogLevelFlags flags, const gchar *domain) {
  g_log(domain, flags, "%s", error->message);
  g_error_free(error);
}

int main(int argc, char **argv) {
  GError *error = NULL;
  GApplication *application = g_application_new("io.github.meisme.AppDrag", G_APPLICATION_DEFAULT_FLAGS);

  g_application_register(application, NULL, NULL);
  if (error != NULL) {
    log_error(error, G_LOG_LEVEL_ERROR, "AppDrag-Core");
  }

  drop_root_privileges(&error);
  if (error != NULL) {
    log_error(error, G_LOG_LEVEL_ERROR, "AppDrag-Core");
  }

  int option = 0;
  while ((option = getopt(argc, argv, "f:h")) != -1) {  // TODO: Help message
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

  GError *error = NULL;
  FlatpakInstallation *installation = flatpak_installation_new_system_with_id("default", NULL, &error);
  if (error != NULL) {
    log_error(error, G_LOG_LEVEL_WARNING, "flatpak");
  }

  GNotification *notification = g_notification_new("");
  GIcon *icon = g_themed_icon_new("dialog-information");

  for (;;) {
    int number_read = read(inotify_fd, buf, BUF_LEN);
    if (number_read == -1) {
      perror("Failed to read inotify descriptor");
      return -1;
    }

    for (ptr = buf; ptr < buf + number_read;) {
      struct inotify_event *event = (struct inotify_event *)ptr;
      if ((event->mask & IN_CREATE || event->mask & IN_MOVED_TO || event->mask & IN_MODIFY) && !(event->mask & IN_ISDIR)) {
        char *tmp = calloc(strlen(filename) + strlen(event->name) + 1, sizeof(char));
        sprintf(tmp, "%s/%s", filename, event->name);
        puts(tmp);

        g_notification_set_title(notification, "Installing flatpakref");
        g_notification_set_body(notification, tmp);
        g_notification_set_icon(notification, icon);
        g_notification_set_priority(notification, G_NOTIFICATION_PRIORITY_NORMAL);
        g_notification_set_category(notification, "flatpak.installing");
        g_application_send_notification(application, NULL, notification);

        gchar *data = NULL;
        GBytes *bytes = NULL;

        FlatpakTransaction *transaction = flatpak_transaction_new_for_installation(installation, NULL, &error);
        if (error != NULL) {
          log_error(error, G_LOG_LEVEL_WARNING, "flatpak");
          goto next;
        }

        g_file_get_contents(tmp, &data, NULL, &error);
        if (error != NULL) {
          log_error(error, G_LOG_LEVEL_WARNING, "flatpak");
          goto next;
        }

        bytes = g_bytes_new(data, strlen(data));
        flatpak_transaction_add_install_flatpakref(transaction, bytes, &error);
        if (error != NULL) {
          log_error(error, G_LOG_LEVEL_WARNING, "flatpak");
          goto next;
        }

        flatpak_transaction_run(transaction, NULL, &error);
        if (error != NULL) {
          log_error(error, G_LOG_LEVEL_WARNING, "flatpak");
          goto next;
        }

        g_log("AppDrag-Core", G_LOG_LEVEL_INFO, "Installed %s successfully", event->name);

        g_notification_set_title(notification, "Installed flatpakref successfully");
        g_notification_set_body(notification, tmp);
        g_notification_set_icon(notification, icon);
        g_notification_set_priority(notification, G_NOTIFICATION_PRIORITY_NORMAL);
        g_notification_set_category(notification, "flatpak.installed");
        g_application_send_notification(application, NULL, notification);

        goto next;

      next : {
        ptr += sizeof(struct inotify_event) + event->len;
        free(tmp);
        if (bytes != NULL) {
          g_free(bytes);
        }
        if (data != NULL) {
          g_free(data);
        }
        g_object_unref(transaction);
        continue;
      }
      }
      ptr += sizeof(struct inotify_event) + event->len;
    }
  }
  g_object_unref(icon);
  g_object_unref(notification);
  g_object_unref(installation);
  return 0;
}
