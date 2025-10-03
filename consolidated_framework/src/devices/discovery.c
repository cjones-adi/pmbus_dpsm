#include "pmbus/discovery.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/**
 * @file discovery.c
 * @brief PMBus Device Discovery and Factory System Implementation
 */

/* Reserved PMBus addresses */
static const uint8_t reserved_addresses[] = {
    0x00, /* General Call */
    0x01, /* START byte */
    0x02, /* CBUS address */
    0x03, /* Different bus format */
    0x04, 0x05, 0x06, 0x07, /* Reserved */
    0x78, 0x79, 0x7A, 0x7B, /* 10-bit addressing */
    0x7C, 0x7D, 0x7E, 0x7F  /* Reserved */
};

static const size_t reserved_addresses_count = sizeof(reserved_addresses) / sizeof(uint8_t);

/* Internal helper functions */
static uint32_t get_timestamp_ms(void);
static bool is_device_present(pmbus_transport_t *transport, uint8_t address);
static pmbus_error_t probe_device_basic_info(pmbus_transport_t *transport, uint8_t address,
                                            pmbus_device_info_t *info);
static pmbus_error_t probe_device_capabilities(pmbus_transport_t *transport, uint8_t address,
                                              pmbus_device_capabilities_t *capabilities);
static const pmbus_device_registry_entry_t* match_device_to_registry(pmbus_device_registry_t *registry,
                                                                    const pmbus_device_info_t *info);

/* Discovery engine functions */
pmbus_discovery_engine_t* pmbus_discovery_engine_create(pmbus_transport_t *transport,
                                                       pmbus_device_factory_t *factory) {
    if (transport == NULL || factory == NULL) {
        return NULL;
    }
    
    pmbus_discovery_engine_t *engine = calloc(1, sizeof(pmbus_discovery_engine_t));
    if (engine == NULL) {
        return NULL;
    }
    
    engine->transport = transport;
    engine->factory = factory;
    engine->state = PMBUS_DISCOVERY_STATE_IDLE;
    
    /* Set default configuration */
    engine->config.start_address = 0x08;
    engine->config.end_address = 0x77;
    engine->config.scan_timeout_ms = 100;
    engine->config.deep_scan = true;
    engine->config.validate_devices = true;
    engine->config.auto_configure = false;
    engine->config.parallel_scan = false;
    engine->config.scan_retries = 3;
    engine->config.ignore_count = 0;
    
    return engine;
}

