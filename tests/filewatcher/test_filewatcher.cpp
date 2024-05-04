// #include "file_watcher.h"
#include "filewatcher/filewatcher_factory.h"
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <ios>
#include <thread>

// How long to wait for the callback to be called
#define THREAD_WAIT_TIME_MS 50

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
    bool callbackCalled = false;
    auto callback = [&callbackCalled]() { callbackCalled = true; };
    createFile(testFilePath, "New content");
    createFile(differentFilePath, "Different content");

    auto watcher = filewatcher_factory::createFileWatcher();
    watcher->startWatching(testFilePath, callback);
    appendToFile(differentFilePath, "New content");
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_WAIT_TIME_MS));
    watcher->stopWatching();

    EXPECT_FALSE(callbackCalled);
}

TEST_F(FileWatcherTest, FileModifiedCallbackCalled) {
    bool callbackCalled = false;
    auto callback = [&callbackCalled]() { callbackCalled = true; };
    createFile(testFilePath, "New content");

    auto watcher = filewatcher_factory::createFileWatcher();
    watcher->startWatching(testFilePath, callback);
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_WAIT_TIME_MS));
    appendToFile(testFilePath, "New content");
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_WAIT_TIME_MS));
    watcher->stopWatching();

    EXPECT_TRUE(callbackCalled);
}

TEST_F(FileWatcherTest, FileDeletedAndReplacedCallbackCalled) {
    bool callbackCalled = false;
    auto callback = [&callbackCalled]() { callbackCalled = true; };
    createFile(testFilePath, "New content");

    auto watcher = filewatcher_factory::createFileWatcher();
    watcher->startWatching(testFilePath, callback);
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_WAIT_TIME_MS));
    replaceFile(testFilePath, "Replacement content");
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_WAIT_TIME_MS));
    watcher->stopWatching();

    EXPECT_TRUE(callbackCalled);
}

TEST_F(FileWatcherTest, FileReplacedMultipleTimesCallbackCalled) {
    int callbackCount = 0;
    auto callback = [&callbackCount]() { callbackCount++; };

    createFile(testFilePath, "New content");
    auto watcher = filewatcher_factory::createFileWatcher();
    watcher->startWatching(testFilePath, callback);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    for (int i = 0; i < 10; ++i) {
        replaceFile(testFilePath, "Content " + std::to_string(i));
        std::this_thread::sleep_for(
            std::chrono::milliseconds(50)); // Wait a bit between replacements
    }
    watcher->stopWatching();

    EXPECT_GE(callbackCount, 10); // Ensure callback was called at least once
}
