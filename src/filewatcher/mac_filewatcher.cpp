#include "filewatcher/mac_filewatcher.h"
#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>
#include <filesystem>
#include <spdlog/spdlog.h>

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
    running = true;
    std::filesystem::path abspath(std::filesystem::absolute(path));
    
    // Use canonical path if file exists to resolve symlinks (e.g., /tmp -> /private/tmp)
    // Otherwise use absolute path to support watching files that don't exist yet
    std::filesystem::path resolvedPath;
    std::error_code ec;
    if (std::filesystem::exists(abspath, ec)) {
        resolvedPath = std::filesystem::canonical(abspath, ec);
        if (ec) {
            // If canonical fails, fall back to absolute
            resolvedPath = abspath;
        }
    } else {
        // File doesn't exist yet, use absolute and try to canonicalize parent
        auto parentPath = abspath.parent_path();
        if (std::filesystem::exists(parentPath, ec)) {
            auto canonicalParent = std::filesystem::canonical(parentPath, ec);
            if (!ec) {
                resolvedPath = canonicalParent / abspath.filename();
            } else {
                resolvedPath = abspath;
            }
        } else {
            resolvedPath = abspath;
        }
    }
    
    std::string dirPath = resolvedPath.parent_path();
    filename = resolvedPath.string();
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
