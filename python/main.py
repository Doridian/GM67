#!/usr/bin/env python3
from serial import Serial
from sys import argv
from time import sleep
from enum import Enum
from binascii import hexlify
from dataclasses import dataclass

class GM67TriggerMode(Enum):
    BUTTON_HOLDING = b"\x00"
    BUTTON_TRIGGER = b"\x02"
    CONTINUOUS_SCANNING = b"\x04"
    AUTOMATIC_INDUCTION = b"\x09"
    HOST = b"\x08"

class GM67DataFormat(Enum):
    CODE = b"\x00"
    CODE_SUFFIX1 = b"\x01"
    CODE_SUFFIX2 = b"\x02"
    CODE_SUFFIX1_SUFFIX2 = b"\x03"
    PREFIX_CODE = b"\x04"
    PREFIX_CODE_SUFFIX1 = b"\x05"
    PREFIX_CODE_SUFFIX2 = b"\x06"
    PREFIX_CODE_SUFFIX1_SUFFIX2 = b"\x07"

ACK_FROM_DEVICE = b"\xd0\x00\x00"
ACK_TO_DEVICE = b"\xd0\x04\x00"
NACK_TO_DEVICE_RESEND = b"\xd1\x04\x00"

MULTIBYTE_OPCODES = {0xF4}

SCANNED_CODE_OPCODES = {0xF3, 0xF4}


class GM67BarcodeType(Enum):
    CODE_39 = 0x01
    CODEBAR = 0x02
    CODE_128 = 0x03
    DISCERE_2_OF_5 = 0x04
    IATA_2_OF_5 = 0x05
    INTERLEAVED_2_OF_5 = 0x06
    CODE_93 = 0x07
    UPC_A = 0x08
    UPC_A_ADDON_2 = 0x48
    UPC_A_ADDON_5 = 0x88
    UPC_E0 = 0x09
    UPC_E0_ADDON_2 = 0x49
    UPC_E0_ADDON_5 = 0x89
    EAN_8 = 0x0A
    EAN_8_ADDON_2 = 0x4A
    EAN_8_ADDON_5 = 0x8A
    EAN_13 = 0x0B
    EAN_13_ADDON_2 = 0x4B
    EAN_13_ADDON_5 = 0x8B
    CODE11 = 0x0C
    MSI = 0x0E
    GS1_128 = 0x0F
    UPC_E1 = 0x10
    UPC_E1_ADDON_2 = 0x50
    UPC_E1_ADDON_5 = 0x90
    TRIOPTIC_CODE_39 = 0x15
    BOOKLAND_EAN = 0x16
    COUPON_CODE = 0x17
    GS1_DATABAR_14 = 0x30
    GS1_DATABAR_LIMITED = 0x31
    GS1_DATABAR_EXPANDED = 0x32
    PDF417 = 0xF0
    QR = 0xF1
    DATA_MATRIX = 0xF2
    AZTEC_CODE = 0xF3
    MAXI_CODE = 0xF4
    VERI_CODE = 0xF5
    HAN_XIN = 0xF7
    AIM128 = 0xA2
    ISSN = 0xA3
    PLESSEY = 0xA4

@dataclass(kw_only=True, frozen=True, eq=True)
class GM67ScannedBarcode:
    barcode_type: GM67BarcodeType
    data: bytes

