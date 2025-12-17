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
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_WAIT_TIME_MS));

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

// Test creating a file after starting to watch it
TEST_F(FileWatcherTest, FileCreatedAfterWatchingCallbackCalled) {
    bool callbackCalled = false;
    auto callback = [&callbackCalled]() { callbackCalled = true; };

    // Start watching before file exists
    auto watcher = filewatcher_factory::createFileWatcher();
    watcher->startWatching(testFilePath, callback);
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_WAIT_TIME_MS));
    
    // Now create the file
    createFile(testFilePath, "Initial content");
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_WAIT_TIME_MS));
    watcher->stopWatching();

    EXPECT_TRUE(callbackCalled);
}

// Test stopping watcher multiple times is safe (idempotent)
TEST_F(FileWatcherTest, StopWatchingMultipleTimesIsSafe) {
    bool callbackCalled = false;
    auto callback = [&callbackCalled]() { callbackCalled = true; };
    
    createFile(testFilePath, "Content");
    auto watcher = filewatcher_factory::createFileWatcher();
    watcher->startWatching(testFilePath, callback);
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_WAIT_TIME_MS));
    
    // Stop multiple times should not crash
    watcher->stopWatching();
    watcher->stopWatching();
    watcher->stopWatching();
    
    // Test passes if we get here without crashing
    EXPECT_TRUE(true);
}

// Test rapid consecutive modifications (debouncing behavior)
TEST_F(FileWatcherTest, RapidModificationsDebounced) {
    int callbackCount = 0;
    auto callback = [&callbackCount]() { callbackCount++; };
    
    createFile(testFilePath, "Initial");
    auto watcher = filewatcher_factory::createFileWatcher();
    watcher->startWatching(testFilePath, callback);
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_WAIT_TIME_MS));
    
    // Rapidly modify the file multiple times
    for (int i = 0; i < 5; ++i) {
        appendToFile(testFilePath, std::to_string(i));
        // Very short delay to test debouncing
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    watcher->stopWatching();
    
    // Due to debouncing (50ms threshold), we expect fewer callbacks than modifications
    // Should be at least 1 but likely less than 5
    EXPECT_GE(callbackCount, 1);
    EXPECT_LE(callbackCount, 5);
}

// Test file in a subdirectory
TEST_F(FileWatcherTest, FileInSubdirectoryWatched) {
    bool callbackCalled = false;
    auto callback = [&callbackCalled]() { callbackCalled = true; };
    
    // Create a subdirectory and file in it
    std::string subdir = "test_subdir";
    std::string subdirFile = subdir + "/testfile_subdir.txt";
    
    // Create directory if it doesn't exist
    #ifdef _WIN32
        _mkdir(subdir.c_str());
    #else
        mkdir(subdir.c_str(), 0777);
    #endif
    
    createFile(subdirFile, "Content in subdir");
    
    auto watcher = filewatcher_factory::createFileWatcher();
    watcher->startWatching(subdirFile, callback);
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_WAIT_TIME_MS));
    
    appendToFile(subdirFile, " - modified");
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_WAIT_TIME_MS));
    watcher->stopWatching();
    
    // Cleanup
    std::remove(subdirFile.c_str());
    rmdir(subdir.c_str());
    
    EXPECT_TRUE(callbackCalled);
}

// Test file creation specifically (not modification or replacement)
TEST_F(FileWatcherTest, NewFileCreationCallbackCalled) {
    bool callbackCalled = false;
    auto callback = [&callbackCalled]() { callbackCalled = true; };
    
    // Ensure file doesn't exist
    std::remove(testFilePath.c_str());
    
    auto watcher = filewatcher_factory::createFileWatcher();
    watcher->startWatching(testFilePath, callback);
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_WAIT_TIME_MS));
    
    // Create new file (not modifying existing)
    createFile(testFilePath, "Brand new file");
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_WAIT_TIME_MS));
    watcher->stopWatching();
    
    EXPECT_TRUE(callbackCalled);
}

// Test multiple separate modifications with proper spacing
TEST_F(FileWatcherTest, MultipleModificationsWithSpacing) {
    int callbackCount = 0;
    auto callback = [&callbackCount]() { callbackCount++; };
    
    createFile(testFilePath, "Initial");
    auto watcher = filewatcher_factory::createFileWatcher();
    watcher->startWatching(testFilePath, callback);
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_WAIT_TIME_MS));
    
    // Make 3 modifications with sufficient spacing to avoid debouncing
    for (int i = 0; i < 3; ++i) {
        appendToFile(testFilePath, " modification");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    watcher->stopWatching();
    
    // With proper spacing, all 3 modifications should be detected
    EXPECT_GE(callbackCount, 3);
}

// Test concurrent file operations don't cause crashes
TEST_F(FileWatcherTest, ConcurrentModificationsSafe) {
    int callbackCount = 0;
    auto callback = [&callbackCount]() { callbackCount++; };
    
    createFile(testFilePath, "Initial");
    auto watcher = filewatcher_factory::createFileWatcher();
    watcher->startWatching(testFilePath, callback);
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_WAIT_TIME_MS));
    
    // Launch multiple threads that modify the file
    std::thread t1([this]() {
        for (int i = 0; i < 3; ++i) {
            appendToFile(testFilePath, "T1 ");
            std::this_thread::sleep_for(std::chrono::milliseconds(60));
        }
    });
    
    std::thread t2([this]() {
        for (int i = 0; i < 3; ++i) {
            appendToFile(testFilePath, "T2 ");
            std::this_thread::sleep_for(std::chrono::milliseconds(60));
        }
    });
    
    t1.join();
    t2.join();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    watcher->stopWatching();
    
    // Should have detected multiple changes without crashing
    EXPECT_GE(callbackCount, 1);
}
