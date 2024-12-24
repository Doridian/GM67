#pragma once

#include <Arduino.h>

#include "gm67_types.hpp"

class GM67 {
public:
    GM67(Stream &serial);
    void wake();

    GM67Response* poll(const unsigned long timeout_ms);
    GM67Barcode* scan(const unsigned long timeout_ms);

    int set_trigger_mode(const GM67TriggerMode mode);

protected:
    int send_command(const uint8_t opcode, const uint8_t* payload, const int payload_len);
    void send_ack();
    void send_nack_resend();
    GM67Response* read();

    int write_uint16(const uint16_t value);
    int assert_ack();
    int read_raw(const int length, uint8_t *buf);
    int write_raw(const int length, const uint8_t *buf);
    int raw_send_command(const uint8_t opcode, const uint8_t* payload, const int payload_len, const bool expect_ack);

private:
    Stream &serial;
    uint16_t checksum_state;
};
