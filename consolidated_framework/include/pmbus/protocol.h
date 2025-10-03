#ifndef PMBUS_PROTOCOL_H
#define PMBUS_PROTOCOL_H

#include "transport.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file pmbus_protocol.h
 * @brief PMBus Protocol Layer Interface
 * 
 * This module provides a unified protocol layer for PMBus communication,
 * including command processing, data conversion, and device abstraction.
 */

/* Forward declarations */
typedef struct pmbus_device pmbus_device_t;
typedef struct pmbus_protocol_engine pmbus_protocol_engine_t;

/* PMBus standard commands */
#define PMBUS_CMD_PAGE                  0x00
#define PMBUS_CMD_OPERATION             0x01
#define PMBUS_CMD_ON_OFF_CONFIG         0x02
#define PMBUS_CMD_CLEAR_FAULTS          0x03
#define PMBUS_CMD_PHASE                 0x04
#define PMBUS_CMD_PAGE_PLUS_WRITE       0x05
#define PMBUS_CMD_PAGE_PLUS_READ        0x06
#define PMBUS_CMD_WRITE_PROTECT         0x10
#define PMBUS_CMD_STORE_DEFAULT_ALL     0x11
#define PMBUS_CMD_RESTORE_DEFAULT_ALL   0x12
#define PMBUS_CMD_STORE_DEFAULT_CODE    0x13
#define PMBUS_CMD_RESTORE_DEFAULT_CODE  0x14
#define PMBUS_CMD_STORE_USER_ALL        0x15
#define PMBUS_CMD_RESTORE_USER_ALL      0x16
#define PMBUS_CMD_STORE_USER_CODE       0x17
#define PMBUS_CMD_RESTORE_USER_CODE     0x18
#define PMBUS_CMD_CAPABILITY            0x19
#define PMBUS_CMD_QUERY                 0x1A
#define PMBUS_CMD_SMBALERT_MASK         0x1B
#define PMBUS_CMD_VOUT_MODE             0x20
#define PMBUS_CMD_VOUT_COMMAND          0x21
#define PMBUS_CMD_VOUT_TRIM             0x22
#define PMBUS_CMD_VOUT_CAL_OFFSET       0x23
#define PMBUS_CMD_VOUT_MAX              0x24
#define PMBUS_CMD_VOUT_MARGIN_HIGH      0x25
#define PMBUS_CMD_VOUT_MARGIN_LOW       0x26
#define PMBUS_CMD_VOUT_TRANSITION_RATE  0x27
#define PMBUS_CMD_VOUT_DROOP            0x28
#define PMBUS_CMD_VOUT_SCALE_LOOP       0x29
#define PMBUS_CMD_VOUT_SCALE_MONITOR    0x2A
#define PMBUS_CMD_COEFFICIENTS          0x30
#define PMBUS_CMD_POUT_MAX              0x31
#define PMBUS_CMD_MAX_DUTY              0x32
#define PMBUS_CMD_FREQUENCY_SWITCH      0x33
#define PMBUS_CMD_VIN_ON                0x35
#define PMBUS_CMD_VIN_OFF               0x36
#define PMBUS_CMD_INTERLEAVE            0x37
#define PMBUS_CMD_IOUT_CAL_GAIN         0x38
#define PMBUS_CMD_IOUT_CAL_OFFSET       0x39
#define PMBUS_CMD_FAN_CONFIG_1_2        0x3A
#define PMBUS_CMD_FAN_COMMAND_1         0x3B
#define PMBUS_CMD_FAN_COMMAND_2         0x3C
#define PMBUS_CMD_FAN_CONFIG_3_4        0x3D
#define PMBUS_CMD_FAN_COMMAND_3         0x3E
#define PMBUS_CMD_FAN_COMMAND_4         0x3F
#define PMBUS_CMD_VOUT_OV_FAULT_LIMIT   0x40
#define PMBUS_CMD_VOUT_OV_FAULT_RESPONSE 0x41
#define PMBUS_CMD_VOUT_OV_WARN_LIMIT    0x42
#define PMBUS_CMD_VOUT_UV_WARN_LIMIT    0x43
#define PMBUS_CMD_VOUT_UV_FAULT_LIMIT   0x44
#define PMBUS_CMD_VOUT_UV_FAULT_RESPONSE 0x45
#define PMBUS_CMD_IOUT_OC_FAULT_LIMIT   0x46
#define PMBUS_CMD_IOUT_OC_FAULT_RESPONSE 0x47
#define PMBUS_CMD_IOUT_OC_LV_FAULT_LIMIT 0x48
#define PMBUS_CMD_IOUT_OC_LV_FAULT_RESPONSE 0x49
#define PMBUS_CMD_IOUT_OC_WARN_LIMIT    0x4A
#define PMBUS_CMD_IOUT_UC_FAULT_LIMIT   0x4B
#define PMBUS_CMD_IOUT_UC_FAULT_RESPONSE 0x4C
#define PMBUS_CMD_OT_FAULT_LIMIT        0x4F
#define PMBUS_CMD_OT_FAULT_RESPONSE     0x50
#define PMBUS_CMD_OT_WARN_LIMIT         0x51
#define PMBUS_CMD_UT_WARN_LIMIT         0x52
#define PMBUS_CMD_UT_FAULT_LIMIT        0x53
#define PMBUS_CMD_UT_FAULT_RESPONSE     0x54
#define PMBUS_CMD_VIN_OV_FAULT_LIMIT    0x55
#define PMBUS_CMD_VIN_OV_FAULT_RESPONSE 0x56
#define PMBUS_CMD_VIN_OV_WARN_LIMIT     0x57
#define PMBUS_CMD_VIN_UV_WARN_LIMIT     0x58
#define PMBUS_CMD_VIN_UV_FAULT_LIMIT    0x59
#define PMBUS_CMD_VIN_UV_FAULT_RESPONSE 0x5A
#define PMBUS_CMD_IIN_OC_FAULT_LIMIT    0x5B
#define PMBUS_CMD_IIN_OC_FAULT_RESPONSE 0x5C
#define PMBUS_CMD_IIN_OC_WARN_LIMIT     0x5D
#define PMBUS_CMD_POWER_GOOD_ON         0x5E
#define PMBUS_CMD_POWER_GOOD_OFF        0x5F
#define PMBUS_CMD_TON_DELAY             0x60
#define PMBUS_CMD_TON_RISE              0x61
#define PMBUS_CMD_TON_MAX_FAULT_LIMIT   0x62
#define PMBUS_CMD_TON_MAX_FAULT_RESPONSE 0x63
#define PMBUS_CMD_TOFF_DELAY            0x64
#define PMBUS_CMD_TOFF_FALL             0x65
#define PMBUS_CMD_TOFF_MAX_WARN_LIMIT   0x66
#define PMBUS_CMD_POUT_OP_FAULT_LIMIT   0x68
#define PMBUS_CMD_POUT_OP_FAULT_RESPONSE 0x69
#define PMBUS_CMD_POUT_OP_WARN_LIMIT    0x6A
#define PMBUS_CMD_PIN_OP_WARN_LIMIT     0x6B
#define PMBUS_CMD_STATUS_BYTE           0x78
#define PMBUS_CMD_STATUS_WORD           0x79
#define PMBUS_CMD_STATUS_VOUT           0x7A
#define PMBUS_CMD_STATUS_IOUT           0x7B
#define PMBUS_CMD_STATUS_INPUT          0x7C
#define PMBUS_CMD_STATUS_TEMPERATURE    0x7D
#define PMBUS_CMD_STATUS_CML            0x7E
#define PMBUS_CMD_STATUS_OTHER          0x7F
#define PMBUS_CMD_STATUS_MFR_SPECIFIC   0x80
#define PMBUS_CMD_STATUS_FANS_1_2       0x81
#define PMBUS_CMD_STATUS_FANS_3_4       0x82
#define PMBUS_CMD_READ_VIN              0x88
#define PMBUS_CMD_READ_IIN              0x89
#define PMBUS_CMD_READ_VCAP             0x8A
#define PMBUS_CMD_READ_VOUT             0x8B
#define PMBUS_CMD_READ_IOUT             0x8C
#define PMBUS_CMD_READ_TEMPERATURE_1    0x8D
#define PMBUS_CMD_READ_TEMPERATURE_2    0x8E
#define PMBUS_CMD_READ_TEMPERATURE_3    0x8F
#define PMBUS_CMD_READ_FAN_SPEED_1      0x90
#define PMBUS_CMD_READ_FAN_SPEED_2      0x91
#define PMBUS_CMD_READ_FAN_SPEED_3      0x92
#define PMBUS_CMD_READ_FAN_SPEED_4      0x93
#define PMBUS_CMD_READ_DUTY_CYCLE       0x94
#define PMBUS_CMD_READ_FREQUENCY        0x95
#define PMBUS_CMD_READ_POUT             0x96
#define PMBUS_CMD_READ_PIN              0x97
#define PMBUS_CMD_PMBUS_REVISION        0x98
#define PMBUS_CMD_MFR_ID                0x99
#define PMBUS_CMD_MFR_MODEL             0x9A
#define PMBUS_CMD_MFR_REVISION          0x9B
#define PMBUS_CMD_MFR_LOCATION          0x9C
#define PMBUS_CMD_MFR_DATE              0x9D
#define PMBUS_CMD_MFR_SERIAL            0x9E
#define PMBUS_CMD_APP_PROFILE_SUPPORT   0x9F
#define PMBUS_CMD_MFR_VIN_MIN           0xA0
#define PMBUS_CMD_MFR_VIN_MAX           0xA1
#define PMBUS_CMD_MFR_IIN_MAX           0xA2
#define PMBUS_CMD_MFR_PIN_MAX           0xA3
#define PMBUS_CMD_MFR_VOUT_MIN          0xA4
#define PMBUS_CMD_MFR_VOUT_MAX          0xA5
#define PMBUS_CMD_MFR_IOUT_MAX          0xA6
#define PMBUS_CMD_MFR_POUT_MAX          0xA7
#define PMBUS_CMD_MFR_TAMBIENT_MAX      0xA8
#define PMBUS_CMD_MFR_TAMBIENT_MIN      0xA9
#define PMBUS_CMD_MFR_EFFICIENCY_LL     0xAA
#define PMBUS_CMD_MFR_EFFICIENCY_HL     0xAB
#define PMBUS_CMD_MFR_MAX_TEMP_1        0xAC
#define PMBUS_CMD_MFR_MAX_TEMP_2        0xAD
#define PMBUS_CMD_MFR_MAX_TEMP_3        0xAE
#define PMBUS_CMD_IC_DEVICE_ID          0xAD
#define PMBUS_CMD_IC_DEVICE_REV         0xAE
#define PMBUS_CMD_USER_DATA_00          0xB0
#define PMBUS_CMD_USER_DATA_01          0xB1
#define PMBUS_CMD_USER_DATA_02          0xB2
#define PMBUS_CMD_USER_DATA_03          0xB3
#define PMBUS_CMD_USER_DATA_04          0xB4
#define PMBUS_CMD_USER_DATA_05          0xB5
#define PMBUS_CMD_USER_DATA_06          0xB6
#define PMBUS_CMD_USER_DATA_07          0xB7
#define PMBUS_CMD_USER_DATA_08          0xB8
#define PMBUS_CMD_USER_DATA_09          0xB9
#define PMBUS_CMD_USER_DATA_10          0xBA
#define PMBUS_CMD_USER_DATA_11          0xBB
#define PMBUS_CMD_USER_DATA_12          0xBC
#define PMBUS_CMD_USER_DATA_13          0xBD
#define PMBUS_CMD_USER_DATA_14          0xBE
#define PMBUS_CMD_USER_DATA_15          0xBF

