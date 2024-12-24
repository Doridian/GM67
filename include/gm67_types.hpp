#pragma once

#include <stdint.h>

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

enum GM67BarcodeType {
    CODE_39 = 0x01,
    CODEBAR = 0x02,
    CODE_128 = 0x03,
    DISCERE_2_OF_5 = 0x04,
    IATA_2_OF_5 = 0x05,
    INTERLEAVED_2_OF_5 = 0x06,
    CODE_93 = 0x07,
    UPC_A = 0x08,
    UPC_A_ADDON_2 = 0x48,
    UPC_A_ADDON_5 = 0x88,
    UPC_E0 = 0x09,
    UPC_E0_ADDON_2 = 0x49,
    UPC_E0_ADDON_5 = 0x89,
    EAN_8 = 0x0A,
    EAN_8_ADDON_2 = 0x4A,
    EAN_8_ADDON_5 = 0x8A,
    EAN_13 = 0x0B,
    EAN_13_ADDON_2 = 0x4B,
    EAN_13_ADDON_5 = 0x8B,
    CODE11 = 0x0C,
    MSI = 0x0E,
    GS1_128 = 0x0F,
    UPC_E1 = 0x10,
    UPC_E1_ADDON_2 = 0x50,
    UPC_E1_ADDON_5 = 0x90,
    TRIOPTIC_CODE_39 = 0x15,
    BOOKLAND_EAN = 0x16,
    COUPON_CODE = 0x17,
    GS1_DATABAR_14 = 0x30,
    GS1_DATABAR_LIMITED = 0x31,
    GS1_DATABAR_EXPANDED = 0x32,
    PDF417 = 0xF0,
    QR = 0xF1,
    DATA_MATRIX = 0xF2,
    AZTEC_CODE = 0xF3,
    MAXI_CODE = 0xF4,
    VERI_CODE = 0xF5,
    HAN_XIN = 0xF7,
    AIM128 = 0xA2,
    ISSN = 0xA3,
    PLESSEY = 0xA4,
};

typedef struct GM67Barcode {
    GM67BarcodeType barcode_type;
    uint8_t length;
    uint8_t* data;
} GM67Barcode;

typedef struct GM67Response {
    GM67Opcode opcode;
    uint8_t length;
    uint8_t* data;
} GM67Response;
