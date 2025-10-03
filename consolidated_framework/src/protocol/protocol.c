#include "pmbus/protocol.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/**
 * @file protocol.c
 * @brief PMBus Protocol Layer Implementation
 */

/* Global command table with standard PMBus commands */
static const pmbus_command_descriptor_t global_command_table[] = {
    {PMBUS_CMD_PAGE, "PAGE", PMBUS_DATA_FORMAT_UINT8, PMBUS_ACCESS_RW, 
     PMBUS_CMD_CLASS_CONTROL, 1, false, false, 1, 1, 0, 255, 0, 0, NULL, NULL},
    {PMBUS_CMD_OPERATION, "OPERATION", PMBUS_DATA_FORMAT_UINT8, PMBUS_ACCESS_RW,
     PMBUS_CMD_CLASS_CONTROL, 1, true, false, 1, 1, 0, 255, 0x80, 0, NULL, NULL},
    {PMBUS_CMD_ON_OFF_CONFIG, "ON_OFF_CONFIG", PMBUS_DATA_FORMAT_UINT8, PMBUS_ACCESS_RW,
     PMBUS_CMD_CLASS_CONFIG, 1, true, false, 1, 1, 0, 255, 0x16, 0, NULL, NULL},
    {PMBUS_CMD_CLEAR_FAULTS, "CLEAR_FAULTS", PMBUS_DATA_FORMAT_LITERAL, PMBUS_ACCESS_SEND_BYTE,
     PMBUS_CMD_CLASS_CONTROL, 0, false, false, 1, 1, 0, 0, 0, 0, NULL, NULL},
    {PMBUS_CMD_WRITE_PROTECT, "WRITE_PROTECT", PMBUS_DATA_FORMAT_UINT8, PMBUS_ACCESS_RW,
     PMBUS_CMD_CLASS_CONFIG, 1, false, false, 1, 1, 0, 255, 0x00, 0, NULL, NULL},
    {PMBUS_CMD_VOUT_MODE, "VOUT_MODE", PMBUS_DATA_FORMAT_UINT8, PMBUS_ACCESS_READ_BYTE,
     PMBUS_CMD_CLASS_CONFIG, 1, true, false, 1, 1, 0, 255, 0x16, 0, NULL, NULL},
    {PMBUS_CMD_VOUT_COMMAND, "VOUT_COMMAND", PMBUS_DATA_FORMAT_LINEAR16, PMBUS_ACCESS_RW,
     PMBUS_CMD_CLASS_CONTROL, 2, true, false, 1, 1000, 0, 65535, 0, 0, NULL, NULL},
    {PMBUS_CMD_VOUT_MAX, "VOUT_MAX", PMBUS_DATA_FORMAT_LINEAR16, PMBUS_ACCESS_RW,
     PMBUS_CMD_CLASS_LIMIT, 2, true, false, 1, 1000, 0, 65535, 0, 0, NULL, NULL},
    {PMBUS_CMD_STATUS_BYTE, "STATUS_BYTE", PMBUS_DATA_FORMAT_UINT8, PMBUS_ACCESS_READ_BYTE,
     PMBUS_CMD_CLASS_STATUS, 1, false, false, 1, 1, 0, 255, 0, 0, NULL, NULL},
    {PMBUS_CMD_STATUS_WORD, "STATUS_WORD", PMBUS_DATA_FORMAT_UINT16, PMBUS_ACCESS_READ_WORD,
     PMBUS_CMD_CLASS_STATUS, 2, false, false, 1, 1, 0, 65535, 0, 0, NULL, NULL},
    {PMBUS_CMD_READ_VIN, "READ_VIN", PMBUS_DATA_FORMAT_LINEAR11, PMBUS_ACCESS_READ_WORD,
     PMBUS_CMD_CLASS_TELEMETRY, 2, true, false, 1, 1000, 0, 65535, 0, 0, NULL, NULL},
    {PMBUS_CMD_READ_VOUT, "READ_VOUT", PMBUS_DATA_FORMAT_LINEAR16, PMBUS_ACCESS_READ_WORD,
     PMBUS_CMD_CLASS_TELEMETRY, 2, true, false, 1, 1000, 0, 65535, 0, 0, NULL, NULL},
    {PMBUS_CMD_READ_IOUT, "READ_IOUT", PMBUS_DATA_FORMAT_LINEAR11, PMBUS_ACCESS_READ_WORD,
     PMBUS_CMD_CLASS_TELEMETRY, 2, true, false, 1, 1000, 0, 65535, 0, 0, NULL, NULL},
    {PMBUS_CMD_READ_TEMPERATURE_1, "READ_TEMPERATURE_1", PMBUS_DATA_FORMAT_LINEAR11, PMBUS_ACCESS_READ_WORD,
     PMBUS_CMD_CLASS_TELEMETRY, 2, true, false, 1, 1000, -40000, 150000, 0, 0, NULL, NULL},
    {PMBUS_CMD_READ_POUT, "READ_POUT", PMBUS_DATA_FORMAT_LINEAR11, PMBUS_ACCESS_READ_WORD,
     PMBUS_CMD_CLASS_TELEMETRY, 2, true, false, 1, 1000, 0, 65535, 0, 0, NULL, NULL},
    {PMBUS_CMD_READ_PIN, "READ_PIN", PMBUS_DATA_FORMAT_LINEAR11, PMBUS_ACCESS_READ_WORD,
     PMBUS_CMD_CLASS_TELEMETRY, 2, true, false, 1, 1000, 0, 65535, 0, 0, NULL, NULL},
    {PMBUS_CMD_MFR_ID, "MFR_ID", PMBUS_DATA_FORMAT_LITERAL, PMBUS_ACCESS_READ_BLOCK,
     PMBUS_CMD_CLASS_IDENTIFICATION, 16, false, false, 1, 1, 0, 255, 0, 0, NULL, NULL},
    {PMBUS_CMD_MFR_MODEL, "MFR_MODEL", PMBUS_DATA_FORMAT_LITERAL, PMBUS_ACCESS_READ_BLOCK,
     PMBUS_CMD_CLASS_IDENTIFICATION, 16, false, false, 1, 1, 0, 255, 0, 0, NULL, NULL},
    {PMBUS_CMD_MFR_REVISION, "MFR_REVISION", PMBUS_DATA_FORMAT_LITERAL, PMBUS_ACCESS_READ_BLOCK,
     PMBUS_CMD_CLASS_IDENTIFICATION, 8, false, false, 1, 1, 0, 255, 0, 0, NULL, NULL}
};

