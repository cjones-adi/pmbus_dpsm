#ifndef PMBUS_TRANSPORT_H
#define PMBUS_TRANSPORT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file pmbus_transport.h
 * @brief PMBus Transport Abstraction Layer
 * 
 * This module provides a unified interface for PMBus communication across
 * different hardware platforms and transport mechanisms.
 */

/* Forward declarations */
typedef struct pmbus_transport pmbus_transport_t;
typedef struct pmbus_transaction pmbus_transaction_t;

/* Constants */
#define PMBUS_MAX_BLOCK_SIZE        32
#define PMBUS_MAX_TRANSACTION_QUEUE 64
#define PMBUS_INVALID_TRANSACTION_ID 0
#define PMBUS_BROADCAST_ADDRESS     0x00

/* Error codes */
typedef enum {
    PMBUS_SUCCESS = 0,
    PMBUS_ERROR_INVALID_PARAM = -1,
    PMBUS_ERROR_NO_MEMORY = -2,
    PMBUS_ERROR_TIMEOUT = -3,
    PMBUS_ERROR_NACK = -4,
    PMBUS_ERROR_BUS_BUSY = -5,
    PMBUS_ERROR_ARBITRATION_LOST = -6,
    PMBUS_ERROR_PEC_MISMATCH = -7,
    PMBUS_ERROR_UNSUPPORTED = -8,
    PMBUS_ERROR_DEVICE_NOT_FOUND = -9,
    PMBUS_ERROR_INVALID_STATE = -10,
    PMBUS_ERROR_QUEUE_FULL = -11,
    PMBUS_ERROR_TRANSACTION_ABORTED = -12,
    PMBUS_ERROR_BUS_ERROR = -13
} pmbus_error_t;

/* Transport types */
typedef enum {
    PMBUS_TRANSPORT_I2C,
    PMBUS_TRANSPORT_SMBUS,
    PMBUS_TRANSPORT_SIMULATION,
    PMBUS_TRANSPORT_CUSTOM
} pmbus_transport_type_t;

/* Transaction types */
typedef enum {
    PMBUS_TRANSACTION_SEND_BYTE,
    PMBUS_TRANSACTION_READ_BYTE,
    PMBUS_TRANSACTION_WRITE_BYTE,
    PMBUS_TRANSACTION_READ_WORD,
    PMBUS_TRANSACTION_WRITE_WORD,
    PMBUS_TRANSACTION_READ_BLOCK,
    PMBUS_TRANSACTION_WRITE_BLOCK,
    PMBUS_TRANSACTION_PROCESS_CALL,
    PMBUS_TRANSACTION_BLOCK_PROCESS_CALL
} pmbus_transaction_type_t;

/* Transaction flags */
typedef enum {
    PMBUS_FLAG_NONE = 0x00,
    PMBUS_FLAG_PEC = 0x01,
    PMBUS_FLAG_RETRY = 0x02,
    PMBUS_FLAG_NO_STOP = 0x04,
    PMBUS_FLAG_QUICK_COMMAND = 0x08,
    PMBUS_FLAG_GROUP_COMMAND = 0x10,
    PMBUS_FLAG_ALERT_RESPONSE = 0x20
} pmbus_transaction_flags_t;

/* Transaction state */
typedef enum {
    PMBUS_TRANSACTION_STATE_PENDING,
    PMBUS_TRANSACTION_STATE_IN_PROGRESS,
    PMBUS_TRANSACTION_STATE_COMPLETED,
    PMBUS_TRANSACTION_STATE_FAILED,
    PMBUS_TRANSACTION_STATE_TIMEOUT,
    PMBUS_TRANSACTION_STATE_ABORTED
} pmbus_transaction_state_t;

