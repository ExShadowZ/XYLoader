# STM32H7B0 Bootloader

Custom bootloader for **STM32H7B0VBTx** with A/B firmware update over SD card,
QSPI flash storage, EEPROM metadata, and an on-screen terminal (LCD).

The bootloader always boots from **partition A**, keeps **partition B** as a
rollback backup, and restores A from B automatically after repeated boot failures.

---

## Features

- **A/B firmware partitioning** in external QSPI flash (W25Q64, 8 MB)
- **SD card firmware update** (`UPDATE/UPD.BIN`) with FATFS
- **CRC32 integrity check** of the update image (STM32 hardware CRC)
- **Version comparison** — only newer firmware is installed
- **Automatic backup** of A → B before writing a new image to A
- **Automatic restore** B → A after 3 failed boot attempts
- **EEPROM metadata** (AT24C04 over I2C): magic, version, CRC,
  update counter, boot attempts, restore flag
- **On-screen terminal** (ST7735 LCD, 80×160) with colored log levels
- **Error logging** to SD card (`LOG/LOG.TXT`) on fatal errors
- **Safe boot chain**: memory-mapped QSPI → jump to application at `0x90000000`

---

## Hardware

| Peripheral | Interface | Details                             |
|------------|-----------|-------------------------------------|
| MCU        | —         | STM32H7B0VBTx (Cortex-M7)           |
| LCD        | SPI4      | ST7735, 0.96" 80×160, backlight PWM |
| SD card    | SDMMC1    | 4-bit, FATFS                        |
| QSPI flash | OCTOSPI1  | W25Q64 (8 MB), memory-mapped        |
| EEPROM     | I2C1      | AT24C04 (512 B)                     |
| LED        | PE3       | Status / error indication           |
| Key        | PC13      | (reserved)                          |

---

## Memory layout

### QSPI flash (8 MB)
0x90000000 ┌─────────────────────────┐
│ Partition A (4 MB) │ ← active firmware (always booted)
0x90400000 ├─────────────────────────┤
│ Partition B (4 MB) │ ← backup / rollback copy
0x90800000 └─────────────────────────┘


### EEPROM (AT24C04)

| Address | Size | Field                  |
|---------|------|------------------------|
| 0x00    | 4    | Magic (partition A)    |
| 0x10    | 1    | Version major (A)      |
| 0x20    | 1    | Version minor (A)      |
| 0x30    | 1    | Version patch (A)      |
| 0x40    | 4    | Firmware CRC (A)       |
| 0x50    | 4    | Magic (partition B)    |
| 0x60    | 1    | Version major (B)      |
| 0x70    | 1    | Version minor (B)      |
| 0x80    | 1    | Version patch (B)      |
| 0x90    | 4    | Firmware CRC (B)       |
| 0xA0    | 4    | Update counter         |
| 0xB0    | 1    | Boot attempts          |
| 0xC0    | 1    | Restore flag           |

`Magic = 0x43EAB508`

---

## Update image format (`UPDATE/UPD.BIN`)

The image is a raw firmware blob followed by a fixed footer:

```c
typedef struct {
    uint32_t magic;      // 0x43EAB508
    uint8_t  major;
    uint8_t  minor;
    uint8_t  patch;
    uint8_t  reserved;
    uint32_t crc;        // CRC32 of the firmware body (excluding footer)
} FirmwareFooter_t;

---

## Boot flow

Reset
  │
  ▼
Init: LCD → SD → QSPI → EEPROM
  │
  ▼
Restore flag set?
  ├─ yes → copy B → A, clear flag, reset
  └─ no
  │
  ▼
UPD.BIN present on SD?
  ├─ no  → JumpToApplication(A)
  └─ yes
       │
       ▼
     Read footer, check magic
       │
       ▼
     Version > current A?
       ├─ no  → JumpToApplication(A)
       └─ yes
            │
            ▼
          CRC matches footer?
            ├─ no  → abort, JumpToApplication(A)
            └─ yes
                 │
                 ▼
               Backup A → B
                 │
                 ▼
               Erase A
                 │
                 ▼
               Write new image to A
                 │
                 ▼
               Update EEPROM A metadata, bump update counter
                 │
                 ▼
               Delete UPD.BIN
                 │
                 ▼
               Reset → boot new A

Boot failure recovery
JumpToApplication() validates the reset vector at 0x90000004. If invalid:

Increment boot-attempt counter in EEPROM.

Reset and retry.

After BOOT_ATTEMPTS_MAX (3) failures → set restore flag → reset.

On next boot the restore flag triggers CopyPartitionBtoA() and reset again.

A successful boot clears the boot-attempt counter.

## Project structure

├── Core/                   STM32CubeMX generated (main, MSP, IT, HAL config)
├── Drivers/                CMSIS + STM32H7xx HAL
├── Middlewares/            FATFS
├── LCD/                    ST7735 driver
├── EPROM/                  AT24Cxx EEPROM driver
├── QSPI/                   OCTOSPI flash driver + firmware update logic
├── SD/                     SD card / FATFS wrapper
├── DTP/                    On-screen terminal (DTP_Print / DTP_PrintLine)
├── LOG/                    Log macros + error code
└── startup_stm32h7b0vbtx.s Startup / vector table

Build
Open the project in STM32CubeIDE (or import into your toolchain).

Build the Debug or Release configuration.

Flash the bootloader to internal flash (0x08000000).

The bootloader itself is a normal .elf — flash it with ST-LINK / CubeProgrammer.

Flashing a new firmware (end-user)
Format an SD card as FAT32.

Create folder UPDATE/ and copy your firmware as UPDATE/UPD.BIN
(built with FirmwareFooter_t appended and CRC computed over the body).

Insert SD, reset the board.

Watch the LCD: UPDATE → Backup A → B → Write A → DONE.

Board reboots into the new firmware.

UPD.BIN is deleted from the SD automatically.

If the new firmware fails to boot 3 times, the bootloader restores A from B
and reboots into the previous working firmware.

Log levels (on LCD)
Macro	Color	Usage
PROGRAMM_LOG_INFO(...)	White	Informational messages
PROGRAMM_LOG_OK(...)	Green	Success
PROGRAMM_LOG_WARNING(...)	Yellow	Warnings / progress
PROGRAMM_LOG_ERROR(...)	Red	Errors
PROGRAMM_LOG_PROGRESS(...)	Cyan	Progress (%, sectors, MB)
Disable all logging by commenting out #define PROGRAMM_DEBUG in LOG/LOG.h.

Error codes
ErrorCode (global in LOG.c) is set before each Error_Handler() call
and printed on the LCD + written to LOG/LOG.TXT on the SD card.

Code	Meaning
1	RCC oscillator config failed
2	RCC clock config failed
3	CRC init failed
4–6	I2C init / analog / digital filter
7	OCTOSPI init failed
8	OCTOSPI manager config failed
9	SDMMC init failed
10	SPI4 init failed
11–13	TIM1 PWM init failed
14	TIM1 break/dead-time config failed
15	EEPROM not responding
16	Unexpected fallthrough after jump
17–19	QSPI memory-mapped setup failed
20	Jump to app returned (should not happen)
21	Update_CheckAndStart returned non-zero
Notes
The bootloader always boots partition A. Partition B is a rollback
snapshot, not a second boot target.

Memory-mapped QSPI requires the MPU region for 0x90000000 to be
cacheable; the application must invalidate D-cache after flashing.

EEPROM writes are page-aligned via EEPROM_ClearPage before each field
update to avoid stale bits.

License
Internal / private project. All rights reserved.


---