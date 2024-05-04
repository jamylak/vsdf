# Test build and run tests on ubuntu
FROM ubuntu:latest

WORKDIR /app

# Install necessary packages
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    g++ \
    cmake \
    ninja-build \
    make \
    libgtest-dev \
    libspdlog-dev \
    libglfw3 libglfw3-dev \
    libvulkan-dev \
    glslang-tools \
    glslang-dev \
    libglm-dev
    # && rm -rf /var/lib/apt/lists/*

COPY . /app

RUN mkdir -p build && \
    cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON -B build -G Ninja .

RUN cmake --build build

CMD build/tests/vsdf_tests && build/tests/filewatcher/filewatcher_tests
