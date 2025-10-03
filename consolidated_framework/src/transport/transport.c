#include "pmbus/transport.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/**
 * @file transport.c
 * @brief PMBus Transport Layer Implementation
 */

/* PEC calculation table (CRC-8 with polynomial 0x07) */
static const uint8_t pec_table[256] = {
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
    0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
    0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
    0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
    0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
    0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
    0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
    0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
    0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
    0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
    0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
    0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
    0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
    0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
    0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
    0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
    0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
    0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
    0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
    0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
    0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
    0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
    0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
    0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
    0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
    0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
    0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

/* Internal helper functions */
static uint32_t get_timestamp_ms(void);
static pmbus_error_t validate_transaction(const pmbus_transaction_t *transaction);
static pmbus_error_t queue_transaction(pmbus_transport_t *transport, pmbus_transaction_t *transaction);
static pmbus_transaction_t* dequeue_transaction(pmbus_transport_t *transport);
static void update_statistics(pmbus_transport_t *transport, const pmbus_transaction_t *transaction);

/* PEC calculation functions */
uint8_t pmbus_calculate_pec(const uint8_t *data, size_t length) {
    uint8_t pec = 0;
    
    if (data == NULL || length == 0) {
        return 0;
    }
    
    for (size_t i = 0; i < length; i++) {
        pec = pec_table[pec ^ data[i]];
    }
    
    return pec;
}

bool pmbus_verify_pec(const uint8_t *data, size_t length, uint8_t expected_pec) {
    if (data == NULL || length == 0) {
        return false;
    }
    
    uint8_t calculated_pec = pmbus_calculate_pec(data, length);
    return (calculated_pec == expected_pec);
}

/* Transport factory functions */
pmbus_transport_t* pmbus_transport_create(pmbus_transport_type_t type) {
    pmbus_transport_t *transport = calloc(1, sizeof(pmbus_transport_t));
    if (transport == NULL) {
        return NULL;
    }
    
    transport->type = type;
    transport->next_transaction_id = 1;
    
    /* Initialize default configuration */
    transport->config.type = type;
    transport->config.clock_speed_hz = 100000; /* 100 kHz default */
    transport->config.timeout_ms = 1000;
    transport->config.retry_count = 3;
    transport->config.enable_pec = true;
    transport->config.enable_smbus_pec = true;
    transport->config.enable_auto_retry = true;
    
    /* Initialize default capabilities */
    transport->capabilities.supports_pec = true;
    transport->capabilities.supports_group_commands = true;
    transport->capabilities.supports_host_notify = false;
    transport->capabilities.supports_smbus_arp = false;
    transport->capabilities.supports_process_call = true;
    transport->capabilities.supports_block_process_call = true;
    transport->capabilities.supports_quick_command = true;
    transport->capabilities.max_clock_speed_hz = 400000; /* 400 kHz */
    transport->capabilities.max_block_size = PMBUS_MAX_BLOCK_SIZE;
    transport->capabilities.address_resolution_bits = 7;
    transport->capabilities.timeout_ms = transport->config.timeout_ms;
    
    return transport;
}

pmbus_error_t pmbus_transport_destroy(pmbus_transport_t *transport) {
    if (transport == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    if (transport->initialized) {
        pmbus_transport_finalize(transport);
    }
    
    /* Clean up any remaining transactions in queue */
    for (size_t i = 0; i < PMBUS_MAX_TRANSACTION_QUEUE; i++) {
        if (transport->transaction_queue[i] != NULL) {
            free(transport->transaction_queue[i]);
            transport->transaction_queue[i] = NULL;
        }
    }
    
    free(transport);
    return PMBUS_SUCCESS;
}

/* Transport management functions */
pmbus_error_t pmbus_transport_init(pmbus_transport_t *transport,
                                  const pmbus_transport_config_t *config) {
    if (transport == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    if (transport->initialized) {
        return PMBUS_ERROR_INVALID_STATE;
    }
    
    if (config != NULL) {
        transport->config = *config;
    }
    
    /* Initialize transport-specific implementation */
    if (transport->ops && transport->ops->init) {
        pmbus_error_t result = transport->ops->init(transport, &transport->config);
        if (result != PMBUS_SUCCESS) {
            return result;
        }
    }
    
    transport->initialized = true;
    return PMBUS_SUCCESS;
}

pmbus_error_t pmbus_transport_finalize(pmbus_transport_t *transport) {
    if (transport == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    if (!transport->initialized) {
        return PMBUS_SUCCESS;
    }
    
    /* Finalize transport-specific implementation */
    if (transport->ops && transport->ops->finalize) {
        transport->ops->finalize(transport);
    }
    
    transport->initialized = false;
    return PMBUS_SUCCESS;
}

/* Synchronous transaction functions */
pmbus_error_t pmbus_send_byte(pmbus_transport_t *transport,
                             uint8_t address,
                             uint8_t data) {
    if (transport == NULL || !transport->initialized) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    pmbus_transaction_t transaction = {0};
    transaction.transaction_id = transport->next_transaction_id++;
    transaction.type = PMBUS_TRANSACTION_SEND_BYTE;
    transaction.address = address;
    transaction.data[0] = data;
    transaction.data_length = 1;
    transaction.flags = PMBUS_FLAG_NONE;
    transaction.state = PMBUS_TRANSACTION_STATE_PENDING;
    
    if (transport->ops && transport->ops->execute_sync) {
        return transport->ops->execute_sync(transport, &transaction);
    }
    
    return PMBUS_ERROR_UNSUPPORTED;
}

pmbus_error_t pmbus_read_byte(pmbus_transport_t *transport,
                             uint8_t address,
                             uint8_t *data) {
    if (transport == NULL || data == NULL || !transport->initialized) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    pmbus_transaction_t transaction = {0};
    transaction.transaction_id = transport->next_transaction_id++;
    transaction.type = PMBUS_TRANSACTION_READ_BYTE;
    transaction.address = address;
    transaction.data_length = 1;
    transaction.flags = PMBUS_FLAG_NONE;
    transaction.state = PMBUS_TRANSACTION_STATE_PENDING;
    
    if (transport->ops && transport->ops->execute_sync) {
        pmbus_error_t result = transport->ops->execute_sync(transport, &transaction);
        if (result == PMBUS_SUCCESS) {
            *data = transaction.data[0];
        }
        return result;
    }
    
    return PMBUS_ERROR_UNSUPPORTED;
}

pmbus_error_t pmbus_write_byte(pmbus_transport_t *transport,
                              uint8_t address,
                              uint8_t command,
                              uint8_t data) {
    if (transport == NULL || !transport->initialized) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    pmbus_transaction_t transaction = {0};
    transaction.transaction_id = transport->next_transaction_id++;
    transaction.type = PMBUS_TRANSACTION_WRITE_BYTE;
    transaction.address = address;
    transaction.command = command;
    transaction.data[0] = data;
    transaction.data_length = 1;
    transaction.flags = transport->config.enable_pec ? PMBUS_FLAG_PEC : PMBUS_FLAG_NONE;
    transaction.state = PMBUS_TRANSACTION_STATE_PENDING;
    
    if (transport->ops && transport->ops->execute_sync) {
        return transport->ops->execute_sync(transport, &transaction);
    }
    
    return PMBUS_ERROR_UNSUPPORTED;
}

pmbus_error_t pmbus_read_word(pmbus_transport_t *transport,
                             uint8_t address,
                             uint8_t command,
                             uint16_t *data) {
    if (transport == NULL || data == NULL || !transport->initialized) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    pmbus_transaction_t transaction = {0};
    transaction.transaction_id = transport->next_transaction_id++;
    transaction.type = PMBUS_TRANSACTION_READ_WORD;
    transaction.address = address;
    transaction.command = command;
    transaction.data_length = 2;
    transaction.flags = transport->config.enable_pec ? PMBUS_FLAG_PEC : PMBUS_FLAG_NONE;
    transaction.state = PMBUS_TRANSACTION_STATE_PENDING;
    
    if (transport->ops && transport->ops->execute_sync) {
        pmbus_error_t result = transport->ops->execute_sync(transport, &transaction);
        if (result == PMBUS_SUCCESS) {
            *data = (uint16_t)(transaction.data[0] | (transaction.data[1] << 8));
        }
        return result;
    }
    
    return PMBUS_ERROR_UNSUPPORTED;
}

pmbus_error_t pmbus_write_word(pmbus_transport_t *transport,
                              uint8_t address,
                              uint8_t command,
                              uint16_t data) {
    if (transport == NULL || !transport->initialized) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    pmbus_transaction_t transaction = {0};
    transaction.transaction_id = transport->next_transaction_id++;
    transaction.type = PMBUS_TRANSACTION_WRITE_WORD;
    transaction.address = address;
    transaction.command = command;
    transaction.data[0] = (uint8_t)(data & 0xFF);
    transaction.data[1] = (uint8_t)((data >> 8) & 0xFF);
    transaction.data_length = 2;
    transaction.flags = transport->config.enable_pec ? PMBUS_FLAG_PEC : PMBUS_FLAG_NONE;
    transaction.state = PMBUS_TRANSACTION_STATE_PENDING;
    
    if (transport->ops && transport->ops->execute_sync) {
        return transport->ops->execute_sync(transport, &transaction);
    }
    
    return PMBUS_ERROR_UNSUPPORTED;
}

pmbus_error_t pmbus_read_block(pmbus_transport_t *transport,
                              uint8_t address,
                              uint8_t command,
                              uint8_t *data,
                              size_t *length) {
    if (transport == NULL || data == NULL || length == NULL || !transport->initialized) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    pmbus_transaction_t transaction = {0};
    transaction.transaction_id = transport->next_transaction_id++;
    transaction.type = PMBUS_TRANSACTION_READ_BLOCK;
    transaction.address = address;
    transaction.command = command;
    transaction.data_length = *length;
    transaction.flags = transport->config.enable_pec ? PMBUS_FLAG_PEC : PMBUS_FLAG_NONE;
    transaction.state = PMBUS_TRANSACTION_STATE_PENDING;
    
    if (transaction.data_length > PMBUS_MAX_BLOCK_SIZE) {
        transaction.data_length = PMBUS_MAX_BLOCK_SIZE;
    }
    
    if (transport->ops && transport->ops->execute_sync) {
        pmbus_error_t result = transport->ops->execute_sync(transport, &transaction);
        if (result == PMBUS_SUCCESS) {
            memcpy(data, transaction.data, transaction.data_length);
            *length = transaction.data_length;
        }
        return result;
    }
    
    return PMBUS_ERROR_UNSUPPORTED;
}

pmbus_error_t pmbus_write_block(pmbus_transport_t *transport,
                               uint8_t address,
                               uint8_t command,
                               const uint8_t *data,
                               size_t length) {
    if (transport == NULL || data == NULL || !transport->initialized) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    if (length > PMBUS_MAX_BLOCK_SIZE) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    pmbus_transaction_t transaction = {0};
    transaction.transaction_id = transport->next_transaction_id++;
    transaction.type = PMBUS_TRANSACTION_WRITE_BLOCK;
    transaction.address = address;
    transaction.command = command;
    memcpy(transaction.data, data, length);
    transaction.data_length = length;
    transaction.flags = transport->config.enable_pec ? PMBUS_FLAG_PEC : PMBUS_FLAG_NONE;
    transaction.state = PMBUS_TRANSACTION_STATE_PENDING;
    
    if (transport->ops && transport->ops->execute_sync) {
        return transport->ops->execute_sync(transport, &transaction);
    }
    
    return PMBUS_ERROR_UNSUPPORTED;
}

pmbus_error_t pmbus_process_call(pmbus_transport_t *transport,
                                uint8_t address,
                                uint8_t command,
                                uint16_t data_in,
                                uint16_t *data_out) {
    if (transport == NULL || data_out == NULL || !transport->initialized) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    pmbus_transaction_t transaction = {0};
    transaction.transaction_id = transport->next_transaction_id++;
    transaction.type = PMBUS_TRANSACTION_PROCESS_CALL;
    transaction.address = address;
    transaction.command = command;
    transaction.data[0] = (uint8_t)(data_in & 0xFF);
    transaction.data[1] = (uint8_t)((data_in >> 8) & 0xFF);
    transaction.data_length = 2;
    transaction.flags = transport->config.enable_pec ? PMBUS_FLAG_PEC : PMBUS_FLAG_NONE;
    transaction.state = PMBUS_TRANSACTION_STATE_PENDING;
    
    if (transport->ops && transport->ops->execute_sync) {
        pmbus_error_t result = transport->ops->execute_sync(transport, &transaction);
        if (result == PMBUS_SUCCESS) {
            *data_out = (uint16_t)(transaction.data[0] | (transaction.data[1] << 8));
        }
        return result;
    }
    
    return PMBUS_ERROR_UNSUPPORTED;
}

/* Utility functions */
pmbus_error_t pmbus_transport_get_capabilities(pmbus_transport_t *transport,
                                              pmbus_transport_capabilities_t *caps) {
    if (transport == NULL || caps == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    if (transport->ops && transport->ops->get_capabilities) {
        return transport->ops->get_capabilities(transport, caps);
    }
    
    *caps = transport->capabilities;
    return PMBUS_SUCCESS;
}

pmbus_error_t pmbus_transport_get_statistics(pmbus_transport_t *transport,
                                            pmbus_transport_statistics_t *stats) {
    if (transport == NULL || stats == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    if (transport->ops && transport->ops->get_statistics) {
        return transport->ops->get_statistics(transport, stats);
    }
    
    *stats = transport->statistics;
    return PMBUS_SUCCESS;
}

pmbus_error_t pmbus_transport_reset_statistics(pmbus_transport_t *transport) {
    if (transport == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    memset(&transport->statistics, 0, sizeof(pmbus_transport_statistics_t));
    return PMBUS_SUCCESS;
}

/* Bus management functions */
pmbus_error_t pmbus_bus_scan(pmbus_transport_t *transport,
                            uint8_t *addresses,
                            size_t *count) {
    if (transport == NULL || addresses == NULL || count == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    if (transport->ops && transport->ops->scan_bus) {
        return transport->ops->scan_bus(transport, addresses, count);
    }
    
    return PMBUS_ERROR_UNSUPPORTED;
}

pmbus_error_t pmbus_bus_reset(pmbus_transport_t *transport) {
    if (transport == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    if (transport->ops && transport->ops->reset) {
        return transport->ops->reset(transport);
    }
    
    return PMBUS_ERROR_UNSUPPORTED;
}

/* Error handling */
const char* pmbus_error_to_string(pmbus_error_t error) {
    switch (error) {
        case PMBUS_SUCCESS: return "Success";
        case PMBUS_ERROR_INVALID_PARAM: return "Invalid parameter";
        case PMBUS_ERROR_NO_MEMORY: return "Out of memory";
        case PMBUS_ERROR_TIMEOUT: return "Timeout";
        case PMBUS_ERROR_NACK: return "NACK received";
        case PMBUS_ERROR_BUS_BUSY: return "Bus busy";
        case PMBUS_ERROR_ARBITRATION_LOST: return "Arbitration lost";
        case PMBUS_ERROR_PEC_MISMATCH: return "PEC mismatch";
        case PMBUS_ERROR_UNSUPPORTED: return "Unsupported operation";
        case PMBUS_ERROR_DEVICE_NOT_FOUND: return "Device not found";
        case PMBUS_ERROR_INVALID_STATE: return "Invalid state";
        case PMBUS_ERROR_QUEUE_FULL: return "Queue full";
        case PMBUS_ERROR_TRANSACTION_ABORTED: return "Transaction aborted";
        case PMBUS_ERROR_BUS_ERROR: return "Bus error";
        default: return "Unknown error";
    }
}

bool pmbus_is_retriable_error(pmbus_error_t error) {
    switch (error) {
        case PMBUS_ERROR_TIMEOUT:
        case PMBUS_ERROR_NACK:
        case PMBUS_ERROR_BUS_BUSY:
        case PMBUS_ERROR_ARBITRATION_LOST:
        case PMBUS_ERROR_BUS_ERROR:
            return true;
        default:
            return false;
    }
}

/* Internal helper functions */
static uint32_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static pmbus_error_t validate_transaction(const pmbus_transaction_t *transaction) {
    if (transaction == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    if (transaction->address > 0x7F) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    if (transaction->data_length > PMBUS_MAX_BLOCK_SIZE) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    return PMBUS_SUCCESS;
}

static void update_statistics(pmbus_transport_t *transport, const pmbus_transaction_t *transaction) {
    if (transport == NULL || transaction == NULL) {
        return;
    }
    
    pmbus_transport_statistics_t *stats = &transport->statistics;
    
    if (transaction->state == PMBUS_TRANSACTION_STATE_COMPLETED) {
        stats->transactions_completed++;
        
        uint32_t duration = transaction->timestamp_end - transaction->timestamp_start;
        if (stats->transactions_completed == 1) {
            stats->min_transaction_time_us = duration;
            stats->max_transaction_time_us = duration;
            stats->average_transaction_time_us = duration;
        } else {
            if (duration < stats->min_transaction_time_us) {
                stats->min_transaction_time_us = duration;
            }
            if (duration > stats->max_transaction_time_us) {
                stats->max_transaction_time_us = duration;
            }
            stats->average_transaction_time_us = 
                (stats->average_transaction_time_us + duration) / 2;
        }
        
        if (transaction->type == PMBUS_TRANSACTION_READ_BYTE ||
            transaction->type == PMBUS_TRANSACTION_READ_WORD ||
            transaction->type == PMBUS_TRANSACTION_READ_BLOCK) {
            stats->total_bytes_received += transaction->data_length;
        } else {
            stats->total_bytes_sent += transaction->data_length;
        }
    } else if (transaction->state == PMBUS_TRANSACTION_STATE_FAILED) {
        stats->transactions_failed++;
        
        switch (transaction->result) {
            case PMBUS_ERROR_TIMEOUT:
                stats->transactions_timeout++;
                break;
            case PMBUS_ERROR_NACK:
                stats->nack_count++;
                break;
            case PMBUS_ERROR_ARBITRATION_LOST:
                stats->arbitration_lost_count++;
                break;
            case PMBUS_ERROR_PEC_MISMATCH:
                stats->pec_errors++;
                break;
            case PMBUS_ERROR_BUS_ERROR:
                stats->bus_error_count++;
                break;
            default:
                break;
        }
    }
    
    if (transaction->retry_count > 0) {
        stats->transactions_retried++;
    }
}
