#ifndef FILE_WATCHER_H
#define FILE_WATCHER_H

#include <functional>
#include <string>

// Abstract class for file watching
class FileWatcher {
  public:
    FileWatcher() = default;
    using FileChangeCallback = std::function<void()>;

    virtual ~FileWatcher() = default;

    // Delete copy constructor and copy assignment operator
    FileWatcher(const FileWatcher &) = delete;
    FileWatcher &operator=(const FileWatcher &) = delete;

    // Delete move constructor and move assignment operator
    FileWatcher(FileWatcher &&) = delete;
    FileWatcher &operator=(FileWatcher &&) = delete;

    // Starts a thread to watch the file
    virtual void startWatching(const std::string &filepath,
                               FileChangeCallback callback) = 0;

    // Allows the thread watching the file to instantly stop
    virtual void stopWatching() = 0;

  protected:
    FileChangeCallback callback;
};

#endif // FILE_WATCHER_H
