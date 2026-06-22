```
    ___ __  __  __     ___________    ________    ____
   / (_) /_/ /_/ /__  / ____/ ___/   / ____/ /   /  _/
  / / / __/ __/ / _ \/ /_   \__ \   / /   / /    / /  
 / / / /_/ /_/ /  __/ __/  ___/ /  / /___/ /____/ /   
/_/_/\__/\__/_/\___/_/    /____/   \____/_____/___/  

```

<img width="400" alt="FS_bb" src="https://github.com/user-attachments/assets/812e3ff2-332c-417d-b261-1688b7db0eb9" />


An interactive filesystem shell for the STM32F411CE (BlackPill) backed by a W25Q64 SPI NOR flash chip running LittleFS. A companion PC tool (`pcfstool`) lets you transfer files over the same UART connection.

## Hardware

| Peripheral | Pin  | Connection                              |
|------------|------|------------------------------------|
| UART1 TX   | PA9  | RX of USB-UART adapter |
| UART1 RX   | PA10 | TX of USB-UART adapter |
| SPI1 SCK   | PA5  | SLK of W25Q64|
| SPI1 MISO  | PA6  | D0 of W25Q64 |
| SPI1 MOSI  | PA7  | D1 of W25Q64 |
| W25Q64 CS  | PA4  | CS of W25Q64 |
| LED        | PC13 | Hardfault indicator, active low (onboard) |

UART: 115200 8N1. Connect with any serial terminal (e.g. `minicom`, `picocom`, PuTTY).

## Shell commands

| Command        | Description                                        |
|----------------|----------------------------------------------------|
| `ls [path]`    | List directory contents                            |
| `lsr [path]`   | Recursively list all files                         |
| `cat <file>`   | Print file contents                                |
| `touch <file>` | Create a new file (prompts for content, end with `EOF`) |
| `mkdir <dir>`  | Create a directory                                 |
| `rm <path>`    | Recursively remove a file or directory             |
| `cd [dir]`     | Change directory (no argument goes to `/`)         |
| `pwd`          | Print current directory                            |
| `info`         | Show filesystem disk usage                         |
| `receive`      | Receive a file from the PC (use `pcfstool --upload`) |
| `send`         | Send a file to the PC (use `pcfstool --download`) |
| `help`         | Show command list                                  |

## File transfer

The `pcfstool` tool runs on Linux and communicates with the board over the UART serial port.

### Build

```bash
gcc -o pcfstool.c pcfstool.c
```

### Upload a file to the MCU

```bash
./pcfstool --port /dev/ttyACM0 --upload /path/to/file.txt
```

The file is created in the shell's current working directory on the MCU.

### Download a file from the MCU

```bash
./pcfstool --port /dev/ttyACM0 --download
```

The tool lists all files on the flash, prompts you to enter the full path of the file to download (e.g. `/logs/data.csv`), then saves it locally under the basename.

### Transfer protocol

Both directions use 128-byte packets over UART. Each packet is acknowledged with a CRC32 (STM32 hardware CRC peripheral, polynomial 0x04C11DB7) computed over the 128 data bytes plus a 4-byte packet sequence number. The receiver verifies integrity before proceeding. On error, a 4-byte error code is sent in place of the CRC:

| Code         | Meaning                                |
|--------------|----------------------------------------|
| `0xF1F1F1F1` | Timeout waiting for packet             |
| `0xF2F2F2F2` | File already exists on flash           |
| `0xF3F3F3F3` | Could not create file on flash         |
| `0xF4F4F4F4` | Requested file does not exist          |
| `0xF5F5F5F5` | Requested path is not a regular file   |
| `0xF6F6F6F6` | Not enough space on flash              |

## Building and flashing

Requires [PlatformIO](https://platformio.org/) and an ST-Link programmer.

```bash
pio run            # build
pio run -t upload  # build and flash
```

## Dependencies

Managed automatically by PlatformIO (`platformio.ini`):

- [`STM32-pio-libs/W25Q64-flash`](https://github.com/STM32-pio-libs/W25Q64-flash) - W25Q64 driver
- [`STM32-pio-libs/W25Q64-lfs`](https://github.com/STM32-pio-libs/W25Q64-lfs) - LittleFS block device adapter for W25Q64
- [`STM32-pio-libs/littlefs`](https://github.com/STM32-pio-libs/littlefs) - LittleFS sources

