#ifndef MAC_FILE_WATCHER_H
#define MAC_FILE_WATCHER_H
#include "filewatcher/filewatcher.h"
#include <CoreServices/CoreServices.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

class MacFileWatcher : public FileWatcher {
  public:
    // Using the callback type from the base class
    using FileWatcher::FileChangeCallback;

    ~MacFileWatcher() { stopWatching(); }
    void startWatching(const std::string &filepath,
                       FileChangeCallback callback) override;

    void stopWatching() override;

  private:
    // Callback function for FSEvent
    static void fsEventsCallback(ConstFSEventStreamRef streamRef,
                                 void *clientCallBackInfo, size_t numEvents,
                                 void *eventPaths,
                                 const FSEventStreamEventFlags eventFlags[],
                                 const FSEventStreamEventId eventIds[]);

    std::string filename; // absolute filename to watch
    std::thread watchThread;
    std::atomic<bool> running;
    std::mutex mtx;
    std::condition_variable cv;
};

#endif // MAC_FILE_WATCHER_H
