# GPS Monitor Package for Omega2

## Overview

This package contains a GPS monitoring tool designed for the Omega2 device. It continuously displays GPS data retrieved from the ubus GPS service, showing real-time location information in a formatted display.

## Features

- Real-time GPS data monitoring
- JSON parsing of GPS information from ubus
- Clean, formatted display with timestamps
- Graceful exit handling (Ctrl+C)

## Package Makefile

The build and installation process is defined in the package Makefile. The package Makefile handles the compilation using OpenWrt's build system. Here's a brief overview of the key sections:

- **Package Definition**: Sets the package name, section, category, and title.
- **Dependencies**: Requires `libjson-c` for JSON parsing.
- **Build Instructions**: Specifies how to compile the program using the OpenWrt toolchain.
- **Installation Instructions**: Defines where to install the compiled binary on the Omega2 device.

## Building the Package

To build this package and create an installable `.ipk` file, follow the [C Package Example guide in the new Onion Documentation](https://documentation.onioniot.com/guides/packages/c-package-example). 

The guide provides step-by-step instructions on:
1. Setting up a Docker-based OpenWRT build environment 
1. Compiling the package
1. Transferring the compiled package binary to your Omega device
1. Installing the package using the OPKG package manager
1. Running the program

## Usage

Once installed, you can run the program by simply typing:

```bash
gps-monitor
```

The program will:
- Display GPS data retrieved from `ubus call gps info`
- Update the display every second
- Show formatted GPS information including latitude, longitude, and other available data
- Display a timestamp for each update

Press `Ctrl+C` to exit the program gracefully.

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

