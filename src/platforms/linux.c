#include "linux.h"
#include <appdrag.h>
#include <backends/backend.h>
#include <stdio.h>
#include <sys/inotify.h>

#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))

int monitor(gchar *filename, GApplication *application) {
  gchar buf[BUF_LEN] __attribute__((aligned(8)));
  gchar *ptr;

  int inotify_fd = inotify_init();
  if (inotify_fd == -1) {
    perror("Inotify init failed");
    return -1;
  }

  int watch = inotify_add_watch(inotify_fd, filename, IN_CREATE | IN_MODIFY | IN_MOVED_TO | IN_MOVED_FROM | IN_DELETE | IN_ISDIR);
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

  gchar *cache_path = g_strconcat(filename, "/", ".cache", NULL);
  g_mkdir_with_parents(cache_path, 0755);

  for (;;) {
    int number_read = read(inotify_fd, buf, BUF_LEN);
    if (number_read == -1) {
      perror("Failed to read inotify descriptor");
      return -1;
    }

    for (ptr = buf; ptr < buf + number_read;) {
      struct inotify_event *event = (struct inotify_event *)ptr;
      gchar *event_path_full = g_strconcat(filename, "/", event->name, NULL);

      gchar *event_cache_path_full = g_strconcat(filename, "/.cache/", event->name, NULL);

      GFile *dest = g_file_new_for_path(event_cache_path_full);
      GFile *source = g_file_parse_name(event_path_full);

      GError *op_error = NULL;

      if ((event->mask & IN_CREATE || event->mask & IN_MOVED_TO || event->mask & IN_MODIFY) && !(event->mask & IN_ISDIR)) {
        g_file_copy(source, dest, G_FILE_COPY_ALL_METADATA, NULL, NULL, NULL, &op_error);
        if (op_error != NULL) {
          goto done;
          continue;
        }
        
        g_notification_set_title(notification, "Installing application");
        g_notification_set_body(notification, event_path_full);
        g_notification_set_icon(notification, icon);
        g_notification_set_priority(notification, G_NOTIFICATION_PRIORITY_NORMAL);
        g_application_send_notification(application, NULL, notification);

        GError *backend_error = NULL;

        backend_select(event_path_full, &backend_error, INSTALL);

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
          free(event_path_full);
          continue;
        }

        err_count = 0;

        g_notification_set_title(notification, "Installed application");
        g_notification_set_body(notification, event_path_full);
        g_notification_set_icon(notification, icon);
        g_notification_set_priority(notification, G_NOTIFICATION_PRIORITY_NORMAL);
        g_application_send_notification(application, NULL, notification);
      } else if ((event->mask & IN_MOVED_FROM || event->mask & IN_DELETE) && !(event->mask & IN_ISDIR)) {
        g_notification_set_title(notification, "Removing application");
        g_notification_set_body(notification, event_path_full);
        g_notification_set_icon(notification, icon);
        g_notification_set_priority(notification, G_NOTIFICATION_PRIORITY_NORMAL);
        g_application_send_notification(application, NULL, notification);

        GError *backend_error = NULL;

        backend_select(event_cache_path_full, &backend_error, UNINSTALL);

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
          free(event_path_full);
          continue;
        }

        err_count = 0;

        g_notification_set_title(notification, "Removed application");
        g_notification_set_body(notification, event_path_full);
        g_notification_set_icon(notification, icon);
        g_notification_set_priority(notification, G_NOTIFICATION_PRIORITY_NORMAL);
        g_application_send_notification(application, NULL, notification);

        g_file_delete(dest, NULL, &op_error);
        if(op_error != NULL) {
          goto done;
          continue;
        }
      }
      goto done;
    done : {
      ptr += sizeof(struct inotify_event) + event->len;
      g_free(event_path_full);
      g_free(event_cache_path_full);
      g_free(source);
      g_free(dest);
    }
    }
  }
  g_object_unref(icon);
  g_object_unref(notification);
  backend_destroy();
  return 0;
}
