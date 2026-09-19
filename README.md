# HACAN for ESPHome

HACAN (Home Automation over CAN) is a custom, distributed home-automation
protocol for Classical CAN 2.0B networks and its initial ESPHome-oriented
implementation for ESP32 devices.

The project keeps the protocol library independent of ESPHome. This allows the
same frame codec and node runtime to be used by future gateways, diagnostic
tools, or implementations for other platforms.

## Documentation

The rendered specification, architecture decisions, and ESPHome implementation
notes are published at [morbic.github.io/esphome-hacan](https://morbic.github.io/esphome-hacan/).

The editable AsciiDoc sources are under [docs](docs/).

## Development

The portable C++17 library and its host tests are in
`components/hacan/protocol/`.

```sh
cmake -S components/hacan/protocol -B build/protocol
cmake --build build/protocol
ctest --test-dir build/protocol --output-on-failure
```

ESPHome integration is intentionally separate from the protocol library.
