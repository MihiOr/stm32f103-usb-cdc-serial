# STM32F103 Native USB Serial

Native USB CDC serial for STM32F103 Blue Pill boards, with C/C++ print helpers
and integration that survives STM32CubeMX code regeneration.

Use the board's USB connector as a virtual serial port. The library provides
USB initialization, board-specific endpoint handling, transmit/receive queues,
and `print` / `println`. No external UART adapter is needed.

This is an application integration layer over STM32Cube HAL and USB Device CDC
middleware. It is not a standalone USB stack. It includes adaptations developed
for an STM32F103C8T6-marked Blue Pill with nonstandard USB behavior; compatibility
with every clone or genuine board has not been established.

## Supported configuration

| Requirement | Configuration |
| --- | --- |
| MCU | STM32F103xB, evaluated on a Blue Pill marked STM32F103C8T6 |
| STM32Cube package | STM32CubeF1 1.8.7 |
| USB class | Single CDC device, USB Device FS |
| Build | CubeMX CMake project, GNU Arm GCC and GNU linker |
| USB clock | 48 MHz |
| Example board clock | HSE 8 MHz, CPU 72 MHz, USB PLL / 1.5 |
| Execution | Bare-metal main loop |

USB OTG MCUs, composite devices, other classes, IAR/Keil linkers, and concurrent
RTOS callers are outside the supported configuration.

## Installation

1. In CubeMX, enable **USB → Device (FS)** and **USB_DEVICE → Communication
   Device Class (Virtual Port Com)**. Configure a 48 MHz USB clock.
2. Generate a **CMake** project with **Keep User Code when re-generating** enabled.
3. Copy the **entire** `Libraries/NativeUsbSerial` folder from this repository
   into your project, including `runtime/` and the license files.
4. In the top-level `CMakeLists.txt`, immediately after the generated subdirectory:

   ```cmake
   add_subdirectory(cmake/stm32cubemx)
   include(Libraries/NativeUsbSerial/NativeUsbSerial.cmake)
   ```

5. Add the include and polling calls inside `main.c`'s `USER CODE` sections.
   See [the minimal example](examples/main_user_code.c).

6. Put CMake 3.22 or newer, Ninja, and GNU Arm Embedded tools
   (`arm-none-eabi-gcc` / `g++`) on your PATH. From your generated project's
   root, configure and build:

   ```sh
   cmake --preset Debug
   cmake --build --preset Debug
   # Or use the Release preset for both commands.
   ```

   Configure once before the first build. The result is
   `build/Debug/<your-project-name>.elf`; flashing is a separate step.

The generated `MX_USB_DEVICE_Init()` call selects this library's implementation.
Do not also call `nativeUsbInit()`. For a manually written main, call
`nativeUsbInit()` once after HAL and clock initialization.

The CMake helper excludes generated `usb_device.c`, `usbd_conf.c`, and
`usbd_cdc_if.c` from the executable. It adds the library and GNU linker wraps for
`HAL_PCD_IRQHandler` and `USBD_LL_DataInStage`. Generated descriptors remain in
use, so CubeMX still owns VID/PID, device strings, and serial-number generation.
HAL and middleware source files do not need manual patches. Copying only the
C/H files without the CMake helper is insufficient.

Some CubeMX versions list `Core/Src/sysmem.c` and `syscalls.c` in CMake without
actually generating them in a new project. If one is missing, the helper selects
its bundled, unmodified ST runtime template in `runtime/` and reports this at
configure time. Existing generated/custom runtime files are preserved and used
normally. No edits to generated CMake files are needed. These templates do not
redirect standard `printf()` to USB; use this library's `print` / `println` API.

## Quick example

```c
/* USER CODE BEGIN Includes */
#include "native_usb_serial.h"
/* USER CODE END Includes */

/* USER CODE BEGIN 2 */
uint32_t lastAlive = HAL_GetTick();
/* USER CODE END 2 */

/* Inside the main loop's USER CODE section */
serialProcess();
if ((uint32_t)(HAL_GetTick() - lastAlive) >= 1000U) {
    lastAlive = HAL_GetTick();
    println("Alive...");
}
```

Call `serialProcess()` regularly, including in error/idle loops that should
continue serving USB. `println()` copies text into a queue; it does not wait
for a serial monitor to open. The example's startup output may be missed before
the host opens the port.

## API

