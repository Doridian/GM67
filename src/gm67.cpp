#include <Arduino.h>

#include "gm67.hpp"

#define MAX_PAYLOAD_LEN (0xFF - 2)

#define ACK_NACK_SIZE 2
static constexpr uint8_t ACK_FROM_DEVICE[ACK_NACK_SIZE] = { 0x00, 0x00 };
static constexpr uint8_t ACK_TO_DEVICE[ACK_NACK_SIZE] = { 0x04, 0x00 };
static constexpr uint8_t NACK_TO_DEVICE_RESEND[ACK_NACK_SIZE] = { 0x04, 0x00 };

// #define GM67_SERIAL_DEBUG Serial

#define SAFE_READ_TO_BUF(length, buf) \
    if (this->read_raw(length, buf) != length) { \
        return nullptr; \
    }

#define SAFE_WRITE_FROM_BUF(length, buf) \
    if (this->write_raw(length, buf) != length) { \
        return 0; \
    }

GM67::GM67(Stream &serial) : serial(serial) {

}

void GM67::wake() {
    serial.write((uint8_t)0x00);
    delay(50);
}

int GM67::send_command(const uint8_t opcode, const uint8_t* payload, const int payload_len) {
    return this->raw_send_command(opcode, payload, payload_len, true);
}

static inline bool is_multibyte_opcode(const GM67Opcode opcode) {
    return opcode == GM67Opcode::SCAN_LONG;
}

static inline uint16_t parse_uint16(const uint8_t* buf) {
    return (buf[0] << 8) | buf[1];
}

int GM67::write_uint16(const uint16_t value) {
    uint8_t buf[2] = {
        (uint8_t)((value >> 8) & 0xFF),
        (uint8_t)(value & 0xFF),
    };
    SAFE_WRITE_FROM_BUF(2, buf);
    return 2;
}

const GM67Response* GM67::poll() {
    if (serial.available()) {
        const GM67Response* res = this->read();
        if (res == nullptr) {
            this->send_nack_resend();
        } else {
            this->send_ack();
        }
        return res;
    }
    return nullptr;
}

// Do NOT free the return value of this!
const GM67Response* GM67::read() {
    // Packet structure:
    // 1 byte: length of packet (including length, excluding checksum)
    // n bytes: data
    // 2 bytes: checksum

    this->checksum_state = 0;

    uint8_t tmp_buf[4]; // We only read 2 or 3 bytes into this, but I like even numbers

    SAFE_READ_TO_BUF(2, tmp_buf);
    int pktlen = tmp_buf[0];
    GM67Opcode opcode = (GM67Opcode)tmp_buf[1];

    if (pktlen == 0xFF && is_multibyte_opcode(opcode)) {
        SAFE_READ_TO_BUF(3, tmp_buf); // 2-byte length
        if (tmp_buf[2] != opcode) {
#ifdef GM67_SERIAL_DEBUG
            GM67_SERIAL_DEBUG.print("Multibyte opcode mismatch. Outer=");
            GM67_SERIAL_DEBUG.print(opcode, HEX);
            GM67_SERIAL_DEBUG.print(" / Inner=");
            GM67_SERIAL_DEBUG.print(tmp_buf[2], HEX);
            GM67_SERIAL_DEBUG.println();
#endif
            return nullptr;
        }
        pktlen = parse_uint16(&tmp_buf[0]) - 5;
    } else {
        pktlen -= 2;
    }
    SAFE_READ_TO_BUF(pktlen, &this->last_response.data[0]);
    this->last_response.length = pktlen;
    this->last_response.opcode = opcode;

    // Grab checksum before we add the checksum to the checksum etc
    uint16_t computed_csum = this->checksum_state;

    SAFE_READ_TO_BUF(2, tmp_buf);
    uint16_t packet_csum = parse_uint16(&tmp_buf[0]);

    if (packet_csum != computed_csum) {
#ifdef GM67_SERIAL_DEBUG
        GM67_SERIAL_DEBUG.print("Checksum mismatch: Computed=");
        GM67_SERIAL_DEBUG.print(computed_csum, HEX);
        GM67_SERIAL_DEBUG.print(" / Packet=");
        GM67_SERIAL_DEBUG.print(packet_csum, HEX);
        GM67_SERIAL_DEBUG.println();
#endif
        return nullptr;
    }

    return &this->last_response;
}

