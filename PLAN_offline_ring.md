  1. Define a ring slot type

  - Add a RingSlot struct inside OfflineSDFRenderer (or a small private header).
  - Fields: offscreen image, image view, framebuffer, command buffer, fence, staging buffer, staging memory, inFlight flag.

  2. Add ring size config

  - Add ringSize to OfflineSDFRenderer (constructor arg or config/CLI flag).
  - Default to 2 or 3.

  3. Allocate N slots on setup

  - Replace “single offscreen image” with a loop:
      - Create image + view + framebuffer per slot.
      - Allocate one command buffer per slot.
      - Create one fence per slot.
      - Create one staging buffer per slot (map persistently if you already do).
  - Keep existing render pass/pipeline shared.

  4. Update render loop to rotate slots

  - slotIndex = frameIndex % ringSize.
  - If slot.inFlight, wait for slot.fence, then reset it.
  - Record + submit commands into slot.cmd targeting slot.framebuffer.
  - Mark slot.inFlight = true.

  5. Readback from completed slots

  - Minimal step: read back immediately from the same slot after waiting on its fence (behavior same as today, but reusable).
  - Better overlap: read back the previous slot (frameIndex - 1) while GPU renders the current slot.

  6. Keep debug PPM dump

  - Use the readback frame to call the existing dumpDebugFrame.
  - No other consumer yet.

  7. Cleanup

  - Loop over slots and destroy per‑slot resources.

  8. Tests

  - Update test case so it always runs offline test cases (not only smoke).
  - Ensure offline tests can run variable frame counts (e.g. 20 or 100).
