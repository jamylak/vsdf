#ifndef LINUX_FILEWATCHER_H
#define LINUX_FILEWATCHER_H
#include "filewatcher.h"
#include <string>
#include <thread>

class LinuxFileWatcher : public FileWatcher {
  public:
    ~LinuxFileWatcher() { stopWatching(); }

    void startWatching(const std::string &filepath,
                       FileChangeCallback cb) override;
    void stopWatching() override;

  private:
    void watchFile();
    std::thread watcherThread;
    std::string filename; // Relative filname we watch
    int fd = -1;          // File descriptor
    int wd = -1;          // Watch descriptor
    bool running = true;
};
#endif
