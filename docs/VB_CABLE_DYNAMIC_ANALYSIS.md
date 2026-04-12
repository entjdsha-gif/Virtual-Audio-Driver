# VB-Cable Dynamic Analysis Notes (2026-04-12)

## Driver Base Addresses (리부트 후)

| Driver | Base | End |
|--------|------|-----|
| vbaudio_cablea64_win10.sys | `fffff807'1c750000` | `fffff807'1c772000` |
| vbaudio_cableb64_win10.sys | `fffff807'1c780000` | `fffff807'1c7a2000` |
| aocablea.sys | `fffff807'1c6f0000` | `fffff807'1c70e000` |
| aocableb.sys | `fffff807'07b00000` | `fffff807'07b1e000` |

## PE Sections (vbaudio_cablea64_win10)

| Section | RVA | Virtual Size |
|---------|-----|-------------|
| .text | 0x1000 | 0x6b92 |
| .rdata | 0x8000 | 0x2154 |
| .data | 0xb000 | 0x8228 |
| .pdata | 0x14000 | 0x07c8 |
| PAGE | 0x15000 | 0x647a |
| INIT | 0x1c000 | 0x0712 |

## Key Function Offsets (from Ghidra, confirmed with WinDbg `u`)

| Ghidra Addr | Offset | Function | Purpose |
|-------------|--------|----------|---------|
| 0x140005910 | +0x5910 | SetState | Stream state change (STOP/PAUSE/RUN) |
| 0x1400039ac | +0x39ac | Ring Reset | Zero ring buffer + counters |
| 0x1400010ec | +0x10ec | Ring Write (overwrite-oldest) | Advance ReadPos to make room |
| 0x140001144 | +0x1144 | Available Frames | (WritePos - ReadPos) % WrapBound |
| 0x1400011d4 | +0x11d4 | Ring Read | Read + denormalize |
| 0x1400022b0 | +0x22b0 | Ring Write (main) | Normalize + write to ring |
| 0x140005420 | +0x5420 | Position Query Helper | GetKsAudioPosition-side internal helper |
| 0x140005634 | +0x5634 | DMA→Scratch→Ring | DMA linearization + ring write |
| 0x140005cc0 | +0x5cc0 | DPC Timer Callback | Shared timer iterates all streams |
| 0x140006320 | +0x6320 | Frame Calculation | QPC→frame count + position update |
| 0x1400065b8 | +0x65b8 | Timer Start | ExAllocateTimer + ExSetTimer |
| 0x140006adc | +0x6adc | Capture DPC Path | Mic DPC ring read + DMA write |
| 0x140004cf4 | +0x4cf4 | Check Other Stream | Check if other stream is RUN |

## Global Data Addresses (VB-Cable A, base+offset)

| Offset | Absolute Addr | Purpose |
|--------|--------------|---------|
| +0x12f90 | `fffff807'1c762f90` | Stream pointer array (9 slots, 8 bytes each) |

## Live Stream Pointers (captured during VB-Cable call)

| Slot | Address | Notes |
|------|---------|-------|
| [0] | `ffffb68f'abd4e070` | Active stream #0 (Speaker or Mic) |
| [8] (at +0x40) | `ffffb68f'ae529120` | Active stream #8 (Speaker or Mic) |

## Stream Structure Offsets (from decompile)

| Offset | Purpose |
|--------|---------|
| +0x70 | buffer_size |
| +0x74 | current position |
| +0x90 | port class object pointer |
| +0x98 | other stream index |
| +0xA0 | stream type/config |
| +0xA4 | isMic flag |
| +0xA8 | DMA buffer size |
| +0xB4 | previous state |
| +0xB8 | active state (0=paused/stopped, nonzero=running) |
| +0xD0 | DMA position 1 |
| +0xD8 | DMA position 2 |
| +0xE0 | TotalBytesProcessed |
| +0xE8 | TotalBytesTransferred |
| +0xF0 | linear position |
| +0x108 | timer period (100ns units) |
| +0x110 | bytes this tick |
| +0x158 | DMA overrun counter (speaker) |
| +0x160 | spinlock (device-level) |
| +0x168 | **ring buffer pointer** |
| +0x178 | next tick deadline (QPC) |
| +0x180 | DMA overrun counter (mic) / timing baseline |
| +0x188 | QPC timestamp |
| +0x190 | zero on RUN |
| +0x198 | zero on RUN |
| +0x1A0 | frames per tick |
| +0x1A8 | zero on RUN |
| +0x1B0 | tick counter / timing |
| +0x1C8 | zero on RUN |

