/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026 MihiOr. See the repository LICENSE.txt. */
/* Integration snippets for CubeMX main.c. This is not a standalone firmware.
   Enable USB Device FS + CDC, a 48 MHz USB clock, and add the CMake helper. */

/* Put inside USER CODE BEGIN Includes: */
#include "native_usb_serial.h"

/* Put inside USER CODE BEGIN 2, after the generated MX_USB_DEVICE_Init(): */
/*
uint32_t lastAlive = HAL_GetTick();
*/

/* Put inside the existing main while(1), in USER CODE BEGIN WHILE: */
/*
serialProcess();

char *line = serialRead();
if (line != NULL) println(line);

if ((uint32_t)(HAL_GetTick() - lastAlive) >= 1000U) {
    lastAlive = HAL_GetTick();
    println("Alive...");
}
*/
