![CI](https://github.com/chaitanya21kumar/x1-btc-psbt-firmware/workflows/ci/badge.svg?branch=main)

**X1-BTC-PSBT-Firmware** is a Bitcoin-only firmware for the Cypherock X1 hardware wallet.  
It provides secure key storage and transaction signing using BIP-32/39 key derivation and BIP-174 PSBT flows.  

The firmware interfaces with companion applications over USB and exposes an API for third-party tools.

## Features
- Securely receive and send Bitcoin transactions  
- Interact with the user via hardware buttons and display  
- Generate and restore BIP-39 seeds (12, 18, or 24 words)  
- Derive extended public keys (xpub) for a given keypath  
- Sign PSBTs with user confirmation prompts  
- Support for RBF (Replace-By-Fee) and CPFP (Child-Pays-For-Parent)  

## Architecture
This firmware is written in C/C++ for the Cypherock X1 microcontroller.  
It uses an ATECC608 secure element for key storage and cryptographic operations.  
Communication occurs over USB CDC and I2C interfaces.

## Build & Installation
See [BUILD.md](BUILD.md) for detailed instructions on setting up your toolchain and flashing the firmware.

## Usage
1. Compile and flash the firmware onto your Cypherock X1.  
2. Use the companion Python utility (`py/load_firmware.py`) or the BitBoxApp to install/upgrade.  
3. Interact with the device to generate/restore seeds or sign PSBTs.

## Contributing
Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting issues or pull requests.  
We welcome improvements to testing, new PSBT flows, or support for advanced features.

## License
This project is licensed under the GPL-3.0 License. See [LICENSE](LICENSE) for details.
