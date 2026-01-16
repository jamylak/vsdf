#include "filewatcher/mac_filewatcher.h"
#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <stdexcept>

// LATENCY
// ### function `FSEventStreamCreate`
// The number of seconds the service should wait after hearing about an event
// from the kernel before passing it along to the client via its callback.
// Specifying a larger value may result in more effective temporal coalescing,
// resulting in fewer callbacks and greater overall efficiency.
static constexpr CFTimeInterval LATENCY = 0.0;

void MacFileWatcher::fsEventsCallback(
    ConstFSEventStreamRef streamRef, void *clientCallBackInfo, size_t numEvents,
    void *eventPaths, const FSEventStreamEventFlags eventFlags[],
    const FSEventStreamEventId eventIds[]) {
    // Unused parameters
    (void)streamRef;
    (void)eventIds;

    char **paths = static_cast<char **>(eventPaths);
    MacFileWatcher *watcher = static_cast<MacFileWatcher *>(clientCallBackInfo);

    // Loop through each event
    for (size_t i = 0; i < numEvents; ++i) {
        std::string filePath(paths[i]);
        spdlog::debug("Checking change: {}", filePath);
        spdlog::debug("Against target: {}", watcher->filename);

        // Check if the event is related to a file
        if ((eventFlags[i] & kFSEventStreamEventFlagItemIsFile) &&
            ((eventFlags[i] & kFSEventStreamEventFlagItemCreated ||
              eventFlags[i] & kFSEventStreamEventFlagItemModified) &&
             !(eventFlags[i] & kFSEventStreamEventFlagItemRemoved))) {
            // Check if filename matches the target
            if (filePath == watcher->filename) {
                spdlog::info("File changed: {}", filePath);
                watcher->callback();
            }
        }
    }

    spdlog::debug("File in watched dir changed");
}
void MacFileWatcher::startWatching(const std::string &path,
                                   FileChangeCallback cb) {
    this->callback = cb;
    std::filesystem::path abspath(std::filesystem::absolute(path));
    std::error_code ec;

    if (!std::filesystem::exists(abspath, ec) || ec) {
        throw std::runtime_error("File does not exist: " + abspath.string());
    }
    if (!std::filesystem::is_regular_file(abspath, ec) || ec) {
        throw std::runtime_error("Path is not a regular file: " +
                                 abspath.string());
    }

    // Use canonical path to resolve symlinks (e.g., /tmp -> /private/tmp).
    std::filesystem::path canonicalPath =
        std::filesystem::canonical(abspath, ec);
    if (ec) {
        throw std::runtime_error("Failed to canonicalize path: " +
                                 abspath.string());
    }

    std::string dirPath = canonicalPath.parent_path();
    filename = canonicalPath.string();
    running = true;
    watchThread = std::thread([this, dirPath]() {
        spdlog::info("Watching file: {}", dirPath);
        CFStringRef pathToWatch = CFStringCreateWithCString(
            NULL, dirPath.c_str(), kCFStringEncodingUTF8);
        spdlog::info("Path to watch: {}", dirPath.c_str());
        CFArrayRef pathsToWatch =
            CFArrayCreate(NULL, (const void **)&pathToWatch, 1, NULL);
        spdlog::info("Setup FSEvent Stream");

        FSEventStreamContext context = {0, NULL, NULL, NULL, NULL};
        context.info = this;
        FSEventStreamRef stream =
            FSEventStreamCreate(NULL, fsEventsCallback, &context, pathsToWatch,
                                kFSEventStreamEventIdSinceNow, LATENCY,
                                kFSEventStreamCreateFlagFileEvents);
        spdlog::info("Create dispatch queue");

        dispatch_queue_t queue =
            dispatch_queue_create("com.example.filewatcherqueue", NULL);
        FSEventStreamSetDispatchQueue(stream, queue);
        FSEventStreamStart(stream);

        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return !running; });

        FSEventStreamStop(stream);
        FSEventStreamInvalidate(stream);
        FSEventStreamRelease(stream);
        CFRelease(pathsToWatch);
        CFRelease(pathToWatch);
        dispatch_release(queue);
        spdlog::info("Watcher thread finished");
    });
}

void MacFileWatcher::stopWatching() {
    running = false;
    cv.notify_one(); // Signal the condition variable to unblock the thread

    if (watchThread.joinable()) {
        watchThread.join();
        spdlog::info("Watcher thread succesfully joined");
    }
}