## Ring Structure Offsets (from decompile)

| Offset | Purpose |
|--------|---------|
| +0x00 | buffer pointer (relative to struct base) |
| +0x08 | buffer displacement |
| +0x0C | channels per frame |
| +0x10 | frame capacity |
| +0x14 | WrapBound (wrap threshold) |
| +0x18 | WritePos (frame index) |
| +0x1C | ReadPos (frame index) |
| +0x20 | stride / internal rate |
| +0x24 | valid flag |
| +0x60 | underrun flag |
| +0x17C | counter 1 (cleared on reset) |
| +0x180 | overflow counter (cleared on reset) |
| +0x184 | counter 3 (cleared on reset) |
| +0x188 | counter 4 / status flag (cleared on reset) |

## Next Steps

1. Read stream+0x168 to get ring pointer for both active streams
2. Read ring+0x14 (WrapBound), +0x18 (WritePos), +0x1C (ReadPos)
3. Read stream+0xA4 (isMic) and +0xB8 (active state) to identify Speaker vs Mic
4. Poll WritePos/ReadPos during active playback to see fill level dynamics
5. Poll during Speaker STOP gap to see if ring empties or stays filled

## WinDbg Commands (to run during active VB-Cable call)

```
REM Identify streams (run once):
dd ffffb68f`abd4e070+a0 L10
dq ffffb68f`abd4e070+168 L1
dd ffffb68f`ae529120+a0 L10
dq ffffb68f`ae529120+168 L1

REM After finding ring pointer (RING_ADDR):
dd RING_ADDR+14 L3    ; WrapBound, WritePos, ReadPos
dd RING_ADDR+17c L4   ; counters
```

---

## Codex Confirmed Notes (2026-04-13, KDNET + WinDbg)

These notes are intentionally additive. They record only facts that were
re-checked in live WinDbg or by direct disassembly during the April 13 session.

### 1. SetState state values are now confirmed

Live breakpoints on `vbaudio_cablea64_win10+0x5910` confirmed:

- `0 = STOP`
- `1 = ACQUIRE`
- `2 = PAUSE`
- `3 = RUN`

This is consistent with the current disassembly:

```
+0x5962  test esi, esi         -> state == 0  => STOP
+0x596a  sub ecx, 2
+0x596d  je +0x5a39           -> state == 2  => PAUSE
+0x5973  cmp ecx, 1
+0x5976  jne +0x5b27          -> state != 3  => early exit
+0x597c ...                   -> state == 3  => RUN
```

Important correction: `ACQUIRE (1)` is a real runtime state in live call flow.
It should not be treated as noise.

### 2. STOP does not reset the ring

Direct disassembly review of `+0x5910` still shows:

- `STOP` goes to `+0x5aca`
- the `STOP` path clears stream/timing fields
- the `STOP` path does **not** call `+0x39ac`

This remains one of the most important confirmed differences vs current AO
behavior assumptions.

### 3. PAUSE conditionally resets the ring

Direct disassembly review of the `PAUSE` path:

```
+0x5a39  cmp [rdi+0xB4], 2
+0x5a40  jle +0x5b27          ; only if previous state > PAUSE
+0x5a46  cmp [rdi+0x58], 0
+0x5a4a  je  +0x5a70
+0x5a5f  mov rcx, [rdi+0x168]
+0x5a66  test rcx, rcx
+0x5a69  je  +0x5a70
+0x5a6b  call +0x39ac         ; ring reset
```

So the correct statement is:

- `PAUSE` can reset the ring
- but only when the previous state was above PAUSE and the needed pointers are valid

This is more precise than the earlier shorthand "PAUSE always resets".

### 4. `+0x39ac` is not "state-only"