static const size_t global_command_table_size = sizeof(global_command_table) / sizeof(pmbus_command_descriptor_t);

/* Internal helper functions */
static pmbus_error_t discover_device_capabilities(pmbus_device_t *device);
static pmbus_error_t read_device_identification(pmbus_device_t *device);
static pmbus_error_t validate_device_response(pmbus_device_t *device, uint8_t command, 
                                             const uint8_t *data, size_t length);

/* Data conversion helper functions */
static int8_t extract_linear11_exponent(uint16_t raw);
static int16_t extract_linear11_mantissa(uint16_t raw);
static uint16_t pack_linear11(int16_t mantissa, int8_t exponent);

/* Protocol engine functions */
pmbus_protocol_engine_t* pmbus_protocol_engine_create(pmbus_transport_t *transport) {
    if (transport == NULL) {
        return NULL;
    }
    
    pmbus_protocol_engine_t *engine = calloc(1, sizeof(pmbus_protocol_engine_t));
    if (engine == NULL) {
        return NULL;
    }
    
    engine->transport = transport;
    engine->max_devices = 64; /* Default maximum devices */
    engine->devices = calloc(engine->max_devices, sizeof(pmbus_device_t*));
    if (engine->devices == NULL) {
        free(engine);
        return NULL;
    }
    
    engine->global_cmd_table = global_command_table;
    engine->global_cmd_table_size = global_command_table_size;
    
    /* Set up default data conversion functions */
    engine->convert_linear11 = pmbus_convert_linear11_to_int32;
    engine->convert_linear16 = pmbus_convert_linear16_to_int32;
    engine->convert_direct = pmbus_convert_direct_to_int32;
    
    return engine;
}

