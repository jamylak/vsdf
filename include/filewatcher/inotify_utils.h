#ifndef INOTIFY_UTILS_H
#define INOTIFY_UTILS_H
#include <spdlog/spdlog.h>
#include <sys/inotify.h>

namespace inotify_utils {
static void logInotifyEvent(inotify_event *event) noexcept {
    spdlog::debug("Event name {}", event->name);
    spdlog::debug("Event mask {}", event->mask);
    spdlog::debug("Event cookie {}", event->cookie);
    spdlog::debug("Event len {}", event->len);

    // Print mask information
    spdlog::debug("Modify: {} ", (event->mask & IN_MODIFY) ? "yes" : "no");
    spdlog::debug("Access: {} ", (event->mask & IN_ACCESS) ? "yes" : "no");
    spdlog::debug("Attrib: {} ", (event->mask & IN_ATTRIB) ? "yes" : "no");
    spdlog::debug("Close write: {} ",
                  (event->mask & IN_CLOSE_WRITE) ? "yes" : "no");
    spdlog::debug("Close nowrite: {} ",
                  (event->mask & IN_CLOSE_NOWRITE) ? "yes" : "no");
    spdlog::debug("Open: {} ", (event->mask & IN_OPEN) ? "yes" : "no");
    spdlog::debug("Moved from: {} ",
                  (event->mask & IN_MOVED_FROM) ? "yes" : "no");
    spdlog::debug("Moved to: {} ", (event->mask & IN_MOVED_TO) ? "yes" : "no");
    spdlog::debug("Create: {} ", (event->mask & IN_CREATE) ? "yes" : "no");
    spdlog::debug("Delete: {} ", (event->mask & IN_DELETE) ? "yes" : "no");
    spdlog::debug("Delete self: {} ",
                  (event->mask & IN_DELETE_SELF) ? "yes" : "no");
    spdlog::debug("Move self: {} ",
                  (event->mask & IN_MOVE_SELF) ? "yes" : "no");
    spdlog::debug("Is dir: {} ", (event->mask & IN_ISDIR) ? "yes" : "no");
    spdlog::debug("Unmount: {} ", (event->mask & IN_UNMOUNT) ? "yes" : "no");
    spdlog::debug("Q overflow: {} ",
                  (event->mask & IN_Q_OVERFLOW) ? "yes" : "no");
    spdlog::debug("Ignored: {} ", (event->mask & IN_IGNORED) ? "yes" : "no");
    spdlog::debug("In close write: {} ",
                  (event->mask & IN_CLOSE_WRITE) ? "yes" : "no");
}
} // namespace inotify_utils
#endif
