/**
 * @file basic_discovery.c
 * @brief Basic PMBus device discovery example
 * 
 * This example demonstrates how to use the PMBus consolidated framework
 * to discover devices on the I2C/SMBus and retrieve basic information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pmbus/transport.h>
#include <pmbus/protocol.h>
#include <pmbus/discovery.h>

static void print_device_info(pmbus_device_t *device)
{
    if (!device) return;
    
    printf("Device Information:\n");
    printf("  Address: 0x%02X\n", device->address);
    printf("  Type: %s\n", pmbus_device_type_to_string(device->type));
    
    // Try to read basic device information
    char mfr_id[32] = {0};
    char mfr_model[32] = {0};
    char mfr_revision[16] = {0};
    
    if (pmbus_read_string(device, PMBUS_CMD_MFR_ID, mfr_id, sizeof(mfr_id) - 1) == PMBUS_SUCCESS) {
        printf("  Manufacturer ID: %s\n", mfr_id);
    }
    
    if (pmbus_read_string(device, PMBUS_CMD_MFR_MODEL, mfr_model, sizeof(mfr_model) - 1) == PMBUS_SUCCESS) {
        printf("  Model: %s\n", mfr_model);
    }
    
    if (pmbus_read_string(device, PMBUS_CMD_MFR_REVISION, mfr_revision, sizeof(mfr_revision) - 1) == PMBUS_SUCCESS) {
        printf("  Revision: %s\n", mfr_revision);
    }
    
    printf("  Capabilities:\n");
    printf("    PEC Support: %s\n", device->capabilities.supports_pec ? "Yes" : "No");
    printf("    Extended Commands: %s\n", device->capabilities.supports_extended_commands ? "Yes" : "No");
    printf("    Group Commands: %s\n", device->capabilities.supports_group_commands ? "Yes" : "No");
    printf("    Pages: %d\n", device->capabilities.num_pages);
    
    printf("\n");
}

static void print_usage(const char *prog_name)
{
    printf("Usage: %s [-b bus_number] [-h]\n", prog_name);
    printf("Options:\n");
    printf("  -b bus_number  I2C bus number to scan (default: 1)\n");
    printf("  -h             Show this help message\n");
}

int main(int argc, char *argv[])
{
    int bus_number = 1;
    int opt;
    
    // Parse command line options
    while ((opt = getopt(argc, argv, "b:h")) != -1) {
        switch (opt) {
            case 'b':
                bus_number = atoi(optarg);
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    printf("PMBus Device Discovery Example\n");
    printf("==============================\n\n");
    
    // Initialize transport
    pmbus_transport_t transport;
    pmbus_transport_config_t transport_config = {
        .bus_number = bus_number,
        .enable_pec = true,
        .retry_count = 3,
        .retry_delay_ms = 10,
        .timeout_ms = 1000
    };
    
    pmbus_result_t result = pmbus_transport_init(&transport, &transport_config);
    if (result != PMBUS_SUCCESS) {
        fprintf(stderr, "Failed to initialize PMBus transport: %s\n", 
                pmbus_result_to_string(result));
        return 1;
    }
    
    printf("Initialized PMBus transport on bus %d\n\n", bus_number);
    
    // Initialize discovery engine
    pmbus_discovery_t discovery;
    pmbus_discovery_config_t discovery_config = {
        .scan_reserved_addresses = false,
        .verify_device_id = true,
        .enable_capability_detection = true,
        .discovery_timeout_ms = 5000
    };
    
    result = pmbus_discovery_init(&discovery, &transport, &discovery_config);
    if (result != PMBUS_SUCCESS) {
        fprintf(stderr, "Failed to initialize discovery engine: %s\n",
                pmbus_result_to_string(result));
        pmbus_transport_cleanup(&transport);
        return 1;
    }
    
    printf("Scanning for PMBus devices...\n\n");
    
    // Discover devices
    result = pmbus_discovery_scan(&discovery);
    if (result != PMBUS_SUCCESS) {
        fprintf(stderr, "Device discovery failed: %s\n",
                pmbus_result_to_string(result));
        pmbus_discovery_cleanup(&discovery);
        pmbus_transport_cleanup(&transport);
        return 1;
    }
    
    // Get discovered devices
    pmbus_device_t **devices = NULL;
    size_t num_devices = 0;
    
    result = pmbus_discovery_get_devices(&discovery, &devices, &num_devices);
    if (result != PMBUS_SUCCESS) {
        fprintf(stderr, "Failed to retrieve discovered devices: %s\n",
                pmbus_result_to_string(result));
        pmbus_discovery_cleanup(&discovery);
        pmbus_transport_cleanup(&transport);
        return 1;
    }
    
    if (num_devices == 0) {
        printf("No PMBus devices found on bus %d\n", bus_number);
    } else {
        printf("Found %zu PMBus device(s):\n\n", num_devices);
        
        for (size_t i = 0; i < num_devices; i++) {
            printf("Device %zu:\n", i + 1);
            print_device_info(devices[i]);
        }
    }
    
    // Print discovery statistics
    pmbus_discovery_stats_t stats;
    pmbus_discovery_get_stats(&discovery, &stats);
    
    printf("Discovery Statistics:\n");
    printf("  Addresses scanned: %u\n", stats.addresses_scanned);
    printf("  Devices found: %u\n", stats.devices_found);
    printf("  Scan duration: %u ms\n", stats.scan_duration_ms);
    printf("  Average response time: %.2f ms\n", stats.avg_response_time_ms);
    
    // Cleanup
    pmbus_discovery_cleanup(&discovery);
    pmbus_transport_cleanup(&transport);
    
    printf("\nDiscovery completed successfully.\n");
    
    return 0;
}
