# Soil Moisture Monitor — ECE3375 Lab

A bare-metal embedded C program for a DE-series FPGA board (e.g. DE1-SoC) that reads a simulated soil moisture level and alerts the user when watering is needed.

## Overview

A potentiometer wired to the onboard ADC simulates a soil moisture sensor. The program continuously reads the ADC, classifies the moisture level into one of three states, and updates the 7-segment displays and red LEDs accordingly.

## Hardware

| Peripheral | Address | Purpose |
|---|---|---|
| ADC | `0xFF204000` | Reads potentiometer (moisture sensor) |
| LEDR | `0xFF200000` | Red LEDs for visual alerts |
| HEX3–HEX0 | `0xFF200020` | 7-segment digits 0–3 |
| HEX5–HEX4 | `0xFF200030` | 7-segment digits 4–5 |

## States

The ADC produces a 12-bit value (0–4095). That value is mapped to one of three states:

| State | Condition | 7-Seg Display | LEDs |
|---|---|---|---|
| **OK** | moisture > 1820 | Raw value (e.g. `2048`) + `no` on HEX5-4 | Off |
| **WARN** | 1365 < moisture ≤ 1820 | `drY` | LED0 on |
| **PUMP** | moisture ≤ 1365 | `PEnP` | LED0 + LED1 on |

## How It Works

1. `adc_read()` selects the ADC channel, waits ~50 cycles for conversion, and returns the 12-bit result.
2. `get_state()` compares the reading against two thresholds to return a state (0, 1, or 2).
3. `display_update()` packs 7-segment patterns into the HEX registers to render the correct message.
4. `led_update()` writes to the LED register, turning on LEDs cumulatively as the state worsens.
5. `main()` runs this loop indefinitely — no OS, no interrupts.

## Building

Compile with any ARM or NIOS II cross-compiler targeting your board, e.g.:

```bash
arm-linux-gnueabihf-gcc -O1 -o labcode.elf labcode.c
```

Then load the ELF onto the board using the appropriate programmer tool (e.g. `quartus_pgm` or the DE-series monitor program).

## Files

| File | Description |
|---|---|
| `labcode.c` | Full source — ADC read, state logic, display and LED drivers |
