#include "filewatcher/linux_filewatcher.h"
#include "filewatcher/inotify_utils.h"
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <sys/inotify.h>
#include <thread>
#include <unistd.h> // for close()

// https://man7.org/linux/man-pages/man7/inotify.7.html
// inotify event has flexible array member: name
// so assume 16 byte name
#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * 4 * (EVENT_SIZE + 16))

using namespace std::chrono;

void LinuxFileWatcher::watchFile() {
    spdlog::info("I'm a thread 2");
    // Do this instead of getting min time because then we
    // get a huge nevative difference result value
    auto lastEventTime =
        std::chrono::steady_clock::now() - std::chrono::seconds(100);

    while (running) {
        char buffer[BUF_LEN];
        ssize_t length = read(fd, buffer, BUF_LEN);
        if (length < 0)
            throw std::runtime_error("Failed to read filebuffer " +
                                     std::string(strerror(errno)));

        ssize_t i = 0;

        while (i < length) {
            // Remember this will be events for whole dir
            spdlog::debug("Read {} bytes from inotify", length);
            inotify_event *event =
                reinterpret_cast<inotify_event *>(buffer + i);
            inotify_utils::logInotifyEvent(event);
            if (filename == event->name) {
                auto currentTime = steady_clock::now();
                auto elapsedTime = currentTime - lastEventTime;
                lastEventTime = currentTime;
                spdlog::debug("Elapsed time: {}", elapsedTime.count());
                if (elapsedTime < 50ms)
                    spdlog::debug("Skipping event as it may be double write");
                else {
                    spdlog::info("Tracked file change: {}", filename);
                    callback();
                }
            }
            i += static_cast<ssize_t>(EVENT_SIZE) +
                 static_cast<ssize_t>(event->len);
        }
    }
    spdlog::info("I finished");
}

void LinuxFileWatcher::startWatching(const std::string &filepath,
                                     FileChangeCallback cb) {
    spdlog::info("Start watching");
    this->callback = cb;

    fd = inotify_init();
    if (fd < 0) {
        throw std::runtime_error("Failed to initialize inotify " +
                                 std::string(strerror(errno)));
    }
    // We watch the dirpath and then filter
    // by filename under that in case file is
    // created and recreated, or else we'd lose
    // track of the inode
    std::filesystem::path path(std::filesystem::absolute(filepath));
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        throw std::runtime_error("File does not exist: " + path.string());
    }
    if (!std::filesystem::is_regular_file(path, ec) || ec) {
        throw std::runtime_error("Path is not a regular file: " +
                                 path.string());
    }
    std::string dirPath = path.parent_path();
    filename = path.filename();
    spdlog::info("Watching dirPath: {} for file path {}", dirPath,
                 path.string());

    wd = inotify_add_watch(fd, dirPath.c_str(), IN_MODIFY);
    if (wd == -1) {
        close(fd);
        throw std::runtime_error("Failed to initialize watch " +
                                 std::string(strerror(errno)));
    }

    watcherThread = std::thread{&LinuxFileWatcher::watchFile, this};
}

void LinuxFileWatcher::stopWatching() {
    spdlog ::debug("Stop watching");
    running = false;
    if (wd != -1)
        inotify_rm_watch(fd, wd);
    // Now it should receive 1 final event which is the
    // inotify watch IN_IGNORED and then end because
    // running is false
    if (watcherThread.joinable()) {
        watcherThread.join();
        spdlog::info("Watcher thread succesfully joined");
    }
    if (fd != -1)
        close(fd);
    spdlog::debug("Finished: Stop watching");
}