/* Data formats */
typedef enum {
    PMBUS_DATA_FORMAT_DIRECT,
    PMBUS_DATA_FORMAT_LINEAR11,
    PMBUS_DATA_FORMAT_LINEAR16,
    PMBUS_DATA_FORMAT_VID,
    PMBUS_DATA_FORMAT_IEEE754,
    PMBUS_DATA_FORMAT_LITERAL,
    PMBUS_DATA_FORMAT_UINT8,
    PMBUS_DATA_FORMAT_UINT16,
    PMBUS_DATA_FORMAT_MANUFACTURER_SPECIFIC
} pmbus_data_format_t;

/* Command access types */
typedef enum {
    PMBUS_ACCESS_SEND_BYTE,
    PMBUS_ACCESS_READ_BYTE,
    PMBUS_ACCESS_WRITE_BYTE,
    PMBUS_ACCESS_READ_WORD,
    PMBUS_ACCESS_WRITE_WORD,
    PMBUS_ACCESS_READ_BLOCK,
    PMBUS_ACCESS_WRITE_BLOCK,
    PMBUS_ACCESS_PROCESS_CALL,
    PMBUS_ACCESS_BLOCK_PROCESS_CALL,
    PMBUS_ACCESS_RW
} pmbus_access_type_t;

/* Command classes */
typedef enum {
    PMBUS_CMD_CLASS_CONTROL,
    PMBUS_CMD_CLASS_CONFIG,
    PMBUS_CMD_CLASS_STATUS,
    PMBUS_CMD_CLASS_TELEMETRY,
    PMBUS_CMD_CLASS_LIMIT,
    PMBUS_CMD_CLASS_TIMING,
    PMBUS_CMD_CLASS_IDENTIFICATION,
    PMBUS_CMD_CLASS_MANUFACTURER,
    PMBUS_CMD_CLASS_EXTENDED
} pmbus_command_class_t;

