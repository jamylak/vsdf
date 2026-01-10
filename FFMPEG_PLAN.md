# FFMPEG Integration Plan (Iterative + Testable)

Goal: integrate libav* (libavformat/libavcodec/libswscale/libavutil) directly into VSDF for recording frames to a video file, with small, testable steps. Each step adds one unit, can be tested, and then composed into the final pipeline. This plan targets offscreen rendering (no swapchain/window).

Assumptions:
- Recording is a separate mode (no shader hot reload while recording, unless explicitly re-enabled later).
- Integration uses FFmpeg libraries, not shelling out to `ffmpeg`.
- Rendering still uses Vulkan; we render to an offscreen color image and read back frame pixels into CPU memory for encoding.
- Recording can optionally enable a preview window, but capture remains offscreen.

---

## Step 1: Add FFmpeg build plumbing (no runtime code)
**Change**: add a CMake option `VSDF_ENABLE_FFMPEG` and link FFmpeg libs when enabled.

Files:
- `CMakeLists.txt` (add option + `find_package(PkgConfig)` or platform-specific find logic)

Snippet (conceptual):
```cmake
option(VSDF_ENABLE_FFMPEG "Enable FFmpeg recording" OFF)
if (VSDF_ENABLE_FFMPEG)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavformat libavcodec libavutil libswscale)
  target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::FFMPEG)
  target_compile_definitions(${PROJECT_NAME} PRIVATE VSDF_ENABLE_FFMPEG=1)
endif()
```

Test:
- Build once with `-DVSDF_ENABLE_FFMPEG=OFF` (default) and once with `ON`.
- Confirm no code changes are required yet.

Local try:
- `cmake -B build -DVSDF_ENABLE_FFMPEG=ON .`
- `cmake --build build`

Compose later:
- Subsequent steps will be guarded with `#if VSDF_ENABLE_FFMPEG`.

---

## Step 2: Add a tiny FFmpeg version probe (self-contained)
**Change**: add a tiny helper that reports libavformat version (compile-time only).

Files:
- `include/ffmpeg_utils.h` (new)
- `src/ffmpeg_utils.cpp` (new)

Snippet:
```cpp
std::string ffmpeg_utils::getLibavformatVersion() {
  unsigned ver = avformat_version();
  return fmt::format("libavformat {}", AV_VERSION_INT);
}
```

Test:
- Add a small unit test (if tests are enabled) or a lightweight run path in `main.cpp` behind a debug flag to print the version.

Local try:
- `./build/vsdf --ffmpeg-version` (new flag in a later step)

Compose later:
- Provides a known-good FFmpeg linkage check before any recording logic.

---

## Step 3: Add a CPU-side frame container type
**Change**: create a minimal `Frame` struct to hold width/height/stride/pixels.

Files:
- `include/frame.h` (new)

Snippet:
```cpp
struct Frame {
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t stride = 0;
  std::vector<uint8_t> rgba;
};
```

Test:
- Add a small test that constructs a `Frame` with dimensions and checks buffer size = `height * stride`.

Local try:
- Run the unit test executable.

Compose later:
- This becomes the bridge between Vulkan readback and FFmpeg encode.

---

## Step 4: Offscreen render target (no swapchain)
**Change**: add an offscreen color `VkImage` + framebuffer + render pass used for recording mode.

Files:
- `include/vkutils.h` (add helper declarations)
- `src/sdf_renderer.cpp` (create offscreen target when recording)

Snippet:
```cpp
struct OffscreenTarget {
  VkImage image;
  VkDeviceMemory memory;
  VkImageView view;
  VkFramebuffer framebuffer;
};
```

Test:
- Add a debug-only path to render one frame offscreen and verify that the render pass completes without errors.

Local try:
- Run `./build/vsdf --frames 1 --headless` and verify logs show offscreen path selected.

Compose later:
- The offscreen target will be used as the source for readback in Step 5.

---

## Step 5: Vulkan readback helper (single frame, blocking)
**Change**: implement a one-shot readback from the offscreen image into `Frame`.

Files:
- `include/vkutils.h` (add helper declarations)
- `src/sdf_renderer.cpp` (call helper once)

Snippet:
```cpp
Frame vkutils::readbackOffscreenImage(
    VkDevice device, VkPhysicalDevice phys, VkQueue queue,
    VkCommandPool pool, VkImage srcImage, VkExtent2D extent);
```

Test:
- Add a debug-only path to read back a single frame and write a checksum (e.g., hash of bytes) to logs.

Local try:
- Run `./build/vsdf --frames 1 --headless` and verify the checksum log.

Compose later:
- The readback helper will be reused by the recorder (Step 7+).

---

## Step 6: Add a basic image dump (PPM)
**Change**: convert `Frame` into a simple PPM file (for pixel correctness).

Files:
- `include/image_dump.h` (new)
- `src/image_dump.cpp` (new)

Snippet:
```cpp
void writePPM(const Frame& frame, const std::string& path);
```

