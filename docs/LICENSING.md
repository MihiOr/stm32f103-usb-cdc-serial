# Licensing and provenance

## Original files: BSD-3-Clause

Copyright (c) 2026 MihiOr. The [root BSD-3-Clause license](../LICENSE.txt)
covers only these independently authored files:

- `.gitignore`
- `README.md` and Markdown documentation in `docs/`
- `examples/main_user_code.c`
- `Libraries/NativeUsbSerial/native_usb_serial.h`
- `Libraries/NativeUsbSerial/NativeUsbSerial.cmake`

The grant does not relicense ST code or its derivatives. The repository as a
whole is not offered under an unrestricted open-source license.

## ST-derived code: existing ST terms

`Libraries/NativeUsbSerial/native_usb_serial.c` combines adapted ST HAL PCD,
USB Device core, and generated USB glue. This entire mixed implementation,
including its adaptations, is excluded from the root BSD grant. Its applicable
ST conditions remain in force.

The bundled CubeF1 package terms identify STM32F1xx HAL as BSD-3-Clause and
STM32 USB Device Library under ST's SLA. The open-source exception for HAL
does not relicense the SLA-covered USB portions.

`runtime/sysmem.c` and `runtime/syscalls.c` are unmodified ST STM32CubeIDE
MCU bare-project templates (MCU resources 2.2.400.202605220818). They are also
excluded from the root BSD grant; their original notices are retained.

Preserved licensing material:

- [Original ST component notice](../LICENSES/ST-Component-Notice.txt)
- [Library component notice](../Libraries/NativeUsbSerial/LICENSE.txt)
- [Runtime component notice](../Libraries/NativeUsbSerial/runtime/LICENSE.txt)
- [Original CubeF1 package terms](../Libraries/NativeUsbSerial/Package_license.html)

The bundled package contains SLA0048 Rev5/October 2025. Retain the applicable
copyright notices, conditions, and disclaimers when redistributing source;
include the required materials with binary distributions as specified there.
ST's name must not be used to imply endorsement.

SLA-covered code and derivative works must execute on or in combination with
processing devices manufactured by or for STMicroelectronics. They must not be
relicensed under BSD or MIT. A Blue Pill form factor or STM32 marking alone
does not establish the chip's origin. This repository grants no permission to
use SLA-covered code on non-ST clones; historical testing on an unidentified
board does not establish such permission.

CubeMX-generated HAL and middleware dependencies keep their own licenses.

## References

- [STM32CubeF1 v1.8.7 component licenses](https://github.com/STMicroelectronics/STM32CubeF1/blob/v1.8.7/LICENSE.md)
- [ST USB Device Library license](https://github.com/STMicroelectronics/stm32-mw-usb-device/blob/master/LICENSE.md)
- [ST SLA0048 reference](https://www.st.com/resource/en/license/SLA0048_STM32CubeIDE-Win.pdf)
- [BSD-3-Clause reference](https://opensource.org/license/bsd-3-clause)

Newer upstream revisions do not automatically replace the terms included with
the copied components.
