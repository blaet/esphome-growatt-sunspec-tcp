# esphome-growatt-sunspec-tcp

ESPHome external component: **Growatt** inverter on **Modbus RTU** (e.g. `growatt_solar`) exposed as **SunSpec Modbus TCP** for **Victron GX** (Cerbo). Live registers come from linked sensors; Victron **Model 123** power limit maps to a configurable Growatt holding register (default **3**, active power %).

**Implemented for ESP8266** (Modbus TCP server uses `WiFiServer` / `WiFiClient`).

Developed and tested with a **Growatt 3000-S** and single-phase-style SunSpec **model 101**. Other Growatt models may need different YAML defaults or register addresses—check your protocol doc.

## Install

From GitHub (recommended):

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/blaet/esphome-growatt-sunspec-tcp.git
      ref: main
      path: esphome/components
    components: [growatt_sunspec_tcp]
```

Local checkout (symlink or sibling folder):

```yaml
external_components:
  - source:
      type: local
      path: ../esphome-growatt-sunspec-tcp/esphome/components
    components: [growatt_sunspec_tcp]
```

(`path` is relative to the YAML file directory.)

## Minimal usage

Use a `modbus` hub + `growatt_solar`, wire sensor IDs into `growatt_sunspec_tcp`, set `unit_id` to match the Cerbo SunSpec TCP device. Example wiring: `growatt_solar` sensors → `growatt_sunspec_tcp`; Cerbo adds a SunSpec Modbus TCP device (same `unit_id`, port `tcp_port`, typically 502).

## License

This repository is dedicated to the public domain under [CC0 1.0 Universal](LICENSE).
