#ifndef PMBUS_DISCOVERY_H
#define PMBUS_DISCOVERY_H

#include "pmbus/protocol.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file pmbus_discovery.h
 * @brief PMBus Device Discovery and Factory System
 * 
 * This module provides automatic device discovery, identification,
 * and factory-based device instantiation.
 */

/* Forward declarations */
typedef struct pmbus_device_registry pmbus_device_registry_t;
typedef struct pmbus_device_factory pmbus_device_factory_t;
typedef struct pmbus_discovery_engine pmbus_discovery_engine_t;

/* Discovery configuration */
typedef struct {
    uint8_t start_address;
    uint8_t end_address;
    uint16_t scan_timeout_ms;
    bool deep_scan;
    bool validate_devices;
    bool auto_configure;
    bool parallel_scan;
    uint8_t scan_retries;
    uint8_t ignore_addresses[16];
    uint8_t ignore_count;
    void (*progress_callback)(uint8_t address, bool found, void *user_data);
    void (*discovery_callback)(pmbus_device_t *device, void *user_data);
    void *user_data;
} pmbus_discovery_config_t;

/* Discovery state */
typedef enum {
    PMBUS_DISCOVERY_STATE_IDLE,
    PMBUS_DISCOVERY_STATE_SCANNING,
    PMBUS_DISCOVERY_STATE_IDENTIFYING,
    PMBUS_DISCOVERY_STATE_CONFIGURING,
    PMBUS_DISCOVERY_STATE_COMPLETED,
    PMBUS_DISCOVERY_STATE_ERROR,
    PMBUS_DISCOVERY_STATE_ABORTED
} pmbus_discovery_state_t;

/* Device categories */
typedef enum {
    PMBUS_DEVICE_CATEGORY_CONTROLLER,
    PMBUS_DEVICE_CATEGORY_MANAGER,
    PMBUS_DEVICE_CATEGORY_SEQUENCER,
    PMBUS_DEVICE_CATEGORY_MONITOR,
    PMBUS_DEVICE_CATEGORY_REGULATOR,
    PMBUS_DEVICE_CATEGORY_CONVERTER,
    PMBUS_DEVICE_CATEGORY_PROTECTION,
    PMBUS_DEVICE_CATEGORY_HYBRID,
    PMBUS_DEVICE_CATEGORY_GENERIC,
    PMBUS_DEVICE_CATEGORY_UNKNOWN
} pmbus_device_category_t;

/* Device filter */
typedef struct {
    pmbus_device_category_t category_mask;
    uint16_t manufacturer_id_mask;
    uint16_t device_id_mask;
    uint32_t capability_mask;
    bool require_all_capabilities;
    uint8_t min_pages;
    uint8_t max_pages;
    char manufacturer_name_filter[16];
    char device_model_filter[16];
} pmbus_device_filter_t;

/* Device registry entry */
typedef struct {
    uint16_t manufacturer_id;
    uint16_t device_id;
    uint16_t revision_mask;
    uint16_t revision_value;
    char device_name[32];
    char manufacturer_name[16];
    pmbus_device_category_t device_category;
    uint32_t capabilities;
    const pmbus_device_ops_t *ops;
    const pmbus_command_descriptor_t *cmd_table;
    size_t cmd_table_size;
    pmbus_device_config_t default_config;
    uint8_t detection_priority;
    pmbus_device_t* (*factory_func)(pmbus_transport_t *transport, uint8_t address,
                                   const pmbus_device_info_t *info);
} pmbus_device_registry_entry_t;

/* Device registry */
struct pmbus_device_registry {
    pmbus_device_registry_entry_t entries[256]; /* Max 256 device types */
    size_t entry_count;
    void *registry_lock; /* Platform-specific mutex */
};

/* Device factory */
struct pmbus_device_factory {
    pmbus_device_registry_t *registry;
    pmbus_device_t *created_devices[128]; /* Max 128 created devices */
    size_t device_count;
    pmbus_protocol_engine_t *protocol_engine;
    void *factory_lock; /* Platform-specific mutex */
};

/* Discovery engine */
struct pmbus_discovery_engine {
    pmbus_transport_t *transport;
    pmbus_device_factory_t *factory;
    pmbus_discovery_config_t config;
    pmbus_device_t *discovered_devices[128]; /* Max 128 discovered devices */
    size_t device_count;
    pmbus_discovery_state_t state;
    uint8_t current_scan_address;
    uint32_t scan_start_time;
    uint32_t scan_progress_percent;
    
    /* Statistics */
    uint32_t addresses_scanned;
    uint32_t devices_found;
    uint32_t devices_identified;
    uint32_t devices_configured;
    uint32_t scan_errors;
    uint32_t total_scan_time_ms;
};

/* Discovery engine functions */
pmbus_discovery_engine_t* pmbus_discovery_engine_create(pmbus_transport_t *transport,
                                                       pmbus_device_factory_t *factory);
pmbus_error_t pmbus_discovery_engine_destroy(pmbus_discovery_engine_t *engine);
pmbus_error_t pmbus_discovery_engine_configure(pmbus_discovery_engine_t *engine,
                                              const pmbus_discovery_config_t *config);

/* Discovery operations */
pmbus_error_t pmbus_start_discovery(pmbus_discovery_engine_t *engine);
pmbus_error_t pmbus_stop_discovery(pmbus_discovery_engine_t *engine);
pmbus_error_t pmbus_discover_at_address(pmbus_discovery_engine_t *engine,
                                       uint8_t address,
                                       pmbus_device_t **device);
