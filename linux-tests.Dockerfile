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
    git \
    libgtest-dev \
    libspdlog-dev \
    libvulkan-dev \
    glslang-tools \
    glslang-dev \
    libglm-dev \
    pkg-config \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev
    # && rm -rf /var/lib/apt/lists/*

# GLFW 3.4+ is required for GLFW_PLATFORM hints on Linux.
RUN git clone --depth 1 --branch 3.4 https://github.com/glfw/glfw.git /tmp/glfw && \
    cmake -S /tmp/glfw -B /tmp/glfw/build -G Ninja \
      -DGLFW_BUILD_TESTS=OFF -DGLFW_BUILD_EXAMPLES=OFF \
      -DGLFW_BUILD_DOCS=OFF -DGLFW_INSTALL=ON \
      -DGLFW_BUILD_WAYLAND=OFF && \
    cmake --build /tmp/glfw/build && \
    cmake --install /tmp/glfw/build

COPY . /app

RUN mkdir -p build && \
    cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON -B build -G Ninja .

RUN cmake --build build

CMD build/tests/vsdf_tests && build/tests/filewatcher/filewatcher_tests
