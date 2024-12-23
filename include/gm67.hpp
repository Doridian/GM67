#pragma once

enum GM67TriggerMode {
    BUTTON_HOLDING = 0x00,
    BUTTON_TRIGGER = 0x02,
    CONTINUOUS_SCANNING = 0x04,
    AUTOMATIC_INDUCTION = 0x09,
    HOST = 0x08,
};

enum GM67DataFormat {
    CODE = 0x00,
    CODE_SUFFIX1 = 0x01,
    CODE_SUFFIX2 = 0x02,
    CODE_SUFFIX1_SUFFIX2 = 0x03,
    PREFIX_CODE = 0x04,
    PREFIX_CODE_SUFFIX1 = 0x05,
    PREFIX_CODE_SUFFIX2 = 0x06,
    PREFIX_CODE_SUFFIX1_SUFFIX2 = 0x07,
};

enum GM67Opcode {
    ACK = 0xD0,
    NACK = 0xD1,
    CONFIGURE = 0xC6,
    SCAN_SHORT = 0xF3,
    SCAN_LONG = 0xF4,
};

typedef struct GM67Response {
    GM67Opcode opcode;
    uint8_t length;
    uint8_t data[0xFFFF];
} GM67Response;

class GM67 {
public:
    GM67(Stream &serial);
    void wake();
    int send_command(const uint8_t opcode, const uint8_t* payload, const int payload_len);
    int set_trigger_mode(const GM67TriggerMode mode);
    const GM67Response* poll();

protected:
    void send_ack();
    void send_nack_resend();
    const GM67Response* read();
    int write_uint16(const uint16_t value);
    int assert_ack();
    int read_raw(const int length, uint8_t *buffer);
    int write_raw(const int length, const uint8_t *buffer);
    int raw_send_command(const uint8_t opcode, const uint8_t* payload, const int payload_len, const bool expect_ack);

private:
    Stream &serial;
    uint16_t checksum_state;
    GM67Response last_response;
};
