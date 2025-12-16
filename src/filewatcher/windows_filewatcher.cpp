#include "filewatcher/windows_filewatcher.h"
#include <chrono>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <stdexcept>

using namespace std::chrono;

// Fixed-size buffer that ReadDirectoryChangesW fills with change notifications.
static constexpr DWORD BUFFER_SIZE = 4096;

// Minimum spacing between callbacks to coalesce duplicate notifications.
static constexpr auto DEBOUNCE_THRESHOLD_MS = 50ms;

// Offset so that the first detected change always fires (elapsed > threshold).
static constexpr auto INITIAL_TIME_OFFSET = std::chrono::seconds(100);

// Background loop that waits on Windows directory change notifications.
void WindowsFileWatcher::watchFile() {
    spdlog::info("Windows file watcher thread started");
    // Allocate a stack buffer for ReadDirectoryChangesW to write events into.
    char buffer[BUFFER_SIZE];
    // Track how many bytes ReadDirectoryChangesW wrote on each iteration.
    DWORD bytesReturned = 0;
    // OVERLAPPED structure enables async I/O without blocking this thread.
    // https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-overlapped
    OVERLAPPED overlapped = {0};
    // Create a manual-reset event that signals when async I/O completes.
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    // Bail out early if the event creation failed (resource exhaustion, etc.).
    if (overlapped.hEvent == NULL) {
        spdlog::error("Failed to create event for overlapped I/O");
        return;
    }

    // Initialize lastEventTime so the very first change never gets debounced.
    auto lastEventTime = std::chrono::steady_clock::now() - INITIAL_TIME_OFFSET;

    // Main loop runs until stopWatching flips the running flag.
    while (running) {
        // Kick off async directory monitoring for writes and renames.
        BOOL success = ReadDirectoryChangesW(
            hDirectory, buffer, BUFFER_SIZE, FALSE,
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
            &bytesReturned, &overlapped, NULL);

        // If the call failed immediately, log and exit the loop.
        if (!success) {
            spdlog::error("ReadDirectoryChangesW failed: {}", GetLastError());
            break;
        }

        // Wait for either a directory change event or the user stop event.
        HANDLE handles[2] = {overlapped.hEvent, hStopEvent};
        // Block until one of the two handles is signaled.
        DWORD waitResult =
            WaitForMultipleObjects(2, handles, FALSE, INFINITE);

        // If the overlapped event fired, there are change notifications to read.
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
                // Convert wide string to UTF-8 string
                int nameLen = fni->FileNameLength / sizeof(WCHAR);
                std::wstring wideFilename(fni->FileName, nameLen);
                
                // Use WideCharToMultiByte for proper UTF-16 to UTF-8 conversion
                int size_needed = WideCharToMultiByte(CP_UTF8, 0, 
                    wideFilename.c_str(), static_cast<int>(wideFilename.length()),
                    NULL, 0, NULL, NULL);
                std::string changedFile(size_needed, 0);
                WideCharToMultiByte(CP_UTF8, 0, wideFilename.c_str(),
                    static_cast<int>(wideFilename.length()), &changedFile[0],
                    size_needed, NULL, NULL);

                spdlog::debug("File change detected: {}", changedFile);
                spdlog::debug("Comparing with target: {}", filename);

                // Check if this is the file we're watching
                if (changedFile == filename) {
                    auto currentTime = steady_clock::now();
                    auto elapsedTime = currentTime - lastEventTime;
                    lastEventTime = currentTime;

                    if (elapsedTime < DEBOUNCE_THRESHOLD_MS) {
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

    // Convert UTF-8 string to wide string for Windows API
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, dirPath.c_str(),
                                          static_cast<int>(dirPath.length()),
                                          NULL, 0);
    std::wstring wideDirPath(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, dirPath.c_str(),
                        static_cast<int>(dirPath.length()), &wideDirPath[0],
                        size_needed);

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