/* Transaction structure */
struct pmbus_transaction {
    uint32_t transaction_id;
    pmbus_transaction_type_t type;
    uint8_t address;
    uint8_t command;
    uint8_t data[PMBUS_MAX_BLOCK_SIZE];
    size_t data_length;
    pmbus_transaction_flags_t flags;
    pmbus_transaction_state_t state;
    pmbus_error_t result;
    uint32_t timestamp_start;
    uint32_t timestamp_end;
    uint8_t retry_count;
    void *user_data;
    
    /* Callback for transaction completion */
    void (*completion_callback)(pmbus_transaction_t *transaction);
};

/* Transport capabilities */
typedef struct {
    bool supports_pec;
    bool supports_group_commands;
    bool supports_host_notify;
    bool supports_smbus_arp;
    bool supports_process_call;
    bool supports_block_process_call;
    bool supports_quick_command;
    uint32_t max_clock_speed_hz;
    uint16_t max_block_size;
    uint8_t address_resolution_bits;
    uint32_t timeout_ms;
} pmbus_transport_capabilities_t;

/* Transport configuration */
typedef struct {
    pmbus_transport_type_t type;
    uint32_t clock_speed_hz;
    uint16_t timeout_ms;
    uint8_t retry_count;
    bool enable_pec;
    bool enable_smbus_pec;
    bool enable_auto_retry;
    void *platform_data;
} pmbus_transport_config_t;

/* Transport statistics */
typedef struct {
    uint32_t transactions_completed;
    uint32_t transactions_failed;
    uint32_t transactions_retried;
    uint32_t transactions_timeout;
    uint32_t pec_errors;
    uint32_t nack_count;
    uint32_t arbitration_lost_count;
    uint32_t bus_error_count;
    uint64_t total_bytes_sent;
    uint64_t total_bytes_received;
    uint32_t average_transaction_time_us;
    uint32_t max_transaction_time_us;
    uint32_t min_transaction_time_us;
} pmbus_transport_statistics_t;

/* Transport operations interface */
typedef struct pmbus_transport_ops {
    /* Initialize transport */
    pmbus_error_t (*init)(pmbus_transport_t *transport, 
                         const pmbus_transport_config_t *config);
    
    /* Finalize transport */
    pmbus_error_t (*finalize)(pmbus_transport_t *transport);
    
    /* Execute transaction synchronously */
    pmbus_error_t (*execute_sync)(pmbus_transport_t *transport,
                                 pmbus_transaction_t *transaction);
    
    /* Queue transaction for asynchronous execution */
    pmbus_error_t (*execute_async)(pmbus_transport_t *transport,
                                  pmbus_transaction_t *transaction);
    
    /* Abort pending/in-progress transaction */
    pmbus_error_t (*abort_transaction)(pmbus_transport_t *transport,
                                      uint32_t transaction_id);
    
    /* Get transport capabilities */
    pmbus_error_t (*get_capabilities)(pmbus_transport_t *transport,
                                     pmbus_transport_capabilities_t *caps);
    
    /* Get transport statistics */
    pmbus_error_t (*get_statistics)(pmbus_transport_t *transport,
                                   pmbus_transport_statistics_t *stats);
    
    /* Reset transport */
    pmbus_error_t (*reset)(pmbus_transport_t *transport);
    
    /* Scan for devices on bus */
    pmbus_error_t (*scan_bus)(pmbus_transport_t *transport,
                             uint8_t *addresses, 
                             size_t *count);
} pmbus_transport_ops_t;

/* Main transport structure */
struct pmbus_transport {
    pmbus_transport_type_t type;
    pmbus_transport_config_t config;
    pmbus_transport_capabilities_t capabilities;
    pmbus_transport_statistics_t statistics;
    const pmbus_transport_ops_t *ops;
    
    /* Transaction management */
    pmbus_transaction_t *transaction_queue[PMBUS_MAX_TRANSACTION_QUEUE];
    size_t queue_head;
    size_t queue_tail;
    size_t queue_count;
    uint32_t next_transaction_id;
    
