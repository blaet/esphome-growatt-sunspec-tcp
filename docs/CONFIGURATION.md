# Configuration reference

## Modbus integration

`growatt_sunspec_tcp` subclasses ESPHome **`modbus::ModbusDevice`**. Alongside the keys below, set at least:

| Key | Typical value | Purpose |
|-----|----------------|---------|
| `modbus_id` | same hub as `growatt_solar` | Shared UART master. |
| `address` | `0x01` | Growatt RTU slave address (must match `growatt_solar`). |

The component registers as **Modbus client** (`final_validate_modbus_device`, role `client`): it issues writes on the RTU bus when Cerbo updates the SunSpec limit registers.

---

## Optional sensor inputs

Each optional key is an ESPHome **`sensor`** `id`. Values feed the **model 101** block (live AC / DC / energy / temperature style fields). Omit keys you do not need; registers stay at prior or uninitialized SunSpec defaults until first valid sample.

Internal scaling (sensor engineering units → raw registers):

| YAML key | Sensor expectation | Register behaviour |
|----------|-------------------|-------------------|
| `ac_voltage` | Volts | Stored as \( \text{V} \times 10 \) (unsigned). |
| `ac_current` | Amperes | Stored as \( \text{A} \times 100 \) (unsigned). |
| `ac_power` | Watts | Signed integer watts; operating state heuristic uses \|P\| vs ~40 W. |
| `frequency` | Hertz | Stored as \( \text{Hz} \times 100 \) (unsigned). |
| `energy_kwh` | Kilowatt-hours | Converted to Wh (`× 1000`), written as 32-bit pair per model 101 WH field layout. |
| `pv_power` | Watts | Mapped to DC power register (signed). |
| `cabinet_temp` | °C | Stored as \( {}^\circ\text{C} \times 10 \) (signed). |

Power factor is not taken from a sensor today; the implementation fixes a nominal PF register value for Cerbo readability.

Refresh cadence: register RAM is updated from sensors about every **200 ms** in firmware `loop()` (independent of `growatt_solar` poll interval).

---

## Timing and RTU sharing

`growatt_solar` and `growatt_sunspec_tcp` share one **`modbus`** controller. RS485 is half-duplex: configure **`modbus`** `send_wait_time` and **`turnaround_time`** so the inverter finishes driving the bus before the ESP replies.

`min_rtu_command_gap` adds extra spacing **before** this component sends an FC06 for power limit. If reads collide or you see sporadic CRC/timeouts, increase gap and/or inverter turnaround in YAML.

---

## Power limit (model 123 → Growatt holding register)

Cerbo writes SunSpec **immediate controls** (model **123**). This firmware allows writes only inside the model 123 span; other addresses reject Modbus TCP writes.

Relevant processed fields:

- **WMaxLimPct** (raw SunSpec): interpreted with **0.1 %** LSB → divide raw by 10 for percent.
- **WMaxLim_Ena**: when raw equals **1** and scaled percent &lt; ~99.5 %, the component queues a capped **0–100** % **FC06** single-register write to **`holding_register_active_power_pct`** (default holding address **3** on many Growatt Modbus V3.x sheets — confirm for your firmware).

When limit is disabled or full power is requested, queued percentage becomes **100 %**.

There is **no** independent watchdog timer in this component (unlike some other bridges); if Cerbo stops writing, the last Growatt limit remains until something changes it.

---

## SunSpec layout (logical)

- Base address **40000** (PDU addressing as used over TCP here).
- **Model 1** — device identity strings + unit id metadata.
- **Model 101** — single-phase inverter instantaneous block (telemetry from sensors).
- **Model 120** — nameplate ratings from `rated_power_w` and derived current estimate.
- **Model 123** — immediate controls template for Victron writes.
- **End** model pair.

Exact offsets are fixed in `growatt_sunspec_tcp.h` (`OFF_*`, `TOTAL_REGS`). Changing Cerbo expectations may require code changes if models or field order must differ.

---

## Troubleshooting

| Symptom | Things to check |
|---------|-----------------|
| Cerbo offline / no TCP | Wi-Fi, firewall, `tcp_port`, ESP IP; only one listener on 502. |
| Wrong values on Cerbo | Sensor `id`s and units; compare raw Modbus TCP read with ESPHome logs. |
| RTU errors after TCP writes | Increase `min_rtu_command_gap`, `turnaround_time`, or reduce polling elsewhere. |
| Limit ignored | `holding_register_active_power_pct` wrong for firmware; Cerbo not writing model 123 enable/limit fields you expect. |

TCP function codes supported: **0x03** (read holding), **0x06**, **0x10** (writes constrained to model 123 window).