Live stack capture at `vbaudio_cablea64_win10+0x39ac` showed at least one call
path through:

- `portcls!CPortPinWaveRT::DistributeDeviceState`
- `portcls!PinPropertyDeviceState`
- `ks!KsPropertyHandler`

This means:

- `+0x39ac` can be reached from property / device-state handling
- not every `+0x39ac` hit should be interpreted as a simple STOP/PAUSE event

This was a useful correction during the live session.

### 5. Live ring tracking: same-ring state churn is real

During live call tracing, one ring pointer was observed as:

- `ffff968f\`b0630000`

For that same ring, live breakpoint logs showed:

- `ACQUIRE (1)`
- `PAUSE (2)`

For another live ring:

- `ffff968f\`ca9e0000`

the auto-log sequence captured:

- `1 -> 2 -> 3 -> 2 -> 1 -> 0`

with `+0x39ac` also logging on that same ring.

Interpretation:

- VB live call flow does not remain in a permanent RUN-only state
- real call flow can churn through `ACQUIRE`, `PAUSE`, `RUN`, and `STOP`
- this matters when comparing against AO gap handling assumptions

### 6. `+0x10ec` should be treated as a trim helper, not the generic write path

Re-reviewed function body:

```
+0x10f5  mov eax, [rcx+18]     ; WritePos
+0x10f8  mov edx, [rcx+1C]     ; ReadPos
+0x10fb  sub eax, edx
+0x10fd  mov r10d, [rcx+14]    ; WrapBound
...
+0x1115  lea eax, [rdx+1]
...
+0x1126  mov [rcx+1C], edx
```

This advances `ReadPos` aggressively until nearly the whole backlog is trimmed.
It is safer to document it as:

- `ReadPos trim / backlog reduction helper`

and **not** as:

- `generic overwrite-oldest write function`

The generic same-rate write path is still tracked separately at `+0x22b0`.

### 7. Recommended evidence tags for future notes

When adding more live facts, keep the following labels:

- `STATIC-CONFIRMED`: direct disassembly only
- `DYNAMIC-CONFIRMED`: live breakpoint / register / stack capture
- `INFERENCE`: interpretation from the above, not directly observed

This should keep static and dynamic claims from getting mixed together.

### 8. `+0x65b8` is the shared timer start / stream registration helper

Direct disassembly of `vbaudio_cablea64_win10+0x65b8` lines up with the import
table mapping found earlier:

- `+0x8110` -> `ExAllocateTimer`
- `+0x8118` -> `ExSetTimer`

Observed behavior of `+0x65b8`:

1. If global active-stream count at `+0x12f84` is zero:
   - zeroes the global stream pointer table at `+0x12f90`
   - clears the global timer handle slot at `+0x12fd8`
   - allocates a timer via `ExAllocateTimer`
   - arms it via `ExSetTimer`
2. Walks the global stream pointer table and inserts the current stream pointer
   (`rbx`) into the first empty slot
3. Increments the global active-stream count
4. Returns the shared timer handle

This strongly supports the earlier interpretation:

- VB uses one shared timer for active streams
- the RUN path registers the stream into that shared timer set
- this is not a per-stream timer allocation model

### 9. Live hot-path capture: A path active, B path idle

During a short live call hot-path trace with auto-continue breakpoints on:

- `A +0x22b0` (`[A Write]`)
- `A +0x11d4` (`[A Read]`)
- `B +0x22b0` (`[B Write]`)
- `B +0x11d4` (`[B Read]`)

the observed output was:

- repeated `A Write` on:
  - `ffff968f\`b0320000`
  - `ffff968f\`b2aa0000`
  - `ffff968f\`b27e0000`
- repeated `A Read` on:
  - `ffff968f\`b0320000`
- no `B Write` hits
- no `B Read` hits

Implications:

1. The traced live call was actively using VB Cable A, not VB Cable B.
2. One A-side ring (`b0320000`) was part of a paired write/read path.
3. Additional A-side write-only rings (`b2aa0000`, `b27e0000`) also existed during
   the same call window, suggesting parallel stream objects or helper/internal
   paths rather than a single simple writer.