/* Device operation modes */
typedef enum {
    PMBUS_OPERATION_OFF = 0x00,
    PMBUS_OPERATION_ON = 0x80,
    PMBUS_OPERATION_MARGIN_LOW = 0x94,
    PMBUS_OPERATION_MARGIN_HIGH = 0xA4,
    PMBUS_OPERATION_MARGIN_OFF = 0x80
} pmbus_operation_mode_t;

/* Power states */
typedef enum {
    PMBUS_POWER_STATE_OFF,
    PMBUS_POWER_STATE_ON,
    PMBUS_POWER_STATE_STANDBY,
    PMBUS_POWER_STATE_FAULT,
    PMBUS_POWER_STATE_UNKNOWN
} pmbus_power_state_t;

/* Device information */
typedef struct {
    uint16_t manufacturer_id;
    uint16_t device_id;
    uint16_t device_revision;
    uint8_t pmbus_revision;
    char manufacturer_name[16];
    char device_model[16];
    char device_revision_str[8];
    char manufacturing_date[8];
    char manufacturing_location[8];
    char serial_number[16];
    char ic_device_id[8];
    uint8_t ic_device_rev;
} pmbus_device_info_t;

/* Device capabilities */
typedef struct {
    uint32_t supported_commands[8]; /* Bitmask for commands 0-255 */
    uint32_t supported_formats;
    uint8_t max_pages;
    bool has_pec_support;
    bool has_smbus_arp;
    bool has_host_notify;
    bool has_process_call;
    bool has_block_process_call;
    bool has_group_command;
    bool has_extended_commands;
    bool voltage_mode_commands;
    bool current_mode_commands;
    bool power_mode_commands;
    bool temperature_commands;
    bool fan_commands;
    uint8_t max_block_size;
    uint16_t timing_accuracy_us;
} pmbus_device_capabilities_t;

