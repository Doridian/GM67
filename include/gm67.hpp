#pragma once

#include <Arduino.h>

#include "gm67_types.hpp"

class GM67 {
public:
    GM67(Stream &serial);
    void wake();

    GM67Payload* poll(const int timeout_ms = -1);
    GM67Barcode* scan(const int timeout_ms = -1);

    int set_trigger_mode(const GM67TriggerMode mode);
    int set_scanner_timeout(const uint8_t timeout_tenths);
    int set_data_format(const GM67DataFormat format);
    int set_packetize_data(const bool packetize);
    int set_scanner_enabled(const bool enabled);
    int set_scanning(const bool enabled);

protected:
    void send_ack();
    void send_nack_resend();
    GM67Payload* read();
    int configure(const uint8_t key, const uint8_t value);

    int send_command(const GM67Payload* payload, const bool expect_ack = true);

    int write_uint16(const uint16_t value);
    int assert_ack();
    int read_raw(const int length, uint8_t *buf);
    int write_raw(const int length, const uint8_t *buf);
    int write_one(const uint8_t buf);

private:
    Stream &serial;
    uint16_t checksum_state;
};
