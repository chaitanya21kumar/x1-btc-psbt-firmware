# Copyright 2019 Chaitanya Kumar
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""USB device utility functions"""

import re
from typing import List
from typing_extensions import TypedDict

import hid
import semver


BB02MULTI_BOOTLOADER = "bb02-bootloader"
BB02BTC_BOOTLOADER = "bb02btc-bootloader"
BITBOX02MULTI = "X1-BTC-PSBT-Firmware"
BITBOX02BTC = "X1-BTC-PSBT-FirmwareBTC"

BITBOX02PLUS_MULTI_BOOTLOADER = "bb02p-bl-multi"
BITBOX02PLUS_BTC_BOOTLOADER = "bb02p-bl-btconly"
BITBOX02PLUS_MULTI = "bb02p-multi"
BITBOX02PLUS_BTC = "bb02p-btconly"


class TooManyFoundException(Exception):
    def __init__(self, count: int) -> None:
        super().__init__(f"Found {count} devices")


class NoneFoundException(Exception):
    def __init__(self) -> None:
        super().__init__("Found 0 devices")


class DeviceInfo(TypedDict):
    serial_number: str
    path: bytes
    product_string: str


def get_devices(product_string: str, interface_number: int = 0) -> List[DeviceInfo]:
    """
    Scans devices and returns a list of hid device info objects.
    """
    # TODO: product id is 0x2403, but 0x2402 is the id of some dev
    # device bootloaders. Can be removed in time, not needed for
    # production devices.
    # HWW/General endpoint is on interface 0
    # U2F Endpoint is on interface 1
    return [
        info
        for info in hid.enumerate()
        if info["vendor_id"] == 0x03EB
        and info["product_id"] in (0x2402, 0x2403)
        and (info["usage_page"] == 0xFFFF or info["interface_number"] == interface_number)
        and info["product_string"] == product_string
    ]


def get_device(product_string: str, interface_number: int = 0) -> DeviceInfo:
    devices = get_devices(product_string, interface_number)
    if len(devices) > 1:
        raise TooManyFoundException(len(devices))
    if not devices:
        raise NoneFoundException()
    return devices[0]


def get_x1-btc-psbt-firmwaremulti_device() -> DeviceInfo:
    return get_device(BITBOX02MULTI)


def get_x1-btc-psbt-firmwaremulti_bootloader() -> DeviceInfo:
    return get_device(BB02MULTI_BOOTLOADER)


def get_x1-btc-psbt-firmwarebtc_device() -> DeviceInfo:
    return get_device(BITBOX02BTC)


def get_x1-btc-psbt-firmwarebtc_bootloader() -> DeviceInfo:
    return get_device(BB02BTC_BOOTLOADER)


def get_x1-btc-psbt-firmwaremulti_devices() -> List[DeviceInfo]:
    return get_devices(BITBOX02MULTI)


def get_x1-btc-psbt-firmwaremulti_bootloaders() -> List[DeviceInfo]:
    return get_devices(BB02MULTI_BOOTLOADER)


def get_x1-btc-psbt-firmwarebtc_devices() -> List[DeviceInfo]:
    return get_devices(BITBOX02BTC)


def get_x1-btc-psbt-firmwarebtc_bootloaders() -> List[DeviceInfo]:
    return get_devices(BB02BTC_BOOTLOADER)


def get_any_x1-btc-psbt-firmwares() -> List[DeviceInfo]:
    """
    Searches for both btc-only and non-btc-only devices
    Returns:
        List of devices
    """
    devices = get_x1-btc-psbt-firmwaremulti_devices()
    devices.extend(get_x1-btc-psbt-firmwarebtc_devices())
    devices.extend(get_devices(BITBOX02PLUS_MULTI))
    devices.extend(get_devices(BITBOX02PLUS_BTC))
    return devices


def get_any_x1-btc-psbt-firmware() -> DeviceInfo:
    """
    Searches for both btc-only and non-btc-only devices
    Raises:
        TooManyFoundException: If more than 1 is found
    Returns:
        A device if found, otherwise None
    """
    devices = get_any_x1-btc-psbt-firmwares()
    if len(devices) > 1:
        raise TooManyFoundException(len(devices))
    if not devices:
        raise NoneFoundException()
    return devices[0]


def get_any_x1-btc-psbt-firmware_bootloaders() -> List[DeviceInfo]:
    """
    Searches for both btc-only and non-btc-only devices
    Returns:
        List of devices
    """
    devices = get_x1-btc-psbt-firmwaremulti_bootloaders()
    devices.extend(get_x1-btc-psbt-firmwarebtc_bootloaders())
    devices.extend(get_devices(BITBOX02PLUS_MULTI_BOOTLOADER))
    devices.extend(get_devices(BITBOX02PLUS_BTC_BOOTLOADER))
    return devices


def get_any_x1-btc-psbt-firmware_bootloader() -> DeviceInfo:
    """
    Searches for both btc-only and non-btc-only devices
    Raises:
        TooManyFoundException: If more than 1 is found
    Returns:
        A device if found, otherwise None
    """
    devices = get_any_x1-btc-psbt-firmware_bootloaders()
    if len(devices) > 1:
        raise TooManyFoundException(len(devices))
    if not devices:
        raise NoneFoundException
    return devices[0]


def parse_device_version(version: str) -> semver.VersionInfo:
    match = re.search(r"v([0-9]+\.[0-9]+\.[0-9]+.*)", version)
    if match is None:
        raise ValueError(f"Could not parse version string from string: {version}")

    return semver.VersionInfo.parse(match.group(1))