/* Command descriptor */
typedef struct {
    uint8_t command;
    char name[32];
    pmbus_data_format_t format;
    pmbus_access_type_t access_type;
    pmbus_command_class_t command_class;
    uint8_t data_length;
    bool is_paged;
    bool is_manufacturer_specific;
    uint8_t pmbus_version;
    int32_t scaling_factor;
    int32_t min_value;
    int32_t max_value;
    int32_t default_value;
    uint32_t capabilities_mask;
    pmbus_error_t (*validation_func)(const uint8_t *data, size_t length);
    pmbus_error_t (*conversion_func)(const uint8_t *raw_data, void *converted_data);
} pmbus_command_descriptor_t;

/* Telemetry data */
typedef struct {
    int32_t voltage_in;      /* mV */
    int32_t voltage_out;     /* mV */
    int32_t current_in;      /* mA */
    int32_t current_out;     /* mA */
    int32_t power_in;        /* mW */
    int32_t power_out;       /* mW */
    int32_t temperature_1;   /* m°C */
    int32_t temperature_2;   /* m°C */
    int32_t temperature_3;   /* m°C */
    int32_t duty_cycle;      /* 0.01% */
    int32_t frequency;       /* Hz */
    int32_t efficiency;      /* 0.01% */
    uint32_t timestamp;
    uint32_t valid_mask;
} pmbus_telemetry_t;

