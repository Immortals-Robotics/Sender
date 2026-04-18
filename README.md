# Sender

Reads robot command packets from a UDP multicast group and forwards them over NRF24L01 radio.

## Dependencies

### RF24

Install the RF24 library using the official installer:

```sh
wget https://raw.githubusercontent.com/nRF24/.github/main/installer/install.sh
chmod +x install.sh
./install.sh
```

When prompted:
- Only choose to install the **RF24** library, skip the rest
- For the SPI driver, choose **SPIDEV**

## Build

Configure and build using CMake presets:

```sh
cmake --preset debug   # or release
cmake --build out
```

## Run

```sh
sudo ./out/sender
```

## Install as a Service

Builds in release mode, installs the binary to `/usr/local/bin/sender`, and registers a systemd service that starts on boot and restarts on failure.

```sh
./scripts/install.sh
```

### Service management

```sh
sudo systemctl start sender
sudo systemctl stop sender
sudo systemctl restart sender
journalctl -u sender -f   # live logs
```

### Uninstall

```sh
./scripts/uninstall.sh
```