Test:
- Use the readback frame from Step 5 and dump to `out.ppm`.

Local try:
- `./build/vsdf --frames 1 --headless --debug-dump-ppm out.ppm` (copies the swapchain image before present, so it stalls)
- Open the file to verify image contents.

Compose later:
- Confirms readback format matches expected RGBA ordering before FFmpeg.

---

## Step 7: Add a minimal FFmpeg encoder wrapper (no Vulkan yet)
**Change**: implement an `FFmpegRecorder` that accepts raw RGBA frames and writes a file.

Files:
- `include/ffmpeg_recorder.h` (new)
- `src/ffmpeg_recorder.cpp` (new)

Snippet:
```cpp
class FFmpegRecorder {
public:
  bool open(const std::string& path, uint32_t w, uint32_t h, uint32_t fps);
  bool writeFrame(const uint8_t* rgba, size_t bytes);
  void close();
};
```

Test:
- Add a small unit test that writes 2-3 synthetic frames (solid colors).

Local try:
- `./build/tests/vsdf_tests` (new test)
- Use `ffprobe` externally to validate duration and frames.

Compose later:
- This encoder will be fed by real frames in Step 8.

---

## Step 8: Wire Vulkan readback into FFmpeg (single frame)
**Change**: take the `Frame` from Step 5 and feed into `FFmpegRecorder` once.

Files:
- `src/sdf_renderer.cpp`

Snippet:
```cpp
if (recordOnce) {
  Frame frame = vkutils::readbackOffscreenImage(...);
  recorder.writeFrame(frame.rgba.data(), frame.rgba.size());
}
```

Test:
- Run with `--frames 1 --headless --record out.mp4`.

Local try:
- `./build/vsdf --toy shaders/testtoyshader.frag --frames 1 --headless --record out.mp4`

Compose later:
- Confirms end-to-end capture/encode for one frame.

---

## Step 9: Add a record mode with fixed duration
**Change**: add CLI flag `--record <path>` and `--record-frames N`.

Files:
- `src/main.cpp`
- `include/sdf_renderer.h` (add recorder config)
- `src/sdf_renderer.cpp` (run recorder during loop)

Snippet:
```cpp
struct RecorderConfig {
  std::string path;
  uint32_t fps = 60;
  uint32_t maxFrames = 0;
};
```

Test:
- Run in headless mode for `N` frames and verify file length.

Local try:
- `./build/vsdf --toy shaders/testtoyshader.frag --headless --record out.mp4 --record-frames 120`

Compose later:
- Establishes a controlled recording mode that disables shader hot reload.

---

## Step 9a: Optional preview window (non-capturing)
**Change**: allow an optional swapchain path for on-screen preview while capture stays offscreen.

Files:
- `src/sdf_renderer.cpp`
- `src/main.cpp`

Snippet:
```cpp
if (previewEnabled) {
  // render same scene to swapchain for display only
}
```

Test:
- Run with `--record out.mp4 --preview` and verify video encodes while window updates.

Local try:
- `./build/vsdf --toy shaders/testtoyshader.frag --record out.mp4 --record-frames 120 --preview`

Compose later:
- Preview stays optional; offscreen capture remains the source of truth.

---

## Step 10: Add a “record + no hot reload” policy
**Change**: disable filewatcher and pipeline updates while recording.

Files:
- `src/sdf_renderer.cpp`

Snippet:
```cpp
if (recorderEnabled) {
  // skip filewatcher setup and pipeline rebuilds
}
```

Test:
- Record while editing the shader; ensure no pipeline rebuilds occur.

Local try:
- Start recording, edit shader, confirm logs show no pipeline recreation.

Compose later:
- Stabilizes recording for deterministic output.

---

## Step 11: Optional perf improvements (async readback)
**Change**: add a ring of staging buffers to overlap GPU/CPU work.

Files:
- `include/vkutils.h`
- `src/sdf_renderer.cpp`

Snippet:
```cpp
struct ReadbackRing {
  std::vector<VkBuffer> buffers;
  std::vector<VkDeviceMemory> memory;
};
```

Test:
- Ensure frame time stays stable at target FPS.

Local try:
- Record 5-10 seconds and observe logs for frame pacing.

Compose later:
- If perf is acceptable, this step can be skipped or deferred.

---

## Step 12: Polish + cleanup
**Change**: finalize CLI flags, docs, and error messages.

Files:
- `README.md`
- `src/main.cpp`

Test:
- Rebuild with `VSDF_ENABLE_FFMPEG=OFF` and ensure all flags behave gracefully.

Local try:
- Run both recording and non-recording paths.

Compose later:
- Final integration with stable UX and clear documentation.

---

### Notes
- libav* API names often change; pin headers carefully and keep the wrapper minimal.
- If cross-platform discovery is tricky, we can add a fallback `VSDF_FFMPEG_ROOT` hint in CMake.
- Pixel format: prefer converting RGBA to YUV420P in `libswscale`.
