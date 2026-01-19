// #include "file_watcher.h"
#include "filewatcher/filewatcher_factory.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <ios>
#include <stdexcept>
#include <thread>

// How long to wait for the callback to be called
constexpr int THREAD_WAIT_TIME_MS = 50;
// Polling avoids a fixed long sleep so tests can finish early when callbacks
// are fast.
constexpr int kPollIntervalMs = 5;

// Helper function to simulate file modification

void createFile(const std::string &path, const std::string &content) {
    std::ofstream file(path);
    if (file.is_open()) {
        file << content;
        file.close();
    }
}

void appendToFile(const std::string &path, const std::string &content) {
    std::ofstream file(path, std::ios_base::app);
    if (file.is_open()) {
        file << content;
        file.close();
    }
}

// Helper function to simulate file deletion and creation
void replaceFile(const std::string &path, const std::string &content) {
    std::remove(path.c_str()); // Delete the file
    createFile(path, content); // Create a new file with content
}

void safeSaveFile(const std::string &path, const std::string &content) {
    std::filesystem::path original(path);
    std::filesystem::path tempPath = original;
    tempPath += ".tmp";
    createFile(tempPath.string(), content);

    std::error_code ec;
    std::filesystem::remove(original, ec);
    ec.clear();
    std::filesystem::rename(tempPath, original, ec);
    if (ec) {
        throw std::runtime_error("Failed to rename temp file: " + ec.message());
    }
}

class FileWatcherTest : public ::testing::Test {
  protected:
    // Our test file
    std::string testFilePath = "testfile.txt";
    // A different file just to make sure we only care about test file
    std::string differentFilePath = "differenttestfile.txt";

    virtual void SetUp() {
        std::remove(testFilePath.c_str());
        std::remove(differentFilePath.c_str());
    }

    virtual void TearDown() {
        std::remove(testFilePath.c_str());
        std::remove(differentFilePath.c_str());
    }
};

TEST_F(FileWatcherTest, NoChangeCallbackNotCalled) {
    std::atomic<bool> callbackCalled{false};
    auto callback = [&callbackCalled]() { callbackCalled.store(true); };
    createFile(testFilePath, "New content");
    createFile(differentFilePath, "Different content");
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_WAIT_TIME_MS));

    auto watcher = filewatcher_factory::createFileWatcher();
    watcher->startWatching(testFilePath, callback);
    appendToFile(differentFilePath, "New content");
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_WAIT_TIME_MS));
    watcher->stopWatching();

    EXPECT_FALSE(callbackCalled.load());
}

TEST_F(FileWatcherTest, FileModifiedCallbackCalled) {
    std::atomic<bool> callbackCalled{false};
    auto callback = [&callbackCalled]() { callbackCalled.store(true); };
    createFile(testFilePath, "New content");

    auto watcher = filewatcher_factory::createFileWatcher();
    watcher->startWatching(testFilePath, callback);
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_WAIT_TIME_MS));
    appendToFile(testFilePath, "New content");
    // Poll briefly so fast callbacks return quickly without a fixed long sleep.
    for (int waitedMs = 0;
         waitedMs < THREAD_WAIT_TIME_MS && !callbackCalled.load();
         waitedMs += kPollIntervalMs) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kPollIntervalMs));
    }
    watcher->stopWatching();

    EXPECT_TRUE(callbackCalled.load());
}

TEST_F(FileWatcherTest, FileDeletedAndReplacedCallbackCalled) {
    std::atomic<bool> callbackCalled{false};
    auto callback = [&callbackCalled]() { callbackCalled.store(true); };
    createFile(testFilePath, "New content");

    auto watcher = filewatcher_factory::createFileWatcher();
    watcher->startWatching(testFilePath, callback);
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_WAIT_TIME_MS));
    replaceFile(testFilePath, "Replacement content");
    for (int waitedMs = 0;
         waitedMs < THREAD_WAIT_TIME_MS && !callbackCalled.load();
         waitedMs += kPollIntervalMs) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kPollIntervalMs));
    }
    watcher->stopWatching();

    EXPECT_TRUE(callbackCalled.load());
}

TEST_F(FileWatcherTest, FileReplacedMultipleTimesCallbackCalled) {
    std::atomic<int> callbackCount{0};
    auto callback = [&callbackCount]() { callbackCount.fetch_add(1); };

    createFile(testFilePath, "New content");
    auto watcher = filewatcher_factory::createFileWatcher();
    watcher->startWatching(testFilePath, callback);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    for (int i = 0; i < 10; ++i) {
        replaceFile(testFilePath, "Content " + std::to_string(i));
        std::this_thread::sleep_for(
            std::chrono::milliseconds(50)); // Wait a bit between replacements
    }
    for (int waitedMs = 0;
         waitedMs < THREAD_WAIT_TIME_MS && callbackCount.load() < 10;
         waitedMs += kPollIntervalMs) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kPollIntervalMs));
    }
    watcher->stopWatching();

    EXPECT_GE(callbackCount.load(), 10); // Ensure callback was called at least once
}

// This can be Windows only for now
// In any case the Shader Compiler will raise if it can't find the file
// So this doesn't seem to be too important eg. on Mac
#if defined(_WIN32)
TEST_F(FileWatcherTest, FileDeletedDoesNotTriggerCallback) {
    bool callbackCalled = false;
    auto callback = [&callbackCalled]() { callbackCalled = true; };
    createFile(testFilePath, "Initial content");

    auto watcher = filewatcher_factory::createFileWatcher();
    watcher->startWatching(testFilePath, callback);
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_WAIT_TIME_MS));
    std::remove(testFilePath.c_str());
    std::this_thread::sleep_for(
        std::chrono::milliseconds(THREAD_WAIT_TIME_MS * 2));
    watcher->stopWatching();

    EXPECT_FALSE(callbackCalled);
}

TEST_F(FileWatcherTest, SafeSaveRenameCallbackSeesFile) {
    std::atomic<int> callbackCount{0};
    std::atomic<int> failedOpenCount{0};
    auto callback = [&]() {
        std::ifstream file(testFilePath, std::ios::binary);
        if (file.is_open()) {
            callbackCount.fetch_add(1);
        } else {
            failedOpenCount.fetch_add(1);
        }
    };

    createFile(testFilePath, "Initial content");
    auto watcher = filewatcher_factory::createFileWatcher();
    watcher->startWatching(testFilePath, callback);
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_WAIT_TIME_MS));
    safeSaveFile(testFilePath, "Updated content");
    std::this_thread::sleep_for(
        std::chrono::milliseconds(THREAD_WAIT_TIME_MS * 4));
    watcher->stopWatching();

    EXPECT_EQ(failedOpenCount.load(), 0);
    EXPECT_GE(callbackCount.load(), 1);
}
#endif
