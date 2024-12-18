#!/usr/bin/env python3
from serial import Serial
from sys import argv
from time import sleep
from enum import Enum
from binascii import hexlify

class GM67TriggerMode(Enum):
    BUTTON_HOLDING = b"\x00"
    BUTTON_TRIGGER = b"\x02"
    CONTINUOUS_SCANNING = b"\x04"
    AUTOMATIC_INDUCTION = b"\x09"
    HOST = b"\x08"

ACK_FROM_DEVICE = b"\xd0\x00\x00"
ACK_TO_DEVICE = b"\xd0\x04\x00"
NACK_TO_DEVICE_RESEND = b"\xd1\x04\x00"

class GM67:
    port: Serial

    def __init__(self, port: Serial):
        self.port = port

    @staticmethod
    def compute_checksum(data: bytes) -> int:
        s = 0
        for b in data:
            s += b
        return 0x10000 - s
    
    def _ensure_read(self, length: int) -> bytes:
        res = self.port.read(length)
        if len(res) != length:
            raise TimeoutError("Timeout reading %d bytes" % length)
        return res
    
    def poll(self) -> bytes | None:
        try:
            data = self.read()
            self.send_command(ACK_TO_DEVICE, expect_ack=False)
            return data
        except TimeoutError:
            return None

    def read(self) -> bytes:
        # Packet structure:
        # 1 byte: length of packet (including length, excluding checksum)
        # n bytes: data
        # 2 bytes: checksum

        pktlenb = self._ensure_read(1)
        pktdata_raw = pktlenb
        pktlen = pktlenb[0]
        if pktlen == 0xFF:
            opcode_dummy = self._ensure_read(1)
            pktdata_raw += opcode_dummy
            pktlenb = self._ensure_read(2) # 2-byte length
            pktdata_raw += pktlenb
            pktdata = self._ensure_read(int.from_bytes(pktlenb, "big") - 2)
            if pktdata[0] != opcode_dummy[0]:
                raise ValueError("Unexpected opcode mismatch in multi-byte length packet: First=%02x / Second=%02x" % (opcode_dummy[0], pktdata[0]))
        else:
            pktdata = self._ensure_read(pktlen + 1)
        
        pktcsum = (pktdata[-2] << 8) | pktdata[-1]
        pktdata = pktdata[:-2]
        pktdata_raw += pktdata

        if pktcsum != GM67.compute_checksum(pktdata_raw):
            raise ValueError("Checksum mismatch: Computed=%04x / Packet=%04x" % (GM67.compute_checksum(pktdata), pktcsum))

        return pktdata

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

    def set_trigger_mode(self, mode: GM67TriggerMode) -> None:
        self.send_command(b"\xC6\x04\x08\x00\x8A" + mode.value)

    def set_packetize_data(self, enable: bool) -> None:
        self.send_command(b"\xC6\x04\x08\x00\xEE" + (b"\x01" if enable else b"\x00"))

    def set_scan_enable(self, enable: bool) -> None:
        self.send_command(b"\xE9\x04\x00\xFF\x0F" if enable else b"\x04\xEA\x04\x00")

    def set_scanner_active(self, active: bool) -> None:
        self.send_command(b"\xE4\x04\x00\xFF\x14" if active else b"\x04\xE5\x04\x00")

def main():
    port = Serial(argv[1], baudrate=115200, timeout=0.2)
    gm = GM67(port)

    gm.wake()
    gm.set_trigger_mode(GM67TriggerMode.HOST)
    gm.set_packetize_data(True)

    sleep(1)

    gm.wake()
    gm.set_scan_enable(True)
    gm.set_scanner_active(True)
    while True:
        print(hexlify(gm.poll()))

if __name__ == '__main__':
    main()
