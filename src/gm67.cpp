#include <Arduino.h>

#include "gm67.hpp"

#define MAX_PAYLOAD_LEN (0xFF - 2)

#define TARGET_SELF 0x00
#define TARGET_SCANNER 0x04

#define UNKNOWN_NORMAL 0x00
#define UNKNOWN_CONFIGURE 0x08

#define NACK_RESEND ((uint8_t)0x01)
#define NACK_BAD_CONTEXT ((uint8_t)0x02)
#define NACK_DENIED ((uint8_t)0x06)

// #define GM67_SERIAL_DEBUG Serial

#define SAFE_READ_TO_BUF(length, buf) \
    if (this->read_raw(length, buf) != length) { \
        return nullptr; \
    }

#define SAFE_WRITE_FROM_BUF(length, buf) \
    if (this->write_raw(length, buf) != length) { \
        return 0; \
    }

#define SAFE_WRITE_ONE(c) \
    if (this->write_one(c) != 1) { \
        return 0; \
    }

GM67::GM67(Stream &serial) : serial(serial) {

}

void GM67::wake() {
    serial.write((uint8_t)0x00);
    delay(50);
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

GM67Payload* GM67::poll(const int timeout_ms) {
    if (!serial.available() && timeout_ms <= 0) {
        return nullptr;
    }

    const unsigned long timeout_old = this->serial.getTimeout();
    if (timeout_ms > 0) {
        this->serial.setTimeout(timeout_ms);
    }
    GM67Payload* resp = this->read();
    if (timeout_ms > 0) {
        this->serial.setTimeout(timeout_old);
    }

    if (resp == nullptr) {
        this->send_nack_resend();
    } else {
        this->send_ack();
    }
    return resp;
}

GM67Barcode* GM67::scan(const int timeout_ms) {
    if (timeout_ms >= 0) {
        if (timeout_ms > 0) {
            this->set_scanner_timeout((uint8_t)(timeout_ms / 100));
        }
        this->set_scanning(true);
    }
    GM67Payload* resp = this->poll(timeout_ms);
    if (resp == nullptr) {
        return nullptr;
    }

    // Clearly, we didn't get a poll from a scan, so drop it on the floor
    if (resp->opcode != GM67Opcode::SCAN_LONG && resp->opcode != GM67Opcode::SCAN_SHORT) {
        free(resp);
        return nullptr;
    }

    const int length = resp->length - 1;
    // How do we make this a single allocation? That's right, we're gonna cheat here, too!
    GM67Barcode* barcode = (GM67Barcode*)malloc(sizeof(GM67Barcode) + length);
    barcode->data = ((uint8_t*)barcode) + sizeof(GM67Barcode);
    barcode->barcode_type = (GM67BarcodeType)resp->data[0];
    barcode->length = length;
    memcpy(barcode->data, &resp->data[1], length);

    free(resp);

    return barcode;
}

// Do NOT free the return value of this!
GM67Payload* GM67::read() {
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
        pktlen = parse_uint16(&tmp_buf[0]) - 3;
    }
    pktlen -= 4;

    // How do we make this a single allocation? That's right, we're gonna cheat!
    GM67Payload *resp = (GM67Payload*)malloc(sizeof(GM67Payload) + pktlen);
    resp->data = ((uint8_t*)resp) + sizeof(GM67Payload);

    SAFE_READ_TO_BUF(2, tmp_buf);
    resp->target = tmp_buf[0];
    resp->unknown = tmp_buf[1];
    SAFE_READ_TO_BUF(pktlen, &resp->data[0]);
    resp->length = pktlen;
    resp->opcode = opcode;

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

    return resp;
}

int GM67::assert_ack() {
    GM67Payload* resp = this->read();
    if (resp == nullptr) {
        return 0;
    }

    int cmpres = resp->opcode == GM67Opcode::ACK ? 0 : 1;
    free(resp);
    return cmpres;
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

int GM67::write_one(const uint8_t buf) {
    if (this->serial.write(buf) != 1) {
        return 0;
    }
    this->checksum_state -= buf;
    return 1;
}

int GM67::write_raw(const int length, const uint8_t *buf) {
    int written = this->serial.write(buf, length);
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

int GM67::send_command(const GM67Payload* payload, const bool expect_ack) {
    if (payload->length > MAX_PAYLOAD_LEN) {
#ifdef GM67_SERIAL_DEBUG
        GM67_SERIAL_DEBUG.print("Payload too long: ");
        GM67_SERIAL_DEBUG.print(payload_len);
        GM67_SERIAL_DEBUG.println();
#endif
        return 0;
    }
    this->checksum_state = 0;
    SAFE_WRITE_ONE(4 + payload->length);
    SAFE_WRITE_ONE((uint8_t)payload->opcode);
    SAFE_WRITE_ONE(payload->target);
    SAFE_WRITE_ONE(payload->unknown);
    if (payload->length > 0) {
        SAFE_WRITE_FROM_BUF(payload->length, payload->data);
    }
    if (this->write_uint16(this->checksum_state) != 2) {
        return 0;
    }
    if (expect_ack && this->assert_ack()) {
        return 0;
    }
    return payload->length;
}

// Basic payloads

void GM67::send_ack() {
    GM67Payload payload = {
        .opcode = GM67Opcode::ACK,
        .target = TARGET_SCANNER,
        .unknown = UNKNOWN_NORMAL,
        .length = 0,
        .data = nullptr,
    };
    this->send_command(&payload, false);
}

void GM67::send_nack_resend() {
    uint8_t data[] = { NACK_RESEND };
    GM67Payload payload = {
        .opcode = GM67Opcode::NACK,
        .target = TARGET_SCANNER,
        .unknown = UNKNOWN_NORMAL,
        .length = 1,
        .data = data,
    };
    this->send_command(&payload, false);
}

int GM67::configure(const uint8_t key, const uint8_t value) {
    uint8_t data[] = { 0x00, key, value };
    GM67Payload payload = {
        .opcode = GM67Opcode::CONFIGURE,
        .target = TARGET_SCANNER,
        .unknown = UNKNOWN_CONFIGURE,
        .length = 1,
        .data = data,
    };
    return this->send_command(&payload, false);
}

// Commands

int GM67::set_trigger_mode(const GM67TriggerMode mode) {
    return this->configure(0x8A, mode);
}

int GM67::set_scanner_timeout(const uint8_t timeout_tenths) {
    return this->configure(0x88, timeout_tenths);
}

int GM67::set_data_format(const GM67DataFormat format) {
    return this->configure(0xEB, format);
}

int GM67::set_packetize_data(const bool packetize) {
    return this->configure(0xEE, packetize ? 0x01 : 0x00);
}

int GM67::set_scanner_enabled(const bool enabled) {
    GM67Payload payload = {
        .opcode = enabled ? GM67Opcode::ENABLE_SCANNER : GM67Opcode::DISABLE_SCANNER,
        .target = TARGET_SCANNER,
        .unknown = UNKNOWN_NORMAL,
        .length = 0,
        .data = nullptr,
    };
    return this->send_command(&payload, false);
}

int GM67::set_scanning(const bool enabled) {
    GM67Payload payload = {
        .opcode = enabled ? GM67Opcode::START_SCAN : GM67Opcode::STOP_SCAN,
        .target = TARGET_SCANNER,
        .unknown = UNKNOWN_NORMAL,
        .length = 0,
        .data = nullptr,
    };
    return this->send_command(&payload, false);
}
