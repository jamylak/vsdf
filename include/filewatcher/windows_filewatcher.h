#ifndef WINDOWS_FILEWATCHER_H
#define WINDOWS_FILEWATCHER_H
#include "filewatcher.h"
#include <atomic>
#include <string>
#include <thread>
#include <windows.h>

class WindowsFileWatcher : public FileWatcher {
  public:
    ~WindowsFileWatcher() { stopWatching(); }

    void startWatching(const std::string &filepath,
                       FileChangeCallback cb) override;
    void stopWatching() override;

  private:
    void watchFile();
    std::thread watcherThread;
    std::string filename;      // Relative filename we watch
    std::string dirPath;       // Directory path to watch
    std::atomic<bool> running{false};
    HANDLE hDirectory = INVALID_HANDLE_VALUE;
    HANDLE hStopEvent = INVALID_HANDLE_VALUE;
};
#endif // WINDOWS_FILEWATCHER_H
