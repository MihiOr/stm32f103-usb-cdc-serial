# Validation record

The original STM32F103xB standalone integration used STM32CubeF1 1.8.7 and a
Blue Pill marked STM32F103C8T6. Historical checks on 2026-09-27 included:

- Debug and Release builds.
- ST-Link programming, verify, and reset of Release.
- Repeated native CDC `Alive...` output on Windows.
- Echo of lines with 1, 7, 62, and 63 characters. The longest echoed output
  includes CRLF, for 65 device-to-host bytes.
- CubeMX regeneration preserving library C/H, root CMake integration, and
  main USER CODE blocks, followed by build/flash/USB checks.
- Restored original HAL PCD and USB core sources matching the local Cube package.

That generator run returned OK but logged duplicate absolute-path diagnostics
for sysmem/syscalls. The project still built. A warning-free generator run was
not established.

The original sample's measured Release footprint was 32,208 bytes flash and
10,456 bytes linked RAM including reserved heap/stack. This is a whole sample
firmware figure, not a current library-only benchmark.

The library also includes the following APIs:
`serialWrite`, `serialTryPrintln`, `serialClearTxBuffer`, and
`serialTryWriteFrame`, plus USB IRQ priority 1. The later TX clearing and frame
helpers compile successfully but have not been hardware-tested.

Host-to-device transfers spanning multiple 64-byte packets failed earlier
tests on the evaluated board. Short-line success does not imply reliable bulk
RX. Additional chip/board variants and genuine STM32 hardware have not been
established as compatible.

Hardware logs and firmware binaries are not distributed with this library.

## Fresh installation build check — 2026-09-30

CubeMX generated a new project from USB CDC/48 MHz settings in an empty
`TMP/QuickExample` directory. No old generated C/H files, build cache, or firmware
were copied. The library folder was copied, its one-line root CMake include
was added, and the exact README quick-example contents were placed in the
appropriate main USER CODE blocks.

The first configure failed because CubeMX listed but omitted `sysmem.c` and
`syscalls.c`. The library includes original ST runtime templates and selects
them only when the generated source is missing. Configure/build then passed:

| Preset | Flash | Linked RAM including reserved heap/stack |
| --- | ---: | ---: |
| Debug | 47,608 bytes | 10,392 bytes |
| Release | 28,992 bytes | 10,392 bytes |

Build tools: CMake 4.3.1+st.1, Ninja 1.13.2+st.1, GNU Arm GCC 14.3.1+st.2;
STM32CubeF1 1.8.7 and the locally installed CubeMX 6.18.1 generator.
The memory figures cover this minimal firmware; optional APIs may be removed
by linker garbage collection. This was a build check without flashing or USB
hardware testing.

A second CubeMX regeneration preserved the quick-example USER CODE contents,
the root CMake file, and the entire library folder. CubeMX changed main.c's
formatting, so the main check compared the preserved code contents rather than
its whole-file byte hash. Debug and Release builds passed again without
reapplying integration edits. Generated HAL/core sources were not patched.
