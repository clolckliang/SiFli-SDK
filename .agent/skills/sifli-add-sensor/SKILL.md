---
name: sifli-add-sensor
description: Add or port a new sensor driver into the SiFli SDK (RT-Thread) peripherals tree. Use when asked to integrate a new sensor model, add Kconfig options, wire SConscript dependencies, implement bus init or IRQ handling, register RT-Thread sensor devices, or add finsh debug commands (e.g., "I want to add xxx sensor").
---

# SiFli Sensor Integration

## Quick start

- Identify sensor class (6-axis, temperature, pressure, magnetometer, light, GPS, motor, etc.) and bus type (I2C, SPI, UART).
- Confirm hardware: interface enabled, pins routed, voltage and frequency ranges match.
- Align with stakeholders on required features (interrupts, FIFO, pedometer, etc.).

## Workflow

1. Create a new folder under `rtos/rthread/bsp/sifli/peripherals/<sensor>` and add driver sources.
2. Update `rtos/rthread/bsp/sifli/peripherals/Kconfig`:
   - Add a top-level class switch and a model-specific enable switch.
   - Add bus selection and bus name (only if the model supports multiple bus types).
   - Add pin configs for IRQ, power, reset, etc.
3. If the user provides reference code, extract the init sequence, register defaults, FIFO/interrupt behavior, and algorithm assumptions; map them to the RT-Thread driver and record any deltas.
4. Implement bus access helpers (I2C/SPI/UART) using the Kconfig macros for bus name, address, and pins.
5. Implement IRQ setup when required, using RT-Thread pin APIs and Kconfig pin macros.
6. Add `sensor_xxx.c` to register with RT-Thread sensor framework (`rt_hw_sensor_register`) and implement `sensor_ops`, then expose a component init hook (e.g., `INIT_COMPONENT_EXPORT`) to call the register function.
7. If the sensor requires signal processing (PPG/HR/SpO2), add an algorithm module with fixed sampling rate/buffer size and wire it into `fetch_data`.
8. Add `SConscript` in the sensor folder and gate it with the model enable macro.
9. Enable and configure the module in menuconfig: `Select board peripherals` -> sensor module -> configuration.
10. Optionally add a finsh command for basic validation (open/close, reg read/write, data read).

## Conventions in this repo

- `Kconfig` is centralized under `rtos/rthread/bsp/sifli/peripherals/Kconfig`.
- The top-level `SConscript` automatically includes any child folder with its own `SConscript`.
- Each sensor folder contains its own `SConscript` that defines the build group and depends on the Kconfig macro.
- Prefer enabling one model per sensor class to avoid bus/address conflicts.

## References

- Use `references/sensor-integration.md` for Kconfig skeletons and code examples (bus access, IRQ setup, sensor registration, finsh debug).
