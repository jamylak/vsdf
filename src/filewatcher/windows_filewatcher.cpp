#include "filewatcher/windows_filewatcher.h"
#include <chrono>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <stdexcept>

using namespace std::chrono;

// Buffer size for ReadDirectoryChangesW
#define BUFFER_SIZE 4096

void WindowsFileWatcher::watchFile() {
    spdlog::info("Windows file watcher thread started");
    char buffer[BUFFER_SIZE];
    DWORD bytesReturned = 0;
    OVERLAPPED overlapped = {0};
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (overlapped.hEvent == NULL) {
        spdlog::error("Failed to create event for overlapped I/O");
        return;
    }

    auto lastEventTime =
        std::chrono::steady_clock::now() - std::chrono::seconds(100);

    while (running) {
        BOOL success = ReadDirectoryChangesW(
            hDirectory, buffer, BUFFER_SIZE, FALSE,
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
            &bytesReturned, &overlapped, NULL);

        if (!success) {
            spdlog::error("ReadDirectoryChangesW failed: {}", GetLastError());
            break;
        }

        // Wait for either the directory change event or the stop event
        HANDLE handles[2] = {overlapped.hEvent, hStopEvent};
        DWORD waitResult =
            WaitForMultipleObjects(2, handles, FALSE, INFINITE);

        if (waitResult == WAIT_OBJECT_0) {
            // Directory change event
            if (!GetOverlappedResult(hDirectory, &overlapped, &bytesReturned,
                                     FALSE)) {
                spdlog::error("GetOverlappedResult failed: {}",
                              GetLastError());
                break;
            }

            if (bytesReturned == 0) {
                spdlog::debug("Buffer overflow or no changes");
                ResetEvent(overlapped.hEvent);
                continue;
            }

            FILE_NOTIFY_INFORMATION *fni =
                reinterpret_cast<FILE_NOTIFY_INFORMATION *>(buffer);

            do {
                // Convert wide string to narrow string
                int nameLen = fni->FileNameLength / sizeof(WCHAR);
                std::wstring wideFilename(fni->FileName, nameLen);
                std::string changedFile(wideFilename.begin(),
                                        wideFilename.end());

                spdlog::debug("File change detected: {}", changedFile);
                spdlog::debug("Comparing with target: {}", filename);

                // Check if this is the file we're watching
                if (changedFile == filename) {
                    auto currentTime = steady_clock::now();
                    auto elapsedTime = currentTime - lastEventTime;
                    lastEventTime = currentTime;

                    if (elapsedTime < 50ms) {
                        spdlog::debug(
                            "Skipping event as it may be duplicate write");
                    } else {
                        spdlog::info("Tracked file change: {}", filename);
                        callback();
                    }
                }

                // Move to next notification if available
                if (fni->NextEntryOffset == 0)
                    break;
                fni = reinterpret_cast<FILE_NOTIFY_INFORMATION *>(
                    reinterpret_cast<BYTE *>(fni) + fni->NextEntryOffset);
            } while (true);

            ResetEvent(overlapped.hEvent);
        } else if (waitResult == WAIT_OBJECT_0 + 1) {
            // Stop event signaled
            spdlog::debug("Stop event received");
            break;
        } else {
            spdlog::error("WaitForMultipleObjects failed: {}",
                          GetLastError());
            break;
        }
    }

    CloseHandle(overlapped.hEvent);
    spdlog::info("Windows file watcher thread finished");
}

void WindowsFileWatcher::startWatching(const std::string &filepath,
                                       FileChangeCallback cb) {
    spdlog::info("Start watching (Windows)");
    this->callback = cb;

    std::filesystem::path path(std::filesystem::absolute(filepath));
    dirPath = path.parent_path().string();
    filename = path.filename().string();

    spdlog::info("Watching dirPath: {} for file: {}", dirPath, filename);

    // Convert to wide string for Windows API
    std::wstring wideDirPath(dirPath.begin(), dirPath.end());

    // Open directory handle
    hDirectory = CreateFileW(
        wideDirPath.c_str(), FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL);

    if (hDirectory == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Failed to open directory for watching: " +
                                 std::to_string(GetLastError()));
    }

    // Create stop event
    hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (hStopEvent == NULL) {
        CloseHandle(hDirectory);
        hDirectory = INVALID_HANDLE_VALUE;
        throw std::runtime_error("Failed to create stop event: " +
                                 std::to_string(GetLastError()));
    }

    running = true;
    watcherThread = std::thread{&WindowsFileWatcher::watchFile, this};
}

void WindowsFileWatcher::stopWatching() {
    spdlog::debug("Stop watching (Windows)");
    if (!running)
        return;

    running = false;

    // Signal the stop event to wake up the watching thread
    if (hStopEvent != INVALID_HANDLE_VALUE) {
        SetEvent(hStopEvent);
    }

    if (watcherThread.joinable()) {
        watcherThread.join();
        spdlog::info("Watcher thread successfully joined");
    }

    if (hStopEvent != INVALID_HANDLE_VALUE) {
        CloseHandle(hStopEvent);
        hStopEvent = INVALID_HANDLE_VALUE;
    }

    if (hDirectory != INVALID_HANDLE_VALUE) {
        CloseHandle(hDirectory);
        hDirectory = INVALID_HANDLE_VALUE;
    }

    spdlog::debug("Finished: Stop watching (Windows)");
}
