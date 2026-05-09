# esphome-growatt-sunspec-tcp

ESPHome **external component**: bridge a **Growatt** inverter that already speaks **Modbus RTU** (via ESPHome [`growatt_solar`](https://esphome.io/components/sensor/growatt_solar.html) on a UART `modbus` hub) to **Victron GX** using **SunSpec over Modbus TCP**.

```
Growatt (RS485 RTU) ──► ESP8266 [growatt_solar] ──► sensors ──► [growatt_sunspec_tcp] ◄── Modbus TCP ── Cerbo GX
```

- **TCP side:** SunSpec-ish map at logical base **40000**, models **1**, **101**, **120**, **123** (+ end). Reads come from a RAM register image refreshed from sensors.
- **RTU side:** Same ESP acts as **Modbus master** (`modbus::ModbusDevice`) to the inverter. Writes from Cerbo into SunSpec **model 123** turn into **FC06** to a configurable **holding register** (default **3**, “active power %” on many Growatt V3.x docs).

**Platform:** TCP server path is implemented only for **ESP8266** (`WiFiServer` / `WiFiClient`). Other targets compile stub TCP handlers and log an error.

**Tested setup:** Growatt **3000-S**, **ESP8266** (`esp07s`), single-phase-style **model 101** map. Three-phase inverters usually need a different SunSpec inverter model (e.g. 103); this component does not provide that yet.

More detail: [docs/CONFIGURATION.md](docs/CONFIGURATION.md).

---

## Quick install

### A — Git submodule (site repo owns pin)

Keeps firmware and component on known commits; clone parent with:

```bash
git submodule update --init --recursive
```

YAML next to `esphome/` (path relative to **this YAML file’s directory**):

```yaml
external_components:
  - source:
      type: local
      path: esphome-growatt-sunspec-tcp/esphome/components
    components: [growatt_sunspec_tcp]
```

### B — Pull by URL (no submodule)

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/blaet/esphome-growatt-sunspec-tcp.git
      ref: main   # or a tag / SHA for reproducible builds
      path: esphome/components
    components: [growatt_sunspec_tcp]
```

---

## Minimal firmware layout

1. **`wifi`** — station mode so the ESP has an IP for Cerbo.
2. **`uart`** + **`modbus`** hub — same wiring `growatt_solar` uses.
3. **`growatt_solar`** — defines sensors (`id:` on the entities you want on the bus).
4. **`growatt_sunspec_tcp`** — same `modbus_id` / slave **`address`** as `growatt_solar`; pass sensor `id`s into the SunSpec façade.

You must extend **`modbus.modbus_device_schema`** fields on `growatt_sunspec_tcp` (typically `modbus_id`, `address: 0x01` or whatever your inverter uses).

---

## Victron Cerbo GX

1. Ensure the ESP is reachable on the LAN (ping / DHCP reservation).
2. **Settings → Services → Modbus TCP** — enable if needed.
3. Add a **SunSpec / PV inverter** style device (wording varies by Venus version): **host** = ESP IP, **port** = `tcp_port` (default 502), **unit ID** = `unit_id` (default 126).
4. Remove duplicate paths to the same inverter (e.g. old MQTT `pvinverter`) so DVCC does not see two sources.

DER active power limit from Venus maps into **model 123**; the component forwards a **0–100 %** write to Growatt holding register **`holding_register_active_power_pct`** when limit enable semantics match the Cerbo write pattern. See [docs/CONFIGURATION.md](docs/CONFIGURATION.md#power-limit-model-123--growatt-holding-register).

---

## YAML reference (summary)

| Key | Default | Notes |
|-----|---------|--------|
| `tcp_port` | 502 | Modbus TCP listen port. |
| `unit_id` | 126 | Must match Cerbo TCP slave ID. |
| `rated_power_w` | 3000 | Nameplate W (model 120). |
| `manufacturer` / `model` / `serial` | Growatt / 3000-S / … | Model 1 strings. |
| `holding_register_active_power_pct` | 3 | Growatt holding reg for active power % (protocol-dependent). |
| `min_rtu_command_gap` | 850 ms | Minimum spacing between RTU commands from this device. Match/modulate with `modbus` `turnaround_time` / `send_wait_time`. |
| `ac_voltage`, `ac_current`, `ac_power`, `frequency`, `energy_kwh`, `pv_power`, `cabinet_temp` | — | Optional `sensor` IDs from `growatt_solar` (or any source). |

Full tables and scaling: [docs/CONFIGURATION.md](docs/CONFIGURATION.md).

---

## Development

After editing files under `esphome/components/growatt_sunspec_tcp/`, run `esphome compile` from your config package. In a submodule checkout, commit inside **this** repo and push; then bump the submodule pointer in the parent firmware repo.

---

## License

[CC0 1.0 Universal](LICENSE) — public domain dedication to the extent allowed by law.
