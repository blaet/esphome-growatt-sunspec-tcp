# `growatt_sunspec_tcp` — configuration details

This page supplements the root [README](../README.md) with scaling, timing, and Cerbo-specific behaviour. YAML **configuration variables** are summarized in the README table (ESPHome convention: documented keys are the user-facing contract).

## See also

- [External Components](https://esphome.io/components/external_components.html)
- [Modbus](https://esphome.io/components/modbus.html)
- [growatt_solar](https://esphome.io/components/sensor/growatt_solar.html)

## Modbus integration

`growatt_sunspec_tcp` subclasses ESPHome **`modbus::ModbusDevice`**. Configure at least:

| Name | Typical value | Description |
|------|---------------|-------------|
| `modbus_id` | same hub as `growatt_solar` | Shared UART Modbus controller. |
| `address` | `0x01` | Growatt RTU slave address (must match `growatt_solar`). |

The component validates as a Modbus **client** (`final_validate_modbus_device`, role `client`): it performs RTU writes when Cerbo updates SunSpec limit registers over TCP.

## Optional sensor inputs

Each optional YAML key is an ESPHome **`sensor`** [`id`](https://esphome.io/components/sensor/). Values feed the SunSpec **model 101** block. If a key is omitted, related registers keep prior or default content until a linked sensor publishes a valid state.

### Scaling (engineering units → registers)

| YAML key | Sensor unit | Register behaviour |
|----------|-------------|-------------------|
| `ac_voltage` | V | Unsigned, stored as `V * 10`. |
| `ac_current` | A | Unsigned, stored as `A * 100`. |
| `ac_power` | W | Signed watts; operating state uses `abs(P)` vs ~40 W heuristic. |
| `frequency` | Hz | Unsigned, stored as `Hz * 100`. |
| `energy_kwh` | kWh | Converted to Wh (`value * 1000`), written as 32-bit pair per model 101 WH layout. |
| `pv_power` | W | Signed; mapped to DC power field. |
| `cabinet_temp` | °C | Signed, stored as `°C * 10`. |

Power factor is not sourced from a sensor; firmware sets a nominal PF register for Cerbo readability.

**Refresh:** The SunSpec RAM image is updated from sensors about every **200 ms** in `loop()`, independent of `growatt_solar` `update_interval`.

## Timing and RTU sharing

`growatt_solar` and `growatt_sunspec_tcp` share one **`modbus`** controller. RS485 is half-duplex: set Modbus **`send_wait_time`** and **`turnaround_time`** so the inverter releases the bus before the ESP transmits.

**`min_rtu_command_gap`** adds spacing before this component sends **FC06** for power limiting. If you see CRC errors or collisions, increase the gap and/or Modbus turnaround settings, or reduce polling load from other readers.

## Power limit (model 123 → Growatt holding register)

Cerbo writes SunSpec **immediate controls** (model **123**). Modbus TCP writes are only accepted inside the model **123** window; other addresses are rejected.

- **WMaxLimPct** (raw): **0.1 %** LSB — divide raw by 10 for percent.
- **WMaxLim_Ena**: when raw is **1** and scaled percent &lt; ~99.5 %, firmware queues **FC06** to **`holding_register_active_power_pct`** (default **3** on many Growatt Modbus V3.x references — confirm for your inverter).

When limiting is off or full power is requested, the queued percentage becomes **100 %**.

There is **no** revert watchdog in this component: if Cerbo stops writing limits, the last Growatt limit stays until something changes it.

## SunSpec layout (logical)

- Base **40000** (PDU-style addressing used on TCP here).
- **Model 1** — identity strings + metadata.
- **Model 101** — single-phase inverter instantaneous values (from sensors).
- **Model 120** — nameplate from `rated_power_w` and derived current estimate.
- **Model 123** — immediate controls for Victron writes.
- **End** model marker.

Offsets are fixed in `growatt_sunspec_tcp.h` (`OFF_*`, `TOTAL_REGS`). Different Cerbo/SunSpec expectations may require code changes.

## Modbus TCP support

| Function | Role |
|----------|------|
| **0x03** | Read holding registers (SunSpec map). |
| **0x06** | Write single register (model 123 region). |
| **0x10** | Write multiple registers (model 123 region). |

## Troubleshooting

| Symptom | Checks |
|---------|--------|
| Cerbo cannot connect | Wi-Fi, firewall, `tcp_port`, ESP IP; single listener on 502. |
| Wrong live values | Sensor `id`s and units; compare TCP reads with ESPHome logs. |
| RTU errors after writes | Increase `min_rtu_command_gap`, `turnaround_time`, or reduce bus traffic. |
| Limit has no effect | Wrong `holding_register_active_power_pct`; Cerbo not writing expected model 123 fields. |
