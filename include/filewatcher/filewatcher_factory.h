#ifndef FILEWATCHERFACTORY_H
#define FILEWATCHERFACTORY_H

#ifdef __APPLE__
#include "mac_filewatcher.h"
#elif __linux__
#include "linux_filewatcher.h"
#else
#error "Unsupported platform."
#endif

namespace filewatcher_factory {
static std::unique_ptr<FileWatcher> createFileWatcher() {
#ifdef __APPLE__
    return std::make_unique<MacFileWatcher>();
#elif __linux__
    return std::make_unique<LinuxFileWatcher>();
#else
    return nullptr; // Handle unsupported platforms gracefully
#endif
}
} // namespace filewatcher_factory
#endif // FILEWATCHERFACTORY_H