pmbus_error_t pmbus_protocol_engine_destroy(pmbus_protocol_engine_t *engine) {
    if (engine == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    /* Destroy all devices */
    for (size_t i = 0; i < engine->device_count; i++) {
        if (engine->devices[i] != NULL) {
            pmbus_device_destroy(engine->devices[i]);
        }
    }
    
    free(engine->devices);
    free(engine);
    return PMBUS_SUCCESS;
}

pmbus_error_t pmbus_protocol_engine_init(pmbus_protocol_engine_t *engine) {
    if (engine == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    return PMBUS_SUCCESS;
}

pmbus_error_t pmbus_protocol_engine_finalize(pmbus_protocol_engine_t *engine) {
    if (engine == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    return PMBUS_SUCCESS;
}

/* Device management functions */
pmbus_error_t pmbus_device_create(pmbus_protocol_engine_t *engine,
                                 uint8_t address,
                                 pmbus_device_t **device) {
    if (engine == NULL || device == NULL || address > 0x7F) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    if (engine->device_count >= engine->max_devices) {
        return PMBUS_ERROR_NO_MEMORY;
    }
    
    pmbus_device_t *new_device = calloc(1, sizeof(pmbus_device_t));
    if (new_device == NULL) {
        return PMBUS_ERROR_NO_MEMORY;
    }
    
    new_device->transport = engine->transport;
    new_device->address = address;
    new_device->cmd_table = engine->global_cmd_table;
    new_device->cmd_table_size = engine->global_cmd_table_size;
    new_device->power_state = PMBUS_POWER_STATE_UNKNOWN;
    new_device->operation_mode = PMBUS_OPERATION_OFF;
    new_device->linear16_exponent = -9; /* Default for voltage */
    
    engine->devices[engine->device_count++] = new_device;
    *device = new_device;
    
    return PMBUS_SUCCESS;
}

pmbus_error_t pmbus_device_destroy(pmbus_device_t *device) {
    if (device == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    free(device);
    return PMBUS_SUCCESS;
}

pmbus_error_t pmbus_device_discover(pmbus_protocol_engine_t *engine,
                                   uint8_t address,
                                   pmbus_device_t **device) {
    if (engine == NULL || device == NULL || address > 0x7F) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    /* Try to read STATUS_BYTE to verify device presence */
    uint8_t status;
    pmbus_error_t result = pmbus_read_byte(engine->transport, address, &status);
    if (result != PMBUS_SUCCESS) {
        return PMBUS_ERROR_DEVICE_NOT_FOUND;
    }
    
    /* Create device */
    result = pmbus_device_create(engine, address, device);
    if (result != PMBUS_SUCCESS) {
        return result;
    }
    
    /* Discover device capabilities and information */
    result = discover_device_capabilities(*device);
    if (result != PMBUS_SUCCESS) {
        pmbus_device_destroy(*device);
        *device = NULL;
        return result;
    }
    
    result = read_device_identification(*device);
    if (result != PMBUS_SUCCESS) {
        /* Non-fatal error - continue with basic device */
    }
    
    (*device)->initialized = true;
    engine->devices_discovered++;
    
    return PMBUS_SUCCESS;
}

/* Device operations */
pmbus_error_t pmbus_device_init(pmbus_device_t *device) {
    if (device == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    if (device->ops && device->ops->init_device) {
        return device->ops->init_device(device);
    }
    
    /* Default initialization - clear faults and read basic info */
    pmbus_error_t result = pmbus_device_clear_faults(device);
    if (result != PMBUS_SUCCESS) {
        return result;
    }
    
    device->initialized = true;
    return PMBUS_SUCCESS;
}

pmbus_error_t pmbus_device_reset(pmbus_device_t *device) {
    if (device == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    if (device->ops && device->ops->reset_device) {
        return device->ops->reset_device(device);
    }
    
    /* Default reset - not all devices support this */
    return PMBUS_ERROR_UNSUPPORTED;
}

pmbus_error_t pmbus_device_set_page(pmbus_device_t *device, uint8_t page) {
    if (device == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    if (page >= device->num_pages && device->num_pages > 0) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    if (device->current_page == page) {
        return PMBUS_SUCCESS; /* Already on the correct page */
    }
    
    pmbus_error_t result = pmbus_write_byte(device->transport, device->address, 
                                          PMBUS_CMD_PAGE, page);
    if (result == PMBUS_SUCCESS) {
        device->current_page = page;
    }
    
    return result;
}

pmbus_error_t pmbus_device_get_page(pmbus_device_t *device, uint8_t *page) {
    if (device == NULL || page == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    pmbus_error_t result = pmbus_read_byte(device->transport, device->address, page);
    if (result == PMBUS_SUCCESS) {
        device->current_page = *page;
    }
    
    return result;
}

/* Command execution functions */
pmbus_error_t pmbus_read_command(pmbus_device_t *device,
                                uint8_t command,
                                uint8_t *data,
                                size_t *length) {
    if (device == NULL || data == NULL || length == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    const pmbus_command_descriptor_t *cmd_desc = pmbus_get_command_descriptor(command);
    if (cmd_desc == NULL) {
        return PMBUS_ERROR_UNSUPPORTED;
    }
    
    pmbus_error_t result;
    
    switch (cmd_desc->access_type) {
        case PMBUS_ACCESS_READ_BYTE:
            if (*length < 1) return PMBUS_ERROR_INVALID_PARAM;
            result = pmbus_read_byte(device->transport, device->address, data);
            *length = (result == PMBUS_SUCCESS) ? 1 : 0;
            break;
            
        case PMBUS_ACCESS_READ_WORD: {
            if (*length < 2) return PMBUS_ERROR_INVALID_PARAM;
            uint16_t word_data;
            result = pmbus_read_word(device->transport, device->address, command, &word_data);
            if (result == PMBUS_SUCCESS) {
                data[0] = (uint8_t)(word_data & 0xFF);
                data[1] = (uint8_t)((word_data >> 8) & 0xFF);
                *length = 2;
            } else {
                *length = 0;
            }
            break;
        }
        
        case PMBUS_ACCESS_READ_BLOCK:
            result = pmbus_read_block(device->transport, device->address, command, data, length);
            break;
            
        default:
            return PMBUS_ERROR_UNSUPPORTED;
    }
    
    return result;
}

pmbus_error_t pmbus_write_command(pmbus_device_t *device,
                                 uint8_t command,
                                 const uint8_t *data,
                                 size_t length) {
    if (device == NULL || data == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    const pmbus_command_descriptor_t *cmd_desc = pmbus_get_command_descriptor(command);
    if (cmd_desc == NULL) {
        return PMBUS_ERROR_UNSUPPORTED;
    }
    
    pmbus_error_t result;
    
    switch (cmd_desc->access_type) {
        case PMBUS_ACCESS_SEND_BYTE:
            if (length != 1) return PMBUS_ERROR_INVALID_PARAM;
            result = pmbus_send_byte(device->transport, device->address, data[0]);
            break;
            
        case PMBUS_ACCESS_WRITE_BYTE:
            if (length != 1) return PMBUS_ERROR_INVALID_PARAM;
            result = pmbus_write_byte(device->transport, device->address, command, data[0]);
            break;
            
        case PMBUS_ACCESS_WRITE_WORD:
            if (length != 2) return PMBUS_ERROR_INVALID_PARAM;
            {
                uint16_t word_data = data[0] | (data[1] << 8);
                result = pmbus_write_word(device->transport, device->address, command, word_data);
            }
            break;
            
        case PMBUS_ACCESS_WRITE_BLOCK:
            result = pmbus_write_block(device->transport, device->address, command, data, length);
            break;
            
        default:
            return PMBUS_ERROR_UNSUPPORTED;
    }
    
    return result;
}

/* High-level device functions */
pmbus_error_t pmbus_device_clear_faults(pmbus_device_t *device) {
    if (device == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    return pmbus_send_byte(device->transport, device->address, PMBUS_CMD_CLEAR_FAULTS);
}

pmbus_error_t pmbus_device_set_operation_mode(pmbus_device_t *device, pmbus_operation_mode_t mode) {
    if (device == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    pmbus_error_t result = pmbus_write_byte(device->transport, device->address, 
                                          PMBUS_CMD_OPERATION, (uint8_t)mode);
    if (result == PMBUS_SUCCESS) {
        device->operation_mode = mode;
    }
    
    return result;
}

/* Data conversion functions */
pmbus_error_t pmbus_convert_linear11_to_int32(uint16_t raw, int32_t *value) {
    if (value == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    int8_t exponent = extract_linear11_exponent(raw);
    int16_t mantissa = extract_linear11_mantissa(raw);
    
    /* Calculate value = mantissa * 2^exponent */
    if (exponent >= 0) {
        *value = (int32_t)mantissa << exponent;
    } else {
        *value = (int32_t)mantissa >> (-exponent);
    }
    
    return PMBUS_SUCCESS;
}

pmbus_error_t pmbus_convert_int32_to_linear11(int32_t value, uint16_t *raw) {
    if (raw == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    /* Find optimal exponent */
    int8_t exponent = 0;
    int32_t temp_value = value;
    
    /* Scale down to fit in 11-bit mantissa */
    while ((temp_value > 1023 || temp_value < -1024) && exponent < 15) {
        temp_value >>= 1;
        exponent++;
    }
    
    /* Scale up for better precision if possible */
    while ((temp_value <= 511 && temp_value >= -512) && exponent > -16) {
        temp_value <<= 1;
        exponent--;
    }
    
    int16_t mantissa = (int16_t)temp_value;
    *raw = pack_linear11(mantissa, exponent);
    
    return PMBUS_SUCCESS;
}

pmbus_error_t pmbus_convert_linear16_to_int32(uint16_t raw, int8_t exponent, int32_t *value) {
    if (value == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    int16_t mantissa = (int16_t)raw;
    
    /* Calculate value = mantissa * 2^exponent */
    if (exponent >= 0) {
        *value = (int32_t)mantissa << exponent;
    } else {
        *value = (int32_t)mantissa >> (-exponent);
    }
    
    return PMBUS_SUCCESS;
}

pmbus_error_t pmbus_convert_direct_to_int32(uint16_t raw,
                                           const pmbus_direct_coefficients_t *coeff,
                                           int32_t *value) {
    if (coeff == NULL || value == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    /* Calculate: value = (raw * m - b) * 10^(-R) */
    int64_t temp = (int64_t)raw * coeff->m - coeff->b;
    
    if (coeff->R >= 0) {
        /* Divide by 10^R */
        for (int i = 0; i < coeff->R; i++) {
            temp /= 10;
        }
    } else {
        /* Multiply by 10^(-R) */
        for (int i = 0; i < (-coeff->R); i++) {
            temp *= 10;
        }
    }
    
    *value = (int32_t)temp;
    return PMBUS_SUCCESS;
}

/* Command descriptor functions */
const pmbus_command_descriptor_t* pmbus_get_command_descriptor(uint8_t command) {
    for (size_t i = 0; i < global_command_table_size; i++) {
        if (global_command_table[i].command == command) {
            return &global_command_table[i];
        }
    }
    return NULL;
}

bool pmbus_is_command_supported(pmbus_device_t *device, uint8_t command) {
    if (device == NULL) {
        return false;
    }
    
    if (device->ops && device->ops->cmd_supported) {
        return device->ops->cmd_supported(device, command);
    }
    
    /* Check if command is in supported commands bitmask */
    uint32_t word_index = command / 32;
    uint32_t bit_index = command % 32;
    
    if (word_index < 8) {
        return (device->capabilities.supported_commands[word_index] & (1U << bit_index)) != 0;
    }
    
    return false;
}

pmbus_data_format_t pmbus_get_command_format(pmbus_device_t *device, uint8_t command) {
    if (device == NULL) {
        return PMBUS_DATA_FORMAT_LITERAL;
    }
    
    if (device->ops && device->ops->get_cmd_format) {
        return device->ops->get_cmd_format(device, command);
    }
    
    const pmbus_command_descriptor_t *cmd_desc = pmbus_get_command_descriptor(command);
    if (cmd_desc != NULL) {
        return cmd_desc->format;
    }
    
    return PMBUS_DATA_FORMAT_LITERAL;
}

/* Utility functions */
const char* pmbus_data_format_to_string(pmbus_data_format_t format) {
    switch (format) {
        case PMBUS_DATA_FORMAT_DIRECT: return "DIRECT";
        case PMBUS_DATA_FORMAT_LINEAR11: return "LINEAR11";
        case PMBUS_DATA_FORMAT_LINEAR16: return "LINEAR16";
        case PMBUS_DATA_FORMAT_VID: return "VID";
        case PMBUS_DATA_FORMAT_IEEE754: return "IEEE754";
        case PMBUS_DATA_FORMAT_LITERAL: return "LITERAL";
        case PMBUS_DATA_FORMAT_UINT8: return "UINT8";
        case PMBUS_DATA_FORMAT_UINT16: return "UINT16";
        case PMBUS_DATA_FORMAT_MANUFACTURER_SPECIFIC: return "MFR_SPECIFIC";
        default: return "UNKNOWN";
    }
}

const char* pmbus_operation_mode_to_string(pmbus_operation_mode_t mode) {
    switch (mode) {
        case PMBUS_OPERATION_OFF: return "OFF";
        case PMBUS_OPERATION_ON: return "ON";
        case PMBUS_OPERATION_MARGIN_LOW: return "MARGIN_LOW";
        case PMBUS_OPERATION_MARGIN_HIGH: return "MARGIN_HIGH";
        default: return "UNKNOWN";
    }
}

const char* pmbus_power_state_to_string(pmbus_power_state_t state) {
    switch (state) {
        case PMBUS_POWER_STATE_OFF: return "OFF";
        case PMBUS_POWER_STATE_ON: return "ON";
        case PMBUS_POWER_STATE_STANDBY: return "STANDBY";
        case PMBUS_POWER_STATE_FAULT: return "FAULT";
        case PMBUS_POWER_STATE_UNKNOWN: return "UNKNOWN";
        default: return "INVALID";
    }
}

/* Internal helper functions */
static pmbus_error_t discover_device_capabilities(pmbus_device_t *device) {
    if (device == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    /* Initialize default capabilities */
    memset(&device->capabilities, 0, sizeof(pmbus_device_capabilities_t));
    device->capabilities.max_pages = 1;
    device->capabilities.max_block_size = PMBUS_MAX_BLOCK_SIZE;
    
    /* Try to read CAPABILITY command if supported */
    uint8_t capability_data;
    if (pmbus_read_byte(device->transport, device->address, &capability_data) == PMBUS_SUCCESS) {
        device->capabilities.has_pec_support = (capability_data & 0x80) != 0;
    }
    
    /* Probe for common commands to build supported commands mask */
    uint8_t test_commands[] = {
        PMBUS_CMD_OPERATION, PMBUS_CMD_STATUS_BYTE, PMBUS_CMD_STATUS_WORD,
        PMBUS_CMD_READ_VIN, PMBUS_CMD_READ_VOUT, PMBUS_CMD_READ_IOUT,
        PMBUS_CMD_READ_TEMPERATURE_1, PMBUS_CMD_READ_POUT, PMBUS_CMD_READ_PIN
    };
    
    for (size_t i = 0; i < sizeof(test_commands); i++) {
        uint8_t cmd = test_commands[i];
        uint8_t dummy_data[4];
        size_t dummy_length = sizeof(dummy_data);
        
        if (pmbus_read_command(device, cmd, dummy_data, &dummy_length) == PMBUS_SUCCESS) {
            uint32_t word_index = cmd / 32;
            uint32_t bit_index = cmd % 32;
            if (word_index < 8) {
                device->capabilities.supported_commands[word_index] |= (1U << bit_index);
            }
        }
    }
    
    return PMBUS_SUCCESS;
}

static pmbus_error_t read_device_identification(pmbus_device_t *device) {
    if (device == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    /* Initialize device info */
    memset(&device->info, 0, sizeof(pmbus_device_info_t));
    
    /* Try to read manufacturer ID */
    uint8_t mfr_data[16];
    size_t mfr_length = sizeof(mfr_data);
    if (pmbus_read_command(device, PMBUS_CMD_MFR_ID, mfr_data, &mfr_length) == PMBUS_SUCCESS) {
        if (mfr_length >= 2) {
            device->info.manufacturer_id = mfr_data[0] | (mfr_data[1] << 8);
        }
        if (mfr_length > 2) {
            size_t copy_len = (mfr_length - 2 < sizeof(device->info.manufacturer_name) - 1) ? 
                             (mfr_length - 2) : (sizeof(device->info.manufacturer_name) - 1);
            memcpy(device->info.manufacturer_name, &mfr_data[2], copy_len);
            device->info.manufacturer_name[copy_len] = '\0';
        }
    }
    
    /* Try to read model */
    uint8_t model_data[16];
    size_t model_length = sizeof(model_data);
    if (pmbus_read_command(device, PMBUS_CMD_MFR_MODEL, model_data, &model_length) == PMBUS_SUCCESS) {
        size_t copy_len = (model_length < sizeof(device->info.device_model) - 1) ? 
                         model_length : (sizeof(device->info.device_model) - 1);
        memcpy(device->info.device_model, model_data, copy_len);
        device->info.device_model[copy_len] = '\0';
    }
    
    return PMBUS_SUCCESS;
}

static int8_t extract_linear11_exponent(uint16_t raw) {
    /* Exponent is in bits 15:11 */
    int8_t exponent = (int8_t)((raw >> 11) & 0x1F);
    /* Sign extend from 5 bits to 8 bits */
    if (exponent & 0x10) {
        exponent |= 0xE0;
    }
    return exponent;
}

static int16_t extract_linear11_mantissa(uint16_t raw) {
    /* Mantissa is in bits 10:0 */
    int16_t mantissa = (int16_t)(raw & 0x7FF);
    /* Sign extend from 11 bits to 16 bits */
    if (mantissa & 0x400) {
        mantissa |= 0xF800;
    }
    return mantissa;
}

static uint16_t pack_linear11(int16_t mantissa, int8_t exponent) {
    uint16_t raw = 0;
    /* Pack exponent into bits 15:11 */
    raw |= ((uint16_t)(exponent & 0x1F)) << 11;
    /* Pack mantissa into bits 10:0 */
    raw |= (uint16_t)(mantissa & 0x7FF);
    return raw;
}