    /* State and synchronization */
    bool initialized;
    bool bus_locked;
    void *lock; /* Platform-specific mutex */
    
    /* Platform-specific data */
    void *platform_data;
    
    /* Alert handling */
    void (*alert_callback)(pmbus_transport_t *transport, uint8_t address);
};

/* Transport factory functions */
pmbus_transport_t* pmbus_transport_create(pmbus_transport_type_t type);
pmbus_error_t pmbus_transport_destroy(pmbus_transport_t *transport);

/* Transport management functions */
pmbus_error_t pmbus_transport_init(pmbus_transport_t *transport,
                                  const pmbus_transport_config_t *config);
pmbus_error_t pmbus_transport_finalize(pmbus_transport_t *transport);

/* Synchronous transaction functions */
pmbus_error_t pmbus_send_byte(pmbus_transport_t *transport,
                             uint8_t address,
                             uint8_t data);

pmbus_error_t pmbus_read_byte(pmbus_transport_t *transport,
                             uint8_t address,
                             uint8_t *data);

pmbus_error_t pmbus_write_byte(pmbus_transport_t *transport,
                              uint8_t address,
                              uint8_t command,
                              uint8_t data);

pmbus_error_t pmbus_read_word(pmbus_transport_t *transport,
                             uint8_t address,
                             uint8_t command,
                             uint16_t *data);

pmbus_error_t pmbus_write_word(pmbus_transport_t *transport,
                              uint8_t address,
                              uint8_t command,
                              uint16_t data);

pmbus_error_t pmbus_read_block(pmbus_transport_t *transport,
                              uint8_t address,
                              uint8_t command,
                              uint8_t *data,
                              size_t *length);

pmbus_error_t pmbus_write_block(pmbus_transport_t *transport,
                               uint8_t address,
                               uint8_t command,
                               const uint8_t *data,
                               size_t length);

pmbus_error_t pmbus_process_call(pmbus_transport_t *transport,
                                uint8_t address,
                                uint8_t command,
                                uint16_t data_in,
                                uint16_t *data_out);

/* Asynchronous transaction functions */
pmbus_error_t pmbus_transaction_create(pmbus_transaction_t **transaction);
pmbus_error_t pmbus_transaction_destroy(pmbus_transaction_t *transaction);
pmbus_error_t pmbus_transaction_execute_async(pmbus_transport_t *transport,
                                             pmbus_transaction_t *transaction);
pmbus_error_t pmbus_transaction_wait(pmbus_transaction_t *transaction,
                                    uint32_t timeout_ms);
pmbus_error_t pmbus_transaction_abort(pmbus_transport_t *transport,
                                     uint32_t transaction_id);

/* Bus management functions */
pmbus_error_t pmbus_bus_scan(pmbus_transport_t *transport,
                            uint8_t *addresses,
                            size_t *count);
pmbus_error_t pmbus_bus_reset(pmbus_transport_t *transport);
pmbus_error_t pmbus_bus_lock(pmbus_transport_t *transport);
pmbus_error_t pmbus_bus_unlock(pmbus_transport_t *transport);

/* Utility functions */
pmbus_error_t pmbus_transport_get_capabilities(pmbus_transport_t *transport,
                                              pmbus_transport_capabilities_t *caps);
pmbus_error_t pmbus_transport_get_statistics(pmbus_transport_t *transport,
                                            pmbus_transport_statistics_t *stats);
pmbus_error_t pmbus_transport_reset_statistics(pmbus_transport_t *transport);

/* PEC (Packet Error Code) functions */
uint8_t pmbus_calculate_pec(const uint8_t *data, size_t length);
bool pmbus_verify_pec(const uint8_t *data, size_t length, uint8_t expected_pec);

/* Error handling */
const char* pmbus_error_to_string(pmbus_error_t error);
bool pmbus_is_retriable_error(pmbus_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* PMBUS_TRANSPORT_H */