This is a stronger dynamic clue than the earlier state-only logs because it
shows which cable actually carried steady-state traffic.

### 10. `+0x669c` is the shared timer unregister / stop helper

Direct disassembly of `vbaudio_cablea64_win10+0x669c` confirms it is the
opposite side of `+0x65b8`:

1. Walks the global stream table at `+0x12f90`
2. Clears the matching stream pointer
3. Decrements the global active-stream count at `+0x12f84`
4. Recomputes the high-water slot count at `+0x12f88`
5. If the active-stream count becomes zero:
   - calls `ExDeleteTimer`
   - clears the shared timer handle at `+0x12fd8`
   - zeroes related global timing fields at `+0x298/+0x2A0/+0x2A8`

This confirms:

- `+0x65b8` = shared timer registration / start
- `+0x669c` = shared timer unregister / stop

### 11. Caller mapping for live hot-path rings

The live caller addresses from the `[A Write]` / `[A Read]` trace now map as
follows:

- `caller=...573a`
  - inside `FUN_140005634`
  - this is the main speaker-side DMA/scratch -> ring write path
- `caller=...6884`
  - inside `FUN_140006778`
  - this is a helper path that can call `+0x11d4` / `+0x22b0` outside the
    main `FUN_140006adc` path
- `caller=...6298`
  - inside `FUN_140005cc0` shared timer callback
  - writes `*(puVar9 + 0x1a8)` from `*(puVar9 + 0x270)`
- `caller=...62e6`
  - inside `FUN_140005cc0` shared timer callback
  - conditionally writes through `piVar15` (captured earlier from
    `*(puVar9 + 0x1a0)`)

Interpretation:

- ring `b0320000` is the main paired write/read path
- rings `b2aa0000` and `b27e0000` are not random noise; they are produced from
  the shared timer callback's auxiliary write paths
- this strongly suggests VB has extra timer-driven helper/output paths beyond a
  single simple writer feeding a single simple reader

### 12. `.fnent` confirmation for live caller addresses

WinDbg `.fnent` lookups confirmed the exact enclosing function ranges for the
live caller addresses:

- `fffff806\`24ea573a`
  - function range `+0x5634 .. +0x5904`
- `fffff806\`24ea6884`
  - function range `+0x6778 .. +0x68ac`
- `fffff806\`24ea6298`
  - function range `+0x5cc0 .. +0x631e`
- `fffff806\`24ea62e6`
  - function range `+0x5cc0 .. +0x631e`

This closes the earlier uncertainty from `ln` and makes the caller mapping
above stronger.

### 13. `+0x6778` is a helper dispatcher around `+0x22b0` / `+0x11d4`

Direct disassembly of `vbaudio_cablea64_win10+0x6778` shows:

- one branch calls `+0x22b0`
- the other branch calls `+0x11d4`
- the branch is controlled by a flag at `[rcx+0xA4]`
- both branches operate on a ring pointer taken from `[rcx+0x170]`

This is important for the live hot-path trace because the `caller=...6884`
address is not the core read function itself; it is inside this helper wrapper.

Safe interpretation:

- `+0x6778` is a higher-level helper that decides whether the stream-side helper
  should push into the ring or pull from it
- `caller=...6884` therefore belongs to an orchestrating helper path, not the
  lowest-level read primitive

### 14. `+0x68ac` is a higher-level periodic transfer helper

Direct disassembly of `vbaudio_cablea64_win10+0x68ac` shows:

- early-init / timing setup through fields such as `+0x190`, `+0x1B0`
- one branch calls `+0x6adc`
- another branch calls `+0x5634`
- the branch decision is controlled by stream flags such as `[rcx+0xA4]` and
  `[rcx+0x164]`

Safe interpretation:

- `+0x68ac` is not a tiny leaf helper
- it looks like a higher-level periodic service routine that dispatches to
  either the capture-side transfer path or the render-side transfer path
- this matches the broader picture that VB has orchestration layers above the
  core ring read/write helpers

### 15. `+0x11d4` is a format-aware read / convert path with fill handling

Direct disassembly of `vbaudio_cablea64_win10+0x11d4` shows:

- argument validation and ring/config guards up front
- a possible diversion to `+0x17ac` when the current stream configuration does
  not match the fast path
- computation of available frames from ring state fields
- format-specific output handling for multiple sample encodings
- fallback fill/clear calls through `+0x7940` on invalid or insufficient-data
  conditions

Safe interpretation:

- `+0x11d4` is a real read-side transport function, not just a tiny wrapper
- it includes output conversion and underrun / recovery style fill behavior
- this supports the earlier view that VB's live path is more layered than a
  single flat "read ring -> write DMA" primitive

### 16. `+0x22b0` is the core write / convert / space-check primitive

Direct disassembly of `vbaudio_cablea64_win10+0x22b0` shows:

- input validation up front
- a fallback / slow-path call to `+0x26a0` when the stream configuration does
  not match the fast path
- direct conversion into the internal ring format for multiple sample encodings:
  - 8-bit unsigned -> centered signed internal value
  - 16-bit signed -> left-shifted internal value
  - 24-bit packed -> unpacked and scaled internal value
  - `0x78c` fast path -> direct 32-bit internal copy path
- available-space checks before writing
- overflow/failure handling with counter increments rather than silent overwrite
- optional zero/fill handling for remaining output span

Safe interpretation:

- `+0x22b0` is the main ring write primitive
- it is not just a memcpy
- it performs validation, format conversion, capacity checks, and error/fill
  behavior inside the primitive itself

### 17. `+0x5634` is the main speaker-side service wrapper around `+0x22b0`

Direct disassembly of `vbaudio_cablea64_win10+0x5634` shows:

- early guards on stream/ring state (`[rcx+178]`, `+0x4080`)
- staging/copy work through `+0x7680`
- a direct call to `+0x22b0` when `[rdi+170]` is available
- additional side work around counters, helper paths, and what looks like
  stream statistics / level handling

This matches the live hot-path trace:

- `caller=...573a` is inside this function
- `b0320000` was the main paired write/read ring

Safe interpretation:

- `+0x5634` is the main speaker-side periodic transfer/service wrapper
- it prepares or stages render data, then pushes into the core ring write
  primitive `+0x22b0`

### 18. `+0x6778` is a helper dispatcher that chooses write-vs-read service

Direct disassembly of `vbaudio_cablea64_win10+0x6778` shows:

- the branch is controlled by a flag at `[rcx+0xA4]`
- one branch calls `+0x22b0`
- the other branch calls `+0x11d4`
- both branches operate on the ring pointer at `[rcx+0x170]`
- side counters such as `+0x278` / `+0x28c` are updated depending on branch

This is important because the live caller:

- `caller=...6884`

is inside this function, which means the captured `[A Read]` lines were coming
through this orchestration helper rather than directly from the raw low-level
primitive only.

Safe interpretation:

- `+0x6778` is a higher-level dispatcher that decides whether a helper/service
  path should push to the ring or pull from it

### 19. Updated transport picture

Combining the live caller mapping with the new disassembly:

- `+0x5634` -> main speaker-side service wrapper
- `+0x22b0` -> core ring write primitive
- `+0x6778` -> helper dispatcher around ring write/read primitives
- `+0x11d4` -> core read / convert / fill primitive
- `+0x5cc0` timer callback -> auxiliary write paths
- `+0x65b8 / +0x669c` -> shared timer register/unregister

This makes the VB transport model look like:

1. shared timer / state orchestration
2. periodic service wrappers
3. helper dispatchers
4. core ring write/read primitives
5. conversion / fill logic inside the primitives

This is materially richer than a naive one-function ring pipeline.

### 20. Negative live check: no `+0x6adc` or `+0x10ec` hits during short speech

With auto-continue breakpoints on:

- `vbaudio_cablea64_win10+0x6adc`
- `vbaudio_cablea64_win10+0x10ec`

and a short live utterance during the call window, no log lines were produced.

Safe interpretation:

- the observed steady-state call path did not execute `+0x6adc` during that
  capture window
- the observed steady-state call path did not execute `+0x10ec` during that
  capture window
- this strengthens the idea that the concrete live path previously observed was
  dominated by:
  - `+0x5634`
  - `+0x22b0`
  - `+0x6778`
  - `+0x11d4`