| Function | Behavior |
| --- | --- |
| `print(text)` / `println(text)` | Queue text; `println` appends CRLF. Drop a new complete message if there is insufficient space. |
| `printInt`, `printFloat`, `printDouble` | Queue formatted numbers; float uses 3 decimal places, double uses 6. |
| `printlnInt`, `printlnFloat`, `printlnDouble` | Numeric formatting with CRLF. |
| `printBin(value)` | 32 bits in groups of four, followed by CRLF. |
| `serialProcess()` | Service receive rearming and submit up to one 64-byte TX chunk. |
| `serialRead()` | Return a line on LF, ignore CR, retain at most 63 characters. Return NULL if no complete line is ready. |
| `serialDroppedBytes()` | Count bytes discarded by normal print queue overflow or explicit TX clearing. |
| `serialWrite(data, length)` | Copy up to 64 bytes that fit; return accepted byte count immediately. |
| `serialTryPrintln(text)` | Queue all text plus CRLF or return zero for caller-managed retry. Maximum text length: 1022 bytes. |
| `serialTryWriteFrame(data, length)` | Queue one complete binary frame only when the previous TX has finished. Return zero immediately if busy/unconfigured. Maximum: 1024 bytes. |
| `serialClearTxBuffer()` | Discard queued TX data. If a submitted text chunk ended mid-line, queue CRLF before new text. |

The `serialRead()` pointer refers to static storage reused by the next call.
Long lines are truncated. Queue management and frame APIs should be called
from a single main-loop context.

Clearing does not recall packets already submitted to the USB controller and
cannot clear buffers on the PC. It can leave a truncated old text line. Do not
use the text clear function to interrupt a binary frame: it may insert CRLF,
and a protocol without synchronization markers cannot recover from truncation.
For latest-only binary output, retain one replaceable pending frame in your
application and call `serialTryWriteFrame()` without blocking new input.

The default buffers reserve **5120 bytes**, excluding USB handles, descriptors,
CDC class storage, and other application data:

| Buffer | Bytes |
| --- | ---: |
| CDC receive buffer | 1024 |
| CDC transmit buffer | 1024 |
| Receive queue | 2048 |
| Print/transmit queue | 1024 |

These are static allocations even when empty. Clearing a queue does not reduce
the linked RAM footprint. The current source sets USB IRQ priority to 1.

## Serial monitor and wiring

Connect the board's native USB connector using a data-capable cable. USB uses
PA11 (D−) and PA12 (D+). The board needs the correct D+ pull-up; some Blue Pill
boards have a 10 kΩ resistor where 1.5 kΩ is expected. Firmware does not replace
the electrical pull-up or repair faulty cables/connectors.

Use a normal CDC serial monitor: 115200, 8N1, no flow control. CDC line coding
does not set a physical UART speed. DTR is not required by this implementation.
Windows chooses the COM number; it is not hardcoded to COM5. If using the optional
line echo, send LF or CRLF.

## Status and limitations

- The initial standalone implementation was built in Debug/Release, flashed,
  and exercised on the development board. CubeMX regeneration followed by a
  rebuild and native USB heartbeat/short-line echo also passed.
- On 2026-09-30, the library and the exact quick example above built in
  Debug and Release in a newly generated CubeMX project. This uncovered and
  fixed the missing runtime-file installation issue. The source compiles with
  the later queue/frame helpers, but their behavior was not hardware-tested in
  this check. See [validation notes](docs/VALIDATION.md).
- **Host-to-device multi-packet reception remains unreliable on the evaluated
  board.** Earlier tests with transfers larger than one 64-byte packet lost
  data. The malformed RX-count guard avoids copying an invalid PMA length; it
  does not solve the reception issue. This is currently best suited to debug
  output and short input lines, not dependable bulk input.
- Device-to-host 65-byte text output passed an earlier test; that is not a
  guarantee for all lengths or applications.
- Queueing is not an acknowledgement protocol. Reset/disconnect may lose an
  already submitted transfer. An application that retries forever on a full
  queue can still block itself; keep retries nonblocking or replace stale data.
- Long periods with interrupts disabled delay USB handling.

## Licensing and attribution

BSD-3-Clause covers the original header, CMake integration, example, and
documentation listed in the licensing notes. The implementation contains
ST-derived HAL/USB code, and the runtime templates also originate from ST.
Those files retain their applicable ST terms and are excluded from the root
BSD grant. The whole repository is not licensed as unrestricted open source.
ST notices and package terms are preserved. SLA-covered code requires a
processing device manufactured by or for ST; this grant does not authorize
use on non-ST clones. See [licensing notes](docs/LICENSING.md),
[LICENSE.txt](LICENSE.txt), and
[ST's package terms](Libraries/NativeUsbSerial/Package_license.html).
This repository is not an official STMicroelectronics product.