/* Status information */
typedef struct {
    uint8_t status_byte;
    uint16_t status_word;
    uint8_t status_vout;
    uint8_t status_iout;
    uint8_t status_input;
    uint8_t status_temperature;
    uint8_t status_cml;
    uint8_t status_other;
    uint8_t status_mfr_specific;
    uint32_t timestamp;
    uint32_t fault_summary;
    uint32_t warning_summary;
} pmbus_status_t;

/* Data conversion coefficients */
typedef struct {
    int16_t m;  /* Mantissa */
    int16_t b;  /* Offset */
    int8_t R;   /* Exponent */
} pmbus_direct_coefficients_t;

typedef struct {
    int16_t mantissa;
    int8_t exponent;
} pmbus_linear_coefficients_t;

/* Device interface */
typedef struct pmbus_device_ops {
    pmbus_error_t (*identify)(pmbus_device_t *device);
    pmbus_error_t (*init_device)(pmbus_device_t *device);
    pmbus_error_t (*reset_device)(pmbus_device_t *device);
    pmbus_error_t (*soft_reset)(pmbus_device_t *device);
    bool (*cmd_supported)(pmbus_device_t *device, uint8_t cmd);
    pmbus_data_format_t (*get_cmd_format)(pmbus_device_t *device, uint8_t cmd);
    pmbus_error_t (*get_direct_coefficients)(pmbus_device_t *device, uint8_t cmd,
                                            pmbus_direct_coefficients_t *coeff);
    pmbus_error_t (*validate_command)(pmbus_device_t *device, uint8_t cmd,
                                     const uint8_t *data, uint8_t length);
    pmbus_error_t (*pre_command_hook)(pmbus_device_t *device, uint8_t cmd);
    pmbus_error_t (*post_command_hook)(pmbus_device_t *device, uint8_t cmd, pmbus_error_t result);
    pmbus_error_t (*read_telemetry)(pmbus_device_t *device, pmbus_telemetry_t *telemetry);
    pmbus_error_t (*set_operation_mode)(pmbus_device_t *device, pmbus_operation_mode_t mode);
    pmbus_error_t (*handle_fault)(pmbus_device_t *device, uint32_t fault_mask);
    pmbus_error_t (*enter_power_state)(pmbus_device_t *device, pmbus_power_state_t state);
} pmbus_device_ops_t;

/* Main device structure */
struct pmbus_device {
    pmbus_transport_t *transport;
    uint8_t address;
    pmbus_device_info_t info;
    pmbus_device_capabilities_t capabilities;
    uint8_t current_page;
    uint8_t num_pages;
    int16_t linear16_exponent;
    uint32_t supported_formats;
    const pmbus_device_ops_t *ops;
    const pmbus_command_descriptor_t *cmd_table;
    size_t cmd_table_size;
    pmbus_power_state_t power_state;
    pmbus_operation_mode_t operation_mode;
    bool initialized;
    bool fault_condition;
    void *device_data; /* Device-specific data */
};

/* Protocol engine */
struct pmbus_protocol_engine {
    pmbus_transport_t *transport;
    pmbus_device_t **devices;
    size_t device_count;
    size_t max_devices;
    
    /* Command processing */
    const pmbus_command_descriptor_t *global_cmd_table;
    size_t global_cmd_table_size;
    
    /* Data conversion */
    pmbus_error_t (*convert_linear11)(uint16_t raw, int32_t *value);
    pmbus_error_t (*convert_linear16)(uint16_t raw, int8_t exponent, int32_t *value);
    pmbus_error_t (*convert_direct)(uint16_t raw, const pmbus_direct_coefficients_t *coeff, int32_t *value);
    pmbus_error_t (*convert_ieee754)(uint16_t raw, float *value);
    
    /* Statistics */
    uint32_t commands_executed;
    uint32_t commands_failed;
    uint32_t devices_discovered;
    uint32_t faults_handled;
};

