/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026 MihiOr. See the repository LICENSE.txt. */
#ifndef NATIVE_USB_SERIAL_H
#define NATIVE_USB_SERIAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CubeMX main may continue calling MX_USB_DEVICE_Init; CMake selects ours. */
void nativeUsbInit(void);

void print(const char *text);
void println(const char *text);

/* Native USB CDC only. Call serialProcess in the main loop. Prints enqueue
   complete messages in 1024 bytes of storage; a full queue drops new messages
   instead of blocking. This also permits printing before USB enumeration. */
void serialProcess(void);
uint32_t serialDroppedBytes(void);
/* Discard queued TX bytes without waiting for the host (main loop only).
   An already submitted USB packet cannot be recalled. If it ended mid-line,
   queue CRLF so the next JSON starts on a fresh line. Does not clear PC buffers. */
void serialClearTxBuffer(void);

/* Main-loop backpressure APIs: no dropping on a full queue.
   serialWrite copies up to 64 bytes and returns the number accepted.
   serialTryPrintln queues the whole text + CRLF, or returns zero to retry. */
uint32_t serialWrite(const uint8_t *data, uint32_t length);
int serialTryPrintln(const char *text);
/* Main loop: accept a complete binary frame only when the previous TX has
   finished. Returns zero without waiting; caller may replace its pending frame.
   Maximum frame size is 1024 bytes. Does not add text or line delimiters. */
int serialTryWriteFrame(const uint8_t *data, uint32_t length);

void printInt(int value);
void printFloat(float value);
void printDouble(double value);

void printlnInt(int value);
void printlnFloat(float value);
void printlnDouble(double value);

void printBin(uint32_t value);

/* Nonblocking native USB line reader: CR ignored, LF terminates, 63 chars max.
   Returned static storage is reused on the next call. */
char *serialRead(void);

#ifdef __cplusplus
}
#endif

#endif