class GM67:
    port: Serial
    _checksum_state: int

    def __init__(self, port: Serial):
        self.port = port
        self._checksum_state = 0

    def _checksum_start(self):
        self._checksum_state = 0

    def _checksum_end(self) -> int:
        return self._checksum_state & 0xFFFF

    def _checksum_read(self, length: int) -> bytes:
        res = self.port.read(length)
        if len(res) != length:
            raise TimeoutError("Timeout reading %d bytes" % length)
        for b in res:
            self._checksum_state -= b
        return res

    @staticmethod
    def compute_checksum(data: bytes) -> int:
        # The real FW likely just uses a uint16_t and wraps around
        csum = 0
        for b in data:
            csum -= b
        return csum & 0xFFFF
    
    def poll(self) -> bytes | None:
        try:
            res = self.read()
            self.send_command(ACK_TO_DEVICE, expect_ack=False)
            return res
        except TimeoutError:
            return None

    # Set duration_seconds to 0 to use "passive" scanning, where the user triggers the scan
    # with the onboard button
    def scan(self, duration_seconds: float = 4.0) -> GM67ScannedBarcode | None:
        self.wake()

        orig_timeout = self.port.timeout

        if duration_seconds > 0:
            self.set_scanning_duration(int(duration_seconds * 10.0))
            self.set_scanner_active(True)
            self.port.timeout = duration_seconds + 0.1

        try:
            data = self.poll()
        finally:
            self.port.timeout = orig_timeout

        if not data:
            return None

        if data[0] in SCANNED_CODE_OPCODES:
            return GM67ScannedBarcode(barcode_type=GM67BarcodeType(data[3]), data=data[4:])

        raise ValueError(f"Unexpected data: {hexlify(data)}")

    def read(self) -> bytes:
        # Packet structure:
        # 1 byte: length of packet (including length, excluding checksum)
        # n bytes: data
        # 2 bytes: checksum

        self._checksum_start()

        pkttmp = self._checksum_read(2)
        pktlen = pkttmp[0]
        opcode = pkttmp[1]
        if pktlen == 0xFF and opcode in MULTIBYTE_OPCODES:
            pkttmp = self._checksum_read(3) # 2-byte length
            if pkttmp[2] != opcode:
                raise ValueError("Unexpected opcode mismatch in multi-byte length packet: First=%02x / Second=%02x" % (opcode, pktdata[0]))
            pktdata = self._checksum_read(int.from_bytes(pkttmp[:2], "big") - 5)
        else:
            pktdata = self._checksum_read(pktlen - 2)

        # Grab checksum before we add the checksum to the checksum etc
        computed_csum = self._checksum_end()

        pkttmp = self._checksum_read(2)
        pktcsum = (pkttmp[-2] << 8) | pkttmp[-1]

        if pktcsum != computed_csum:
            raise ValueError("Checksum mismatch: Computed=%04x / Packet=%04x" % (computed_csum, pktcsum))

        return bytes([opcode]) + pktdata

    def asset_ack(self) -> None:
        data = self.read()
        if data != ACK_FROM_DEVICE:
            raise Exception("Expected ACK, got " + hexlify(data).decode())

    def wake(self) -> None:
        self.port.write(b'\0')
        sleep(0.05)

    def send_command(self, command: bytes, expect_ack: bool = True) -> None:
        command = bytes([len(command) + 1]) + command
        command =  command + GM67.compute_checksum(command).to_bytes(2, "big")
        self.port.write(command)
        if expect_ack:
            self.asset_ack()

    def set_scanning_duration(self, tenths_seconds: int) -> None:
        self.send_command(b"\xC6\x04\x08\x00\x88" + tenths_seconds.to_bytes(1, "big"))

    def set_data_send_format(self, format: GM67DataFormat) -> None:
        self.send_command(b"\xC6\x04\x08\x00\xEB" + format.value)

    def set_trigger_mode(self, mode: GM67TriggerMode) -> None:
        self.send_command(b"\xC6\x04\x08\x00\x8A" + mode.value)

    def set_packetize_data(self, enable: bool) -> None:
        self.send_command(b"\xC6\x04\x08\x00\xEE" + (b"\x01" if enable else b"\x00"))

    def set_scan_enable(self, enable: bool) -> None:
        self.send_command(b"\xE9\x04\x00" if enable else b"\xEA\x04\x00")

    def set_scanner_active(self, active: bool) -> None:
        self.send_command(b"\xE4\x04\x00" if active else b"\xE5\x04\x00")

def main():
    port = Serial(argv[1], baudrate=115200, timeout=0.2)
    gm = GM67(port)

    gm.wake()
    gm.set_trigger_mode(GM67TriggerMode.HOST)
    gm.set_packetize_data(True)
    gm.set_data_send_format(GM67DataFormat.CODE)
    gm.set_scan_enable(True)

    sleep(1)

    while True:
        print("Scanning...")
        d = gm.scan()
        if d:
            print(d)

if __name__ == '__main__':
    main()