/* Protocol engine functions */
pmbus_protocol_engine_t* pmbus_protocol_engine_create(pmbus_transport_t *transport);
pmbus_error_t pmbus_protocol_engine_destroy(pmbus_protocol_engine_t *engine);
pmbus_error_t pmbus_protocol_engine_init(pmbus_protocol_engine_t *engine);
pmbus_error_t pmbus_protocol_engine_finalize(pmbus_protocol_engine_t *engine);

/* Device management functions */
pmbus_error_t pmbus_device_create(pmbus_protocol_engine_t *engine,
                                 uint8_t address,
                                 pmbus_device_t **device);
pmbus_error_t pmbus_device_destroy(pmbus_device_t *device);
pmbus_error_t pmbus_device_discover(pmbus_protocol_engine_t *engine,
                                   uint8_t address,
                                   pmbus_device_t **device);

/* Device operations */
pmbus_error_t pmbus_device_init(pmbus_device_t *device);
pmbus_error_t pmbus_device_reset(pmbus_device_t *device);
pmbus_error_t pmbus_device_get_info(pmbus_device_t *device, pmbus_device_info_t *info);
pmbus_error_t pmbus_device_get_capabilities(pmbus_device_t *device, 
                                           pmbus_device_capabilities_t *caps);
pmbus_error_t pmbus_device_set_page(pmbus_device_t *device, uint8_t page);
pmbus_error_t pmbus_device_get_page(pmbus_device_t *device, uint8_t *page);

/* Command execution functions */
pmbus_error_t pmbus_execute_command(pmbus_device_t *device,
                                   uint8_t command,
                                   const uint8_t *write_data,
                                   size_t write_length,
                                   uint8_t *read_data,
                                   size_t *read_length);

pmbus_error_t pmbus_read_command(pmbus_device_t *device,
                                uint8_t command,
                                uint8_t *data,
                                size_t *length);

pmbus_error_t pmbus_write_command(pmbus_device_t *device,
                                 uint8_t command,
                                 const uint8_t *data,
                                 size_t length);

/* High-level device functions */
pmbus_error_t pmbus_device_read_telemetry(pmbus_device_t *device, pmbus_telemetry_t *telemetry);
pmbus_error_t pmbus_device_read_status(pmbus_device_t *device, pmbus_status_t *status);
pmbus_error_t pmbus_device_set_operation_mode(pmbus_device_t *device, pmbus_operation_mode_t mode);
pmbus_error_t pmbus_device_clear_faults(pmbus_device_t *device);
pmbus_error_t pmbus_device_store_user_all(pmbus_device_t *device);
pmbus_error_t pmbus_device_restore_user_all(pmbus_device_t *device);

/* Data conversion functions */
pmbus_error_t pmbus_convert_linear11_to_int32(uint16_t raw, int32_t *value);
pmbus_error_t pmbus_convert_int32_to_linear11(int32_t value, uint16_t *raw);
pmbus_error_t pmbus_convert_linear16_to_int32(uint16_t raw, int8_t exponent, int32_t *value);
pmbus_error_t pmbus_convert_int32_to_linear16(int32_t value, int8_t exponent, uint16_t *raw);
pmbus_error_t pmbus_convert_direct_to_int32(uint16_t raw, 
                                           const pmbus_direct_coefficients_t *coeff,
                                           int32_t *value);
pmbus_error_t pmbus_convert_int32_to_direct(int32_t value,
                                           const pmbus_direct_coefficients_t *coeff,
                                           uint16_t *raw);

/* Command descriptor functions */
const pmbus_command_descriptor_t* pmbus_get_command_descriptor(uint8_t command);
bool pmbus_is_command_supported(pmbus_device_t *device, uint8_t command);
pmbus_data_format_t pmbus_get_command_format(pmbus_device_t *device, uint8_t command);

/* Utility functions */
const char* pmbus_data_format_to_string(pmbus_data_format_t format);
const char* pmbus_access_type_to_string(pmbus_access_type_t access);
const char* pmbus_command_class_to_string(pmbus_command_class_t class);
const char* pmbus_operation_mode_to_string(pmbus_operation_mode_t mode);
const char* pmbus_power_state_to_string(pmbus_power_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* PMBUS_PROTOCOL_H */