pmbus_error_t pmbus_discovery_engine_destroy(pmbus_discovery_engine_t *engine) {
    if (engine == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    /* Clean up discovered devices */
    for (size_t i = 0; i < engine->device_count; i++) {
        if (engine->discovered_devices[i] != NULL) {
            pmbus_destroy_device_instance(engine->factory, engine->discovered_devices[i]);
        }
    }
    
    free(engine);
    return PMBUS_SUCCESS;
}

pmbus_error_t pmbus_discovery_engine_configure(pmbus_discovery_engine_t *engine,
                                              const pmbus_discovery_config_t *config) {
    if (engine == NULL || config == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    if (engine->state != PMBUS_DISCOVERY_STATE_IDLE) {
        return PMBUS_ERROR_INVALID_STATE;
    }
    
    engine->config = *config;
    return PMBUS_SUCCESS;
}

pmbus_error_t pmbus_start_discovery(pmbus_discovery_engine_t *engine) {
    if (engine == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    if (engine->state != PMBUS_DISCOVERY_STATE_IDLE) {
        return PMBUS_ERROR_INVALID_STATE;
    }
    
    engine->state = PMBUS_DISCOVERY_STATE_SCANNING;
    engine->scan_start_time = get_timestamp_ms();
    engine->current_scan_address = engine->config.start_address;
    engine->device_count = 0;
    
    /* Reset statistics */
    engine->addresses_scanned = 0;
    engine->devices_found = 0;
    engine->devices_identified = 0;
    engine->devices_configured = 0;
    engine->scan_errors = 0;
    
    /* Perform the scan */
    return pmbus_scan_address_range(engine, engine->config.start_address, engine->config.end_address);
}

pmbus_error_t pmbus_scan_address_range(pmbus_discovery_engine_t *engine,
                                      uint8_t start_address,
                                      uint8_t end_address) {
    if (engine == NULL || start_address > end_address) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    pmbus_error_t overall_result = PMBUS_SUCCESS;
    
    for (uint8_t address = start_address; address <= end_address; address++) {
        if (engine->state == PMBUS_DISCOVERY_STATE_ABORTED) {
            break;
        }
        
        /* Skip reserved addresses */
        if (pmbus_is_address_reserved(address)) {
            continue;
        }
        
        /* Skip ignored addresses */
        bool should_ignore = false;
        for (size_t i = 0; i < engine->config.ignore_count; i++) {
            if (engine->config.ignore_addresses[i] == address) {
                should_ignore = true;
                break;
            }
        }
        if (should_ignore) {
            continue;
        }
        
        engine->current_scan_address = address;
        engine->addresses_scanned++;
        
        /* Update progress */
        uint32_t total_addresses = end_address - start_address + 1;
        engine->scan_progress_percent = ((address - start_address) * 100) / total_addresses;
        
        /* Call progress callback if provided */
        if (engine->config.progress_callback) {
            engine->config.progress_callback(address, false, engine->config.user_data);
        }
        
        pmbus_device_t *device = NULL;
        pmbus_error_t result = pmbus_discover_at_address(engine, address, &device);
        
        if (result == PMBUS_SUCCESS && device != NULL) {
            engine->devices_found++;
            
            /* Add to discovered devices list */
            if (engine->device_count < 128) {
                engine->discovered_devices[engine->device_count++] = device;
                
                /* Call discovery callback if provided */
                if (engine->config.discovery_callback) {
                    engine->config.discovery_callback(device, engine->config.user_data);
                }
                
                /* Update progress callback with found device */
                if (engine->config.progress_callback) {
                    engine->config.progress_callback(address, true, engine->config.user_data);
                }
            }
        } else if (result != PMBUS_ERROR_DEVICE_NOT_FOUND) {
            engine->scan_errors++;
            if (overall_result == PMBUS_SUCCESS) {
                overall_result = result;
            }
        }
    }
    
    engine->state = PMBUS_DISCOVERY_STATE_COMPLETED;
    engine->total_scan_time_ms = get_timestamp_ms() - engine->scan_start_time;
    
    return overall_result;
}

pmbus_error_t pmbus_discover_at_address(pmbus_discovery_engine_t *engine,
                                       uint8_t address,
                                       pmbus_device_t **device) {
    if (engine == NULL || device == NULL || !pmbus_is_address_valid(address)) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    *device = NULL;
    
    /* Check if device is present at this address */
    if (!is_device_present(engine->transport, address)) {
        return PMBUS_ERROR_DEVICE_NOT_FOUND;
    }
    
    /* Identify the device */
    pmbus_device_info_t device_info;
    pmbus_device_capabilities_t device_capabilities;
    
    pmbus_error_t result = pmbus_identify_device(engine->transport, address, 
                                               &device_info, &device_capabilities);
    if (result != PMBUS_SUCCESS) {
        return result;
    }
    
    engine->devices_identified++;
    
    /* Create device instance */
    *device = pmbus_create_device(engine->factory, engine->transport, address);
    if (*device == NULL) {
        return PMBUS_ERROR_NO_MEMORY;
    }
    
    /* Copy device information */
    (*device)->info = device_info;
    (*device)->capabilities = device_capabilities;
    
    /* Configure device if requested */
    if (engine->config.auto_configure) {
        result = pmbus_device_init(*device);
        if (result == PMBUS_SUCCESS) {
            engine->devices_configured++;
        }
    }
    
    return PMBUS_SUCCESS;
}

/* Device registry functions */
pmbus_device_registry_t* pmbus_device_registry_create(void) {
    pmbus_device_registry_t *registry = calloc(1, sizeof(pmbus_device_registry_t));
    if (registry == NULL) {
        return NULL;
    }
    
    return registry;
}

pmbus_error_t pmbus_device_registry_destroy(pmbus_device_registry_t *registry) {
    if (registry == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    free(registry);
    return PMBUS_SUCCESS;
}

pmbus_error_t pmbus_register_device_type(pmbus_device_registry_t *registry,
                                        const pmbus_device_registry_entry_t *entry) {
    if (registry == NULL || entry == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    if (registry->entry_count >= 256) {
        return PMBUS_ERROR_NO_MEMORY;
    }
    
    /* Check for duplicate entries */
    for (size_t i = 0; i < registry->entry_count; i++) {
        if (registry->entries[i].manufacturer_id == entry->manufacturer_id &&
            registry->entries[i].device_id == entry->device_id) {
            return PMBUS_ERROR_INVALID_STATE; /* Already registered */
        }
    }
    
    registry->entries[registry->entry_count++] = *entry;
    return PMBUS_SUCCESS;
}

const pmbus_device_registry_entry_t* pmbus_find_device_type(pmbus_device_registry_t *registry,
                                                           uint16_t manufacturer_id,
                                                           uint16_t device_id,
                                                           uint16_t revision) {
    if (registry == NULL) {
        return NULL;
    }
    
    const pmbus_device_registry_entry_t *best_match = NULL;
    uint8_t highest_priority = 0;
    
    for (size_t i = 0; i < registry->entry_count; i++) {
        const pmbus_device_registry_entry_t *entry = &registry->entries[i];
        
        if (entry->manufacturer_id == manufacturer_id &&
            entry->device_id == device_id &&
            (entry->revision_mask == 0 || 
             (revision & entry->revision_mask) == entry->revision_value)) {
            
            if (entry->detection_priority > highest_priority) {
                best_match = entry;
                highest_priority = entry->detection_priority;
            }
        }
    }
    
    return best_match;
}

/* Device factory functions */
pmbus_device_factory_t* pmbus_device_factory_create(pmbus_device_registry_t *registry,
                                                   pmbus_protocol_engine_t *engine) {
    if (registry == NULL || engine == NULL) {
        return NULL;
    }
    
    pmbus_device_factory_t *factory = calloc(1, sizeof(pmbus_device_factory_t));
    if (factory == NULL) {
        return NULL;
    }
    
    factory->registry = registry;
    factory->protocol_engine = engine;
    
    return factory;
}

pmbus_error_t pmbus_device_factory_destroy(pmbus_device_factory_t *factory) {
    if (factory == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    /* Destroy all created devices */
    for (size_t i = 0; i < factory->device_count; i++) {
        if (factory->created_devices[i] != NULL) {
            pmbus_device_destroy(factory->created_devices[i]);
        }
    }
    
    free(factory);
    return PMBUS_SUCCESS;
}

pmbus_device_t* pmbus_create_device(pmbus_device_factory_t *factory,
                                   pmbus_transport_t *transport,
                                   uint8_t address) {
    if (factory == NULL || transport == NULL || !pmbus_is_address_valid(address)) {
        return NULL;
    }
    
    if (factory->device_count >= 128) {
        return NULL; /* Too many devices */
    }
    
    /* Create basic device */
    pmbus_device_t *device = NULL;
    pmbus_error_t result = pmbus_device_create(factory->protocol_engine, address, &device);
    if (result != PMBUS_SUCCESS || device == NULL) {
        return NULL;
    }
    
    /* Try to identify device and find specific implementation */
    pmbus_device_info_t device_info;
    if (pmbus_probe_device_info(transport, address, &device_info) == PMBUS_SUCCESS) {
        device->info = device_info;
        
        /* Find registry entry for this device */
        const pmbus_device_registry_entry_t *entry = pmbus_find_device_type(
            factory->registry, device_info.manufacturer_id, 
            device_info.device_id, device_info.device_revision);
        
        if (entry != NULL) {
            /* Use device-specific implementation if available */
            if (entry->factory_func != NULL) {
                pmbus_device_t *specific_device = entry->factory_func(transport, address, &device_info);
                if (specific_device != NULL) {
                    pmbus_device_destroy(device);
                    device = specific_device;
                }
            }
            
            /* Set device-specific operations and command table */
            if (entry->ops != NULL) {
                device->ops = entry->ops;
            }
            if (entry->cmd_table != NULL) {
                device->cmd_table = entry->cmd_table;
                device->cmd_table_size = entry->cmd_table_size;
            }
        }
    }
    
    /* Add to factory's device list */
    factory->created_devices[factory->device_count++] = device;
    
    return device;
}

/* Device identification functions */
pmbus_error_t pmbus_identify_device(pmbus_transport_t *transport,
                                   uint8_t address,
                                   pmbus_device_info_t *info,
                                   pmbus_device_capabilities_t *capabilities) {
    if (transport == NULL || info == NULL || capabilities == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    /* Probe basic device information */
    pmbus_error_t result = pmbus_probe_device_info(transport, address, info);
    if (result != PMBUS_SUCCESS) {
        return result;
    }
    
    /* Probe device capabilities */
    result = pmbus_probe_device_capabilities(transport, address, capabilities);
    if (result != PMBUS_SUCCESS) {
        /* Non-fatal - continue with basic capabilities */
        memset(capabilities, 0, sizeof(pmbus_device_capabilities_t));
        capabilities->max_pages = 1;
    }
    
    return PMBUS_SUCCESS;
}

pmbus_error_t pmbus_probe_device_info(pmbus_transport_t *transport,
                                     uint8_t address,
                                     pmbus_device_info_t *info) {
    if (transport == NULL || info == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    memset(info, 0, sizeof(pmbus_device_info_t));
    
    /* Try to read manufacturer ID */
    uint8_t mfr_data[16];
    size_t mfr_length = sizeof(mfr_data);
    if (pmbus_read_block(transport, address, PMBUS_CMD_MFR_ID, mfr_data, &mfr_length) == PMBUS_SUCCESS) {
        if (mfr_length >= 2) {
            info->manufacturer_id = mfr_data[0] | (mfr_data[1] << 8);
        }
        if (mfr_length > 2) {
            size_t name_len = (mfr_length - 2 < sizeof(info->manufacturer_name) - 1) ? 
                             (mfr_length - 2) : (sizeof(info->manufacturer_name) - 1);
            memcpy(info->manufacturer_name, &mfr_data[2], name_len);
            info->manufacturer_name[name_len] = '\0';
        }
    }
    
    /* Try to read device model */
    uint8_t model_data[16];
    size_t model_length = sizeof(model_data);
    if (pmbus_read_block(transport, address, PMBUS_CMD_MFR_MODEL, model_data, &model_length) == PMBUS_SUCCESS) {
        size_t copy_len = (model_length < sizeof(info->device_model) - 1) ? 
                         model_length : (sizeof(info->device_model) - 1);
        memcpy(info->device_model, model_data, copy_len);
        info->device_model[copy_len] = '\0';
    }
    
    /* Try to read device revision */
    uint8_t rev_data[8];
    size_t rev_length = sizeof(rev_data);
    if (pmbus_read_block(transport, address, PMBUS_CMD_MFR_REVISION, rev_data, &rev_length) == PMBUS_SUCCESS) {
        size_t copy_len = (rev_length < sizeof(info->device_revision_str) - 1) ? 
                         rev_length : (sizeof(info->device_revision_str) - 1);
        memcpy(info->device_revision_str, rev_data, copy_len);
        info->device_revision_str[copy_len] = '\0';
    }
    
    /* Try to read PMBus revision */
    uint8_t pmbus_rev;
    if (pmbus_read_byte(transport, address, &pmbus_rev) == PMBUS_SUCCESS) {
        info->pmbus_revision = pmbus_rev;
    } else {
        info->pmbus_revision = 0x11; /* Assume PMBus 1.1 */
    }
    
    /* Try to read IC device ID and revision */
    uint8_t ic_id_data[8];
    size_t ic_id_length = sizeof(ic_id_data);
    if (pmbus_read_block(transport, address, PMBUS_CMD_IC_DEVICE_ID, ic_id_data, &ic_id_length) == PMBUS_SUCCESS) {
        if (ic_id_length >= 2) {
            info->device_id = ic_id_data[0] | (ic_id_data[1] << 8);
        }
        if (ic_id_length > 2) {
            size_t copy_len = (ic_id_length - 2 < sizeof(info->ic_device_id) - 1) ? 
                             (ic_id_length - 2) : (sizeof(info->ic_device_id) - 1);
            memcpy(info->ic_device_id, &ic_id_data[2], copy_len);
            info->ic_device_id[copy_len] = '\0';
        }
    }
    
    uint8_t ic_rev;
    if (pmbus_read_byte(transport, address, &ic_rev) == PMBUS_SUCCESS) {
        info->ic_device_rev = ic_rev;
        info->device_revision = ic_rev;
    }
    
    return PMBUS_SUCCESS;
}

/* Utility functions */
const char* pmbus_device_category_to_string(pmbus_device_category_t category) {
    switch (category) {
        case PMBUS_DEVICE_CATEGORY_CONTROLLER: return "CONTROLLER";
        case PMBUS_DEVICE_CATEGORY_MANAGER: return "MANAGER";
        case PMBUS_DEVICE_CATEGORY_SEQUENCER: return "SEQUENCER";
        case PMBUS_DEVICE_CATEGORY_MONITOR: return "MONITOR";
        case PMBUS_DEVICE_CATEGORY_REGULATOR: return "REGULATOR";
        case PMBUS_DEVICE_CATEGORY_CONVERTER: return "CONVERTER";
        case PMBUS_DEVICE_CATEGORY_PROTECTION: return "PROTECTION";
        case PMBUS_DEVICE_CATEGORY_HYBRID: return "HYBRID";
        case PMBUS_DEVICE_CATEGORY_GENERIC: return "GENERIC";
        case PMBUS_DEVICE_CATEGORY_UNKNOWN: return "UNKNOWN";
        default: return "INVALID";
    }
}

const char* pmbus_discovery_state_to_string(pmbus_discovery_state_t state) {
    switch (state) {
        case PMBUS_DISCOVERY_STATE_IDLE: return "IDLE";
        case PMBUS_DISCOVERY_STATE_SCANNING: return "SCANNING";
        case PMBUS_DISCOVERY_STATE_IDENTIFYING: return "IDENTIFYING";
        case PMBUS_DISCOVERY_STATE_CONFIGURING: return "CONFIGURING";
        case PMBUS_DISCOVERY_STATE_COMPLETED: return "COMPLETED";
        case PMBUS_DISCOVERY_STATE_ERROR: return "ERROR";
        case PMBUS_DISCOVERY_STATE_ABORTED: return "ABORTED";
        default: return "INVALID";
    }
}

bool pmbus_is_address_reserved(uint8_t address) {
    for (size_t i = 0; i < reserved_addresses_count; i++) {
        if (reserved_addresses[i] == address) {
            return true;
        }
    }
    return false;
}

bool pmbus_is_address_valid(uint8_t address) {
    return (address <= 0x7F && !pmbus_is_address_reserved(address));
}

/* Internal helper functions */
static uint32_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static bool is_device_present(pmbus_transport_t *transport, uint8_t address) {
    if (transport == NULL) {
        return false;
    }
    
    /* Try to read STATUS_BYTE command - most devices support this */
    uint8_t status;
    pmbus_error_t result = pmbus_read_byte(transport, address, &status);
    
    return (result == PMBUS_SUCCESS);
}

static pmbus_error_t probe_device_capabilities(pmbus_transport_t *transport, uint8_t address,
                                              pmbus_device_capabilities_t *capabilities) {
    if (transport == NULL || capabilities == NULL) {
        return PMBUS_ERROR_INVALID_PARAM;
    }
    
    memset(capabilities, 0, sizeof(pmbus_device_capabilities_t));
    
    /* Set default capabilities */
    capabilities->max_pages = 1;
    capabilities->max_block_size = PMBUS_MAX_BLOCK_SIZE;
    capabilities->timing_accuracy_us = 1000;
    
    /* Try to probe for common commands */
    uint8_t test_commands[] = {
        PMBUS_CMD_OPERATION, PMBUS_CMD_STATUS_BYTE, PMBUS_CMD_STATUS_WORD,
        PMBUS_CMD_READ_VIN, PMBUS_CMD_READ_VOUT, PMBUS_CMD_READ_IOUT,
        PMBUS_CMD_READ_TEMPERATURE_1, PMBUS_CMD_READ_POUT, PMBUS_CMD_READ_PIN
    };
    
    for (size_t i = 0; i < sizeof(test_commands); i++) {
        uint8_t cmd = test_commands[i];
        uint8_t dummy_data[4];
        
        /* Try reading word data */
        uint16_t word_data;
        if (pmbus_read_word(transport, address, cmd, &word_data) == PMBUS_SUCCESS) {
            uint32_t word_index = cmd / 32;
            uint32_t bit_index = cmd % 32;
            if (word_index < 8) {
                capabilities->supported_commands[word_index] |= (1U << bit_index);
            }
        }
    }
    
    /* Check for voltage, current, power, and temperature command support */
    if (capabilities->supported_commands[PMBUS_CMD_READ_VIN / 32] & (1U << (PMBUS_CMD_READ_VIN % 32))) {
        capabilities->voltage_mode_commands = true;
    }
    if (capabilities->supported_commands[PMBUS_CMD_READ_IOUT / 32] & (1U << (PMBUS_CMD_READ_IOUT % 32))) {
        capabilities->current_mode_commands = true;
    }
    if (capabilities->supported_commands[PMBUS_CMD_READ_POUT / 32] & (1U << (PMBUS_CMD_READ_POUT % 32))) {
        capabilities->power_mode_commands = true;
    }
    if (capabilities->supported_commands[PMBUS_CMD_READ_TEMPERATURE_1 / 32] & (1U << (PMBUS_CMD_READ_TEMPERATURE_1 % 32))) {
        capabilities->temperature_commands = true;
    }
    
    return PMBUS_SUCCESS;
}
