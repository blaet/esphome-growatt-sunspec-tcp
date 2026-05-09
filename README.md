# Component: `growatt_sunspec_tcp`

Bridge a **Growatt** inverter that already speaks **Modbus RTU** (typically via the bundled [`growatt_solar`](https://esphome.io/components/sensor/growatt_solar.html) sensor on a [`modbus`](https://esphome.io/components/modbus.html) hub) to **Victron GX** using **SunSpec over Modbus TCP**.

This repository follows the layout described under **Git → Example of git repositories** in the ESPHome [**External Components**](https://esphome.io/components/external_components.html) documentation (`esphome/components/…`).

## Overview

```
Growatt (RS485 RTU) ──► ESP8266 [growatt_solar] ──► sensors ──► [growatt_sunspec_tcp] ◄── Modbus TCP ── Cerbo GX
```

| Role | Behaviour |
|------|-----------|
| **SunSpec (TCP)** | Logical map at base **40000**, models **1**, **101**, **120**, **123** (+ end). Values come from a RAM image refreshed from linked sensors. |
| **Growatt (RTU)** | Same chip is a **Modbus master** (`modbus::ModbusDevice`). Cerbo writes to model **123**; firmware translates limits into **FC06** on a configurable holding register (default **3**, “active power %” on many Growatt Modbus V3.x docs). |

**Platform:** Modbus TCP listen path is implemented only on **ESP8266**. Other targets log an error at runtime for TCP setup.

**Tested:** Growatt **3000-S**, **ESP8266** (`esp07s`), SunSpec **model 101** (single-phase style). Three-phase setups often need model **103** or similar; this component does not implement that yet.

## Requirements

- [**WiFi**](https://esphome.io/components/wifi.html) (station mode) so the ESP has a LAN IP for the Cerbo.
- [**UART**](https://esphome.io/components/uart.html) + [**Modbus**](https://esphome.io/components/modbus.html) hub wired to the inverter (same as `growatt_solar`).
- [**growatt_solar**](https://esphome.io/components/sensor/growatt_solar.html) (or any sensors you map) providing `id:` handles for telemetry.

## Installation

Load this repo with [**external_components**](https://esphome.io/components/external_components.html). Paths are relative to the **YAML file** that references them.

**Git (recommended for reproducible builds — pin `ref` to a tag or SHA when possible):**

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/blaet/esphome-growatt-sunspec-tcp.git
      ref: main
      path: esphome/components
    components: [growatt_sunspec_tcp]
```

**Git shorthand (same source):**

```yaml
external_components:
  - source: github://blaet/esphome-growatt-sunspec-tcp@main
    path: esphome/components
    components: [growatt_sunspec_tcp]
```

Optional: add `refresh:` per [External Components → Refresh](https://esphome.io/components/external_components.html) (ignored for `type: local`).

**Local folder / submodule** (development):

```yaml
external_components:
  - source:
      type: local
      path: esphome-growatt-sunspec-tcp/esphome/components
    components: [growatt_sunspec_tcp]
```

Repository layout (matches ESPHome docs):

```text
esphome/
└── components/
    └── growatt_sunspec_tcp/
        ├── __init__.py
        ├── growatt_sunspec_tcp.cpp
        └── growatt_sunspec_tcp.h
```

## Configuration variables

_In addition to the keys below, this component extends [`COMPONENT_SCHEMA`](https://esphome.io/components/esphome/) (e.g. `setup_priority`) and [**Modbus Device**](https://esphome.io/components/modbus.html) (`modbus_id`, `address`, …). You must set **`modbus_id`** and **`address`** to match your Growatt RTU slave._

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `tcp_port` | port | `502` | TCP port for SunSpec Modbus (Cerbo connects here). |
| `unit_id` | int | `126` | Modbus **unit ID** on TCP; must match the Cerbo SunSpec TCP device. Range 1–247. |
| `rated_power_w` | int | `3000` | Nameplate power (W) for SunSpec model **120**. |
| `manufacturer` | string | `Growatt` | Model **1** manufacturer string. |
| `model` | string | `3000-S` | Model **1** model string. |
| `serial` | string | `GROWATT-SUNSPEC` | Model **1** serial string. |
| `holding_register_active_power_pct` | int | `3` | Growatt **holding register** for active power limit (%). Protocol-specific; confirm for your firmware. |
| `min_rtu_command_gap` | time | `850ms` | Minimum delay before this component sends another RTU command (e.g. FC06 after Cerbo writes). Tune with Modbus `turnaround_time` / `send_wait_time`. |
| `ac_voltage` | [sensor ID](https://esphome.io/components/sensor/) | — | Phase voltage (V) → model **101**. |
| `ac_current` | sensor ID | — | Current (A) → model **101**. |
| `ac_power` | sensor ID | — | Power (W) → model **101**. |
| `frequency` | sensor ID | — | Frequency (Hz) → model **101**. |
| `energy_kwh` | sensor ID | — | Energy (kWh) → model **101** WH fields. |
| `pv_power` | sensor ID | — | DC/PV power (W) → model **101**. |
| `cabinet_temp` | sensor ID | — | Cabinet temperature (°C) → model **101**. |

## Example

Minimal pattern (adjust pins, `address`, and sensor wiring to match your install):

```yaml
uart:
  id: mod_bus
  tx_pin: 1
  rx_pin: 3
  baud_rate: 9600

modbus:
  id: modbus1
  uart_id: mod_bus
  send_wait_time: 400ms
  turnaround_time: 850ms

external_components:
  - source: github://blaet/esphome-growatt-sunspec-tcp@main
    path: esphome/components
    components: [growatt_sunspec_tcp]

sensor:
  - platform: growatt_solar
    # … sensors with id: for voltage, current, power, etc.

growatt_sunspec_tcp:
  modbus_id: modbus1
  address: 0x01
  tcp_port: 502
  unit_id: 126
  ac_voltage: l3_voltage
  ac_current: l3_current
  ac_power: l3_power
  frequency: l3_frequency
  energy_kwh: total_energy_production
  pv_power: pv_active_power
  cabinet_temp: inv_temp
```

## Victron Cerbo GX

1. Ensure the ESP is reachable on the LAN.
2. Enable [**Modbus TCP**](https://www.victronenergy.com/live/ccgx:modbus_tcp) on the GX if required.
3. Add a **SunSpec / PV inverter** device: **host** = ESP IP, **port** = `tcp_port`, **unit ID** = `unit_id`.
4. Avoid duplicate inverter paths (e.g. MQTT `pvinverter` + SunSpec) on the same physical unit.

DER limiting via SunSpec model **123** is described in [docs/CONFIGURATION.md](docs/CONFIGURATION.md).

## Further documentation

- [docs/CONFIGURATION.md](docs/CONFIGURATION.md) — scaling, RTU sharing, model **123** → Growatt register behaviour, troubleshooting.

## See also

- [External Components](https://esphome.io/components/external_components.html)
- [Modbus](https://esphome.io/components/modbus.html)
- [UART](https://esphome.io/components/uart.html)
- [growatt_solar Sensor](https://esphome.io/components/sensor/growatt_solar.html)
- [ESPHome contributing / standards](https://esphome.io/guides/contributing.html) (code style; configuration keys documented here are the stable YAML contract for this repo)

## Contributing

Changes belong in this repository under `esphome/components/growatt_sunspec_tcp/`. After editing, run `esphome compile` against a YAML that pulls the component (local path or git). Prefer pinning **`ref`** to a commit SHA or tag in downstream firmware when you need reproducible builds.

## License

[CC0 1.0 Universal](LICENSE).
