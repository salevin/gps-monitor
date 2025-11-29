# GPS Monitor Package for Omega2

## Overview

This package contains a GPS monitoring tool designed for the Omega2 device. It continuously displays GPS data retrieved from the ubus GPS service, showing real-time location information in a formatted display.

## Features

- **gps-monitor**: Real-time GPS data monitoring with interactive display
  - JSON parsing of GPS information from ubus
  - Clean, formatted display with timestamps
  - Graceful exit handling (Ctrl+C)
- **gps-logger**: Background daemon for logging GPS coordinates to CSV
  - Configurable logging intervals (default: 30 seconds)
  - CSV output with timestamp, coordinates, speed, elevation, and course
  - Can run as a daemon in the background

## Package Makefile

The build and installation process is defined in the package Makefile. The package Makefile handles the compilation using OpenWrt's build system. Here's a brief overview of the key sections:

- **Package Definition**: Sets the package name, section, category, and title.
- **Dependencies**: Requires `libjson-c` for JSON parsing.
- **Build Instructions**: Specifies how to compile the program using the OpenWrt toolchain.
- **Installation Instructions**: Defines where to install the compiled binary on the Omega2 device.

## Building the Package

To build this package and create an installable `.ipk` file, follow the Docker setup instructions [here](DOCKER_SETUP.md).

## Usage

### GPS Monitor (Interactive Display)

Once installed, you can run the interactive monitor by typing:

```bash
gps-monitor
```

The program will:
- Display GPS data retrieved from `ubus call gps info`
- Update the display every second
- Show formatted GPS information including latitude, longitude, and other available data
- Display a timestamp for each update

Press `Ctrl+C` to exit the program gracefully.

### GPS Logger (CSV Logging Daemon)

To log GPS coordinates to a CSV file:

```bash
# Log every 30 seconds to /tmp/gps-log.csv (default)
gps-logger

# Log every 60 seconds to a custom file
gps-logger -i 60 -o /tmp/my-gps-data.csv

# Run as background daemon with 10-second interval
gps-logger -d -i 10
```

**Options:**
- `-i, --interval <seconds>`: Logging interval in seconds (default: 30)
- `-o, --output <file>`: Output CSV file path (default: `/tmp/gps-log.csv`)
- `-d, --daemon`: Run as daemon in background
- `-h, --help`: Show help message

**CSV Output Format:**
```
timestamp,latitude,longitude,speed,elevation,course,age
2025-11-29 14:30:00,37.774929,-122.419418,0.5,10.2,180.0,1
2025-11-29 14:30:30,37.774935,-122.419420,0.3,10.5,182.5,2
```

Press `Ctrl+C` to stop the logger (when not running as daemon).

## Dependencies

- `libjson-c`: Required for parsing JSON data from the GPS service

## License

This project is licensed under the [MIT License](../LICENSE.md).

## Additional Resources

- **Onion Omega2 Documentation**: [https://documentation.onioniot.com/](https://documentation.onioniot.com/)
- **OpenWrt Package Development Guide**: [OpenWrt Wiki](https://openwrt.org/docs/guide-developer/packages)

## Contributing

Contributions are welcome! Please open an issue or submit a pull request on the GitHub repository.

---

*This README was generated to assist users in understanding and building the GPS Monitor package for the Omega2 device.*