int GM67::assert_ack() {
    const GM67Response* buf = this->read();
    if (buf == nullptr) {
        return 0;
    }
    if (memcmp(buf->data, ACK_FROM_DEVICE, ACK_NACK_SIZE) != 0) {
#ifdef GM67_SERIAL_DEBUG
        GM67_SERIAL_DEBUG.println("ACK mismatch");
#endif
        return 1;
    }
    return 0;
}

int GM67::read_raw(const int length, uint8_t *buf) {
    int read = serial.readBytes(buf, length);
    if (read != length) {
#ifdef GM67_SERIAL_DEBUG
        GM67_SERIAL_DEBUG.print("Read mismatch: Expected=");
        GM67_SERIAL_DEBUG.print(length);
        GM67_SERIAL_DEBUG.print(" / Got=");
        GM67_SERIAL_DEBUG.print(read);
        GM67_SERIAL_DEBUG.print(" / Data=");
        for (int i = 0; i < read; i++) {
            GM67_SERIAL_DEBUG.print(buf[i], HEX);
            GM67_SERIAL_DEBUG.print(" ");
        }
        GM67_SERIAL_DEBUG.println();
#endif
        return 0;
    }

    for (int i = 0; i < length; i++) {
        this->checksum_state -= buf[i];
    }

#ifdef GM67_SERIAL_DEBUG
    GM67_SERIAL_DEBUG.print("Read data: ");
    for (int i = 0; i < length; i++) {
        GM67_SERIAL_DEBUG.print(buf[i], HEX);
        GM67_SERIAL_DEBUG.print(" ");
    }
    GM67_SERIAL_DEBUG.println();
#endif

    return length;
}

int GM67::write_raw(const int length, const uint8_t *buf) {
    int written = serial.write(buf, length);
    if (written != length) {
#ifdef GM67_SERIAL_DEBUG
        GM67_SERIAL_DEBUG.print("Write mismatch: Expected=");
        GM67_SERIAL_DEBUG.print(length);
        GM67_SERIAL_DEBUG.print(" / Got=");
        GM67_SERIAL_DEBUG.print(written);
        GM67_SERIAL_DEBUG.print(" / Data=");
        for (int i = 0; i < written; i++) {
            GM67_SERIAL_DEBUG.print(buf[i], HEX);
            GM67_SERIAL_DEBUG.print(" ");
        }
        GM67_SERIAL_DEBUG.println();
#endif
        return 0;
    }
    for (int i = 0; i < length; i++) {
        this->checksum_state -= buf[i];
    }

#ifdef GM67_SERIAL_DEBUG
    GM67_SERIAL_DEBUG.print("Written data: ");
    for (int i = 0; i < length; i++) {
        GM67_SERIAL_DEBUG.print(buf[i], HEX);
        GM67_SERIAL_DEBUG.print(" ");
    }
    GM67_SERIAL_DEBUG.println();
#endif

    return length;
}

int GM67::raw_send_command(const uint8_t opcode, const uint8_t* payload, const int payload_len, const bool expect_ack) {
    if (payload_len > MAX_PAYLOAD_LEN) {
#ifdef GM67_SERIAL_DEBUG
        GM67_SERIAL_DEBUG.print("Payload too long: ");
        GM67_SERIAL_DEBUG.print(payload_len);
        GM67_SERIAL_DEBUG.println();
#endif
        return 0;
    }
    const uint8_t command_len = 2 + payload_len;
    this->checksum_state = 0;
    SAFE_WRITE_FROM_BUF(1, &command_len);
    SAFE_WRITE_FROM_BUF(1, &opcode);
    SAFE_WRITE_FROM_BUF(payload_len, payload);
    if (this->write_uint16(this->checksum_state) != 2) {
        return 0;
    }
    if (expect_ack && this->assert_ack()) {
        return 0;
    }
    return command_len;
}

void GM67::send_ack() {
    this->raw_send_command(GM67Opcode::ACK, ACK_TO_DEVICE, ACK_NACK_SIZE, false);
}

void GM67::send_nack_resend() {
    this->raw_send_command(GM67Opcode::NACK, NACK_TO_DEVICE_RESEND, ACK_NACK_SIZE, false);
}

// Commands

int GM67::set_trigger_mode(const GM67TriggerMode mode) {
    const uint8_t command[5] = {0x04, 0x08, 0x00, 0x8A, mode};
    return this->send_command(GM67Opcode::CONFIGURE, command, 5);
}