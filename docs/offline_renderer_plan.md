# Offline Renderer Plan

## Goal
Add a new headless/offscreen renderer class that can later feed frames to ffmpeg, while keeping the existing online path intact.

## Agreed Plan
- Keep the existing `OnlineSDFRenderer` as the online/windowed path.
- Add a note in `OnlineSDFRenderer` to clarify it is the online/swapchain path.
- Add a new class (e.g., `OfflineSDFRenderer`) dedicated to headless/offscreen rendering.
- Offline renderer requirements:
  - No swapchain or present.
  - No GLFW window or input handling.
  - No shader hot-reloading; compile once and fail fast on error.
  - Render to an offscreen image and support readback (future ffmpeg target).
  - Plan for a small ring of readback buffers (2-4) to overlap GPU render,
    GPU copy, and CPU encode once ffmpeg is integrated.
- Accept some duplicated logic initially; refactor shared pieces later if needed.
- Add the width and height params

## Near-Term Validation
- Render to an offscreen image and read back a frame for inspection.
- Use this as a checkpoint before integrating ffmpeg.

## Later
- See if we can move duplicated logic to shared base class
- Decide on an ffmpeg input pixel format (e.g., BGRA->YUV conversion path) and
  whether conversion happens on GPU or CPU.