pmbus_error_t pmbus_scan_address_range(pmbus_discovery_engine_t *engine,
                                      uint8_t start_address,
                                      uint8_t end_address);

/* Discovery results */
pmbus_device_t** pmbus_get_discovered_devices(pmbus_discovery_engine_t *engine,
                                             size_t *count);
pmbus_device_t** pmbus_filter_discovered_devices(pmbus_discovery_engine_t *engine,
                                                const pmbus_device_filter_t *filter,
                                                size_t *count);
pmbus_error_t pmbus_get_discovery_statistics(pmbus_discovery_engine_t *engine,
                                            uint32_t *addresses_scanned,
                                            uint32_t *devices_found,
                                            uint32_t *total_time_ms);

/* Device registry functions */
pmbus_device_registry_t* pmbus_device_registry_create(void);
pmbus_error_t pmbus_device_registry_destroy(pmbus_device_registry_t *registry);

pmbus_error_t pmbus_register_device_type(pmbus_device_registry_t *registry,
                                        const pmbus_device_registry_entry_t *entry);
pmbus_error_t pmbus_unregister_device_type(pmbus_device_registry_t *registry,
                                          uint16_t manufacturer_id,
                                          uint16_t device_id);

const pmbus_device_registry_entry_t* pmbus_find_device_type(pmbus_device_registry_t *registry,
                                                           uint16_t manufacturer_id,
                                                           uint16_t device_id,
                                                           uint16_t revision);

pmbus_device_registry_entry_t** pmbus_enumerate_device_types(pmbus_device_registry_t *registry,
                                                            const pmbus_device_filter_t *filter,
                                                            size_t *count);

pmbus_error_t pmbus_validate_registry(pmbus_device_registry_t *registry);

/* Device factory functions */
pmbus_device_factory_t* pmbus_device_factory_create(pmbus_device_registry_t *registry,
                                                   pmbus_protocol_engine_t *engine);
pmbus_error_t pmbus_device_factory_destroy(pmbus_device_factory_t *factory);

pmbus_device_t* pmbus_create_device(pmbus_device_factory_t *factory,
                                   pmbus_transport_t *transport,
                                   uint8_t address);
pmbus_device_t* pmbus_create_device_by_type(pmbus_device_factory_t *factory,
                                           pmbus_transport_t *transport,
                                           uint8_t address,
                                           const pmbus_device_registry_entry_t *entry);
pmbus_error_t pmbus_destroy_device_instance(pmbus_device_factory_t *factory,
                                           pmbus_device_t *device);

pmbus_device_t* pmbus_clone_device(pmbus_device_factory_t *factory,
                                  const pmbus_device_t *source,
                                  uint8_t new_address);

pmbus_error_t pmbus_validate_device_instance(pmbus_device_factory_t *factory,
                                            pmbus_device_t *device);

/* Device identification functions */
pmbus_error_t pmbus_identify_device(pmbus_transport_t *transport,
                                   uint8_t address,
                                   pmbus_device_info_t *info,
                                   pmbus_device_capabilities_t *capabilities);

pmbus_error_t pmbus_probe_device_info(pmbus_transport_t *transport,
                                     uint8_t address,
                                     pmbus_device_info_t *info);

pmbus_error_t pmbus_detect_device_capabilities(pmbus_transport_t *transport,
                                              uint8_t address,
                                              pmbus_device_capabilities_t *capabilities);

pmbus_error_t pmbus_read_manufacturer_info(pmbus_transport_t *transport,
                                          uint8_t address,
                                          char *manufacturer_name,
                                          char *model_name,
                                          char *revision);

/* Built-in device type registrations */
pmbus_error_t pmbus_register_linear_technology_devices(pmbus_device_registry_t *registry);
pmbus_error_t pmbus_register_analog_devices_devices(pmbus_device_registry_t *registry);
pmbus_error_t pmbus_register_texas_instruments_devices(pmbus_device_registry_t *registry);
pmbus_error_t pmbus_register_generic_devices(pmbus_device_registry_t *registry);

/* Utility functions */
const char* pmbus_device_category_to_string(pmbus_device_category_t category);
const char* pmbus_discovery_state_to_string(pmbus_discovery_state_t state);

pmbus_error_t pmbus_create_device_filter(pmbus_device_filter_t *filter);
pmbus_error_t pmbus_set_filter_category(pmbus_device_filter_t *filter,
                                       pmbus_device_category_t category);
pmbus_error_t pmbus_set_filter_manufacturer(pmbus_device_filter_t *filter,
                                           uint16_t manufacturer_id,
                                           const char *manufacturer_name);
pmbus_error_t pmbus_set_filter_capabilities(pmbus_device_filter_t *filter,
                                           uint32_t required_capabilities,
                                           bool require_all);

bool pmbus_device_matches_filter(const pmbus_device_t *device,
                                const pmbus_device_filter_t *filter);

/* Address management */
bool pmbus_is_address_reserved(uint8_t address);
bool pmbus_is_address_valid(uint8_t address);
uint8_t pmbus_get_next_scan_address(uint8_t current_address,
                                   const uint8_t *ignore_list,
                                   size_t ignore_count);

/* Discovery optimization */
pmbus_error_t pmbus_optimize_scan_order(pmbus_discovery_engine_t *engine,
                                       uint8_t *optimized_addresses,
                                       size_t *count);
pmbus_error_t pmbus_estimate_scan_time(const pmbus_discovery_config_t *config,
                                      uint32_t *estimated_time_ms);

#ifdef __cplusplus
}
#endif

#endif /* PMBUS_DISCOVERY_H */