This does **not** prove that `+0x6adc` or `+0x10ec` are unused globally; only
that they were not part of the captured short steady-state call interval.

### 21. `+0x26a0` is the large slow-path write / adaptation routine

Direct disassembly of `vbaudio_cablea64_win10+0x26a0` shows:

- it is entered from `+0x22b0` when the fast-path assumptions do not hold
- it performs substantial validation and format dispatch
- it uses large local scratch areas (`[rbp-70]`, `[rbp-30]`) as temporary
  per-channel accumulators / residual buffers
- it contains repeated weighted split / recombine loops rather than a simple
  copy
- it supports the same major format branches seen elsewhere:
  - 8-bit
  - 16-bit
  - 24-bit packed
  - special `0x78c` branch
- it updates persistent state fields such as `[rdx+34]`, which suggests phase /
  carry / residual tracking across calls

Safe interpretation:

- `+0x26a0` is not a minor fallback wrapper
- it is the large slow-path write/adaptation routine
- it likely handles sample-rate adaptation and/or more complex channel / frame
  redistribution than the fast path

This is important because it means VB's write side has both:

- a core fast-path primitive (`+0x22b0`)
- a much heavier adaptation path (`+0x26a0`)

### 22. `+0x4f2c` is a smoothed per-channel level / peak helper

Direct disassembly of `vbaudio_cablea64_win10+0x4f2c` shows:

- it scans source audio data by format branch
- it computes per-channel magnitude / absolute peak values into a local buffer
- it supports 8-bit / 16-bit / 24-bit / special `0x78c` input handling
- at the end it updates per-channel arrays around `+0x50` or `+0xD0`
- when the new value is lower than the previous stored value, it applies a
  `* 0x7f >> 7` style decay instead of dropping immediately

Safe interpretation:

- `+0x4f2c` is not core transport
- it is a smoothed per-channel peak / level tracking helper
- it is likely used for metering / activity / side statistics rather than the
  main ring data path

This matters for comparison because AO should not confuse VB's side metering
helpers with its essential transport logic.

### 23. `portcls!CPortPinWaveRT::GetKsAudioPosition` is reached through the property path

Live breakpoint hits on `portcls!CPortPinWaveRT::GetKsAudioPosition` showed the
same repeated stack:

- `portcls!CPortPinWaveRT::GetKsAudioPosition`
- `portcls!PinPropertyPositionEx`
- `ks!KspPropertyHandler`
- `ks!KsPropertyHandler`
- `portcls!CPortPinWaveRT::DeviceIoControl`

Safe interpretation:

- `GetKsAudioPosition` is being polled through the KS property / DeviceIoControl
  path
- this is a position-query path, not proof of the core data-movement hot path
- position reporting must therefore be modeled separately from the main
  read/write transport path
- the same stack was observed both while idle and while speaking during a call,
  so this polling path does not appear to depend on active utterances

This is useful because it confirms that VB exposes timing/position through a
property polling mechanism layered above the actual transport helpers.

### 24. Short live call reconfirmed `+0x5634 -> +0x22b0` as the main write-side path

During a short spoken call segment, breakpoints on:

- `vbaudio_cablea64_win10+0x5634`
- `vbaudio_cablea64_win10+0x22b0`
- `vbaudio_cablea64_win10+0x68ac`

produced repeated hits of the form:

- `+0x5634` with frame-like `edx` values such as `3000`, `2610`, `3054`,
  `2772`, `3072`, `3066`, `3108`, `2754`, `2748`, `2760`, `3030`, `2826`,
  `2790`
- multiple immediate `+0x22b0` hits after each `+0x5634`
- no `+0x68ac` hits in the same interval

Safe interpretation:

- this strongly reconfirms `+0x5634` as the active write-side service wrapper
  in the observed live path
- `+0x22b0` is the repeatedly invoked core write primitive under that wrapper
- `+0x68ac` was again not part of the captured short steady-state write window
- the multiple `+0x22b0` hits per `+0x5634` suggest that one service interval
  can fan out into several internal write operations, though the exact unit
  should not be overclaimed yet

