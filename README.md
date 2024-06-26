# Raspberry Pi Pico example

[![Build status](https://github.com/stepchowfun/pico/workflows/Continuous%20integration/badge.svg?branch=main)](https://github.com/stepchowfun/pico/actions?query=branch%3Amain)

This repository contains the scaffolding for programming and debugging a [Raspberry Pi Pico or Pico W](https://www.raspberrypi.com/documentation/microcontrollers/raspberry-pi-pico.html).

It currently has a simple demo program that reads the raw angle from an AS5600 magnetic rotary position sensor over I²C and emits some output via UART and an LED.

## Instructions

### Preliminaries

Install the following tools:

- GDB (e.g., with `brew install arm-none-eabi-gdb`)
- Minicom (e.g., with `brew install minicom`)
- OpenOCD (e.g., with `brew install openocd`)
- Toast (e.g., with `brew install toast`)

Then initialize the submodules with:

   ```sh
   git submodule update --init
   (cd pico-sdk && git submodule update --init)
   ```

**Note:** It might be tempting to just run `git submodule update --init --recursive`, but that would take a long time and download more submodules than necessary.

### Build the project

You can build the project by running `toast`. This will produce two output files: `out.elf` and `out.uf2`.

### Program the Raspberry Pi Pico

There are two ways to flash the code onto the microcontroller. The first way is via USB:

1. Plug the Raspberry Pi Pico into a USB port while holding the `BOOTSEL` button.
2. Run this command, adjusting the path to the device as necessary:

   ```sh
   cp image.uf2 /Volumes/RPI-RP2/
   ```

The code is now running. To flash new code, unplug the Pico and start again.

The second way to program it is with a Raspberry Pi Debug Probe that is connected to the Pico's SWD debug port:

```sh
openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c 'adapter speed 5000; program out.elf verify reset exit'
```

If you want to restart the code without reprogramming the microcontroller, run:

```sh
openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c 'adapter speed 5000; init; reset; exit'
```

### Debug the code with a Raspberry Pi Debug Probe

To debug the code, first build the code in debug mode:

```
BUILD_TYPE=Debug toast
```

Then flash the code onto the Raspberry Pi Pico with the instructions above.

Now run the following command to start GDB servers:

```sh
openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c 'adapter speed 5000'
```

Then start GDB:

```sh
arm-none-eabi-gdb
```

Attach to one of the two cores with `core0` or `core1`. At this point the program will pause. The following commands are helpful:
- `continue` (`c`): Resume execution
- `^C`: Pause execution
- `next` (`n`): Execute one step, treating subroutines as a single step
- `step` (`s`): Execute one step, including individual steps in subroutines
- `print x` (`p x`): Print the value of the variable named `x`
- `break` (`b`): Set a breakpoint
- `delete` (`d`): Delete a breakpoint
- `disable`: Disable a breakpoint
- `enable`: Enable a breakpoint
- `list .` (`l .`): Show the code at the current location
- `where`: Show the current stack trace
- `finish`: Run the current function to completion
- `quit` (`q`): Quit

### Run a serial console

You can use a serial console to connect to the Raspberry Pi Pico or a Raspberry Pi Debug Probe connected to the Pico's UART pins:

1. Run Minicom, adjusting the path to the device as necessary:

   ```
   minicom --device /dev/tty.usbmodem*
   ```
2. Press Meta + Q to quit.