### 25. Entry breakpoint on `+0x5634` also hit from the position-query path

An entry breakpoint directly on `vbaudio_cablea64_win10+0x5634` produced a
stable stack of:

- `vbaudio_cablea64_win10+0x5634`
- `vbaudio_cablea64_win10+0x64f6`
- `vbaudio_cablea64_win10+0x54bb`
- `portcls!CPortPinWaveRT::GetKsAudioPosition+0x5d`
- `portcls!PinPropertyPositionEx`

This same stack was observed while speaking during a call.

Safe interpretation:

- `+0x5634` is not a helper that is only reachable from a single background
  service thread
- the position-query path can re-enter an internal helper chain that ends at
  `+0x5634`
- the safest wording is therefore:
  - `+0x5634` participates in the observed write-side path
  - but it can also be reused by position/accounting refresh logic reached from
    `GetKsAudioPosition`

This does **not** invalidate the earlier `+0x5634 -> +0x22b0` live write-path
evidence; it narrows the claim by showing that `+0x5634` is a shared internal
helper rather than a uniquely-owned top-level periodic entry.

### 26. `+0x5420 / +0x6320` suggest that position polling can actively drive update work

Static disassembly of the helpers around the observed stack shows:

- `+0x5420` acquires a lock around `stream+0x160`
- if the stream state at `+0xB4` is `3` and `+0xB0 == 0`, it obtains a timing
  value and calls `+0x6320`
- after that call, it writes values from `stream+0xC8` and `stream+0xD0` to the
  caller-provided output buffer and returns
- inside `+0x6320`, the driver:
  - converts elapsed time using timing / QPC-like fields
  - derives a frame delta
  - for one branch calls `+0x6adc`
  - for the other branch calls `+0x5634`
  - advances internal cursor / accounting fields such as `+0xD0`, `+0xD8`,
    `+0xE0`, `+0xE8`, and `+0x1B8`

Safe interpretation:

- `GetKsAudioPosition` is not merely reading counters that were already updated
  elsewhere
- in the observed path, position polling can actively trigger an internal
  elapsed-frame update chain
- for the main observed branch, that chain can reach `+0x5634`, which then
  reaches the core write primitive `+0x22b0`

Safest architecture wording after this evidence:

- VB does have a real shared-timer subsystem
- but at least part of the main ring / accounting progression can also be
  driven lazily from position-query polling
- the current best model is therefore hybrid:
  - shared timer coordination / auxiliary activity exists
  - position polling can drive main-path update work

### 27. `+0x6320` contains the explicit 8-frame minimum gate

Direct disassembly of `+0x6320` shows the core gate:

- a frame delta is derived from elapsed time and stream timing fields
- that delta is reduced by a previously-accounted value
- the result is compared against `8`
- if the delta is `< 8`, control jumps directly to the tail without doing the
  main processing work

Relevant sequence:

- `mov ebx, edx`
- `sub ebx, dword ptr [rdi+198h]`
- `cmp ebx, 8`
- `jl  +0x65a0`

Safe interpretation:

- this is the concrete implementation of the "8-frame minimum gate" idea
- sub-threshold elapsed work is accumulated rather than immediately converted
  into transport activity
- this makes the position-driven update model efficient even under frequent
  polling

### 28. Best current architecture model: main paired path is polling-driven, timer path remains auxiliary

The strongest combined reading of the current evidence is:

- the main observed paired path can be driven from
  `GetKsAudioPosition -> +0x5420 -> +0x6320 -> +0x5634 -> +0x22b0`
- `+0x6320` can also dispatch to `+0x6adc` on its other branch
- the shared-timer subsystem still exists:
  - register/unregister helpers `+0x65b8 / +0x669c`
  - shared callback `+0x5cc0`
- earlier live caller evidence already showed `+0x5cc0` participating in
  auxiliary write-only ring activity

Therefore the safest final wording is:

- **not** "the timer model was completely false"
- **not** "all data movement is timer-driven"
- but:
  - the main observed paired path appears to be lazily advanced by position
    polling
  - the timer subsystem remains real and appears to service auxiliary/shared
    activity
