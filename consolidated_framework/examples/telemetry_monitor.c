/**
 * @file telemetry_monitor.c
 * @brief PMBus device telemetry monitoring example
 * 
 * This example demonstrates how to monitor telemetry data from PMBus devices,
 * including voltage, current, temperature, and power measurements.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include <pmbus/transport.h>
#include <pmbus/protocol.h>
#include <pmbus/discovery.h>
#include <pmbus/data_conversion.h>

static volatile bool monitoring_active = true;

static void signal_handler(int signal)
{
    (void)signal;
    monitoring_active = false;
    printf("\nStopping telemetry monitoring...\n");
}

static void print_telemetry(pmbus_device_t *device)
{
    pmbus_telemetry_t telemetry;
    pmbus_result_t result;
    time_t now;
    struct tm *tm_info;
    char time_str[64];
    
    // Get current time
    time(&now);
    tm_info = localtime(&now);
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
    
    // Read telemetry data
    result = pmbus_read_telemetry(device, &telemetry);
    if (result != PMBUS_SUCCESS) {
        printf("[%s] Device 0x%02X: Failed to read telemetry: %s\n",
               time_str, device->address, pmbus_result_to_string(result));
        return;
    }
    
    printf("[%s] Device 0x%02X Telemetry:\n", time_str, device->address);
    
    // Print voltage measurements
    if (telemetry.voltages.input_valid) {
        printf("  Input Voltage:  %8.3f V\n", telemetry.voltages.input);
    }
    if (telemetry.voltages.output_valid) {
        printf("  Output Voltage: %8.3f V\n", telemetry.voltages.output);
    }
    
    // Print current measurements
    if (telemetry.currents.input_valid) {
        printf("  Input Current:  %8.3f A\n", telemetry.currents.input);
    }
    if (telemetry.currents.output_valid) {
        printf("  Output Current: %8.3f A\n", telemetry.currents.output);
    }
    
    // Print power measurements
    if (telemetry.power.input_valid) {
        printf("  Input Power:    %8.3f W\n", telemetry.power.input);
    }
    if (telemetry.power.output_valid) {
        printf("  Output Power:   %8.3f W\n", telemetry.power.output);
    }
    
    // Print temperature measurements
    if (telemetry.temperatures.internal_valid) {
        printf("  Internal Temp:  %8.1f °C\n", telemetry.temperatures.internal);
    }
    if (telemetry.temperatures.external_valid) {
        printf("  External Temp:  %8.1f °C\n", telemetry.temperatures.external);
    }
    
    // Print efficiency if both input and output power are valid
    if (telemetry.power.input_valid && telemetry.power.output_valid && 
        telemetry.power.input > 0.001) {
        double efficiency = (telemetry.power.output / telemetry.power.input) * 100.0;
        printf("  Efficiency:     %8.2f %%\n", efficiency);
    }
    
    // Print status information
    pmbus_status_word_t status;
    if (pmbus_read_status_word(device, &status) == PMBUS_SUCCESS) {
        if (status.raw != 0) {
            printf("  Status Flags:   0x%04X", status.raw);
            if (status.flags.vout_ov_fault) printf(" VOUT_OV");
            if (status.flags.iout_oc_fault) printf(" IOUT_OC");
            if (status.flags.vin_uv_fault) printf(" VIN_UV");
            if (status.flags.temperature_fault) printf(" TEMP");
            if (status.flags.cml_fault) printf(" CML");
            if (status.flags.power_good_negated) printf(" !PGOOD");
            printf("\n");
        }
    }
    
    printf("\n");
}

static void print_usage(const char *prog_name)
{
    printf("Usage: %s [-b bus_number] [-a address] [-i interval] [-h]\n", prog_name);
    printf("Options:\n");
    printf("  -b bus_number  I2C bus number (default: 1)\n");
    printf("  -a address     Device address in hex (default: scan all)\n");
    printf("  -i interval    Monitoring interval in seconds (default: 1)\n");
    printf("  -h             Show this help message\n");
    printf("\nPress Ctrl+C to stop monitoring.\n");
}

int main(int argc, char *argv[])
{
    int bus_number = 1;
    int target_address = -1;
    int interval = 1;
    int opt;
    
    // Parse command line options
    while ((opt = getopt(argc, argv, "b:a:i:h")) != -1) {
        switch (opt) {
            case 'b':
                bus_number = atoi(optarg);
                break;
            case 'a':
                target_address = (int)strtol(optarg, NULL, 16);
                break;
            case 'i':
                interval = atoi(optarg);
                if (interval < 1) interval = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Set up signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("PMBus Telemetry Monitor\n");
    printf("=======================\n");
    
    if (target_address >= 0) {
        printf("Target device: 0x%02X\n", target_address);
    } else {
        printf("Monitoring all discovered devices\n");
    }
    printf("Update interval: %d second(s)\n\n", interval);
    
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
    
    // Initialize discovery engine if scanning all devices
    pmbus_discovery_t discovery;
    pmbus_device_t **devices = NULL;
    size_t num_devices = 0;
    pmbus_device_t single_device;
    
    if (target_address >= 0) {
        // Monitor single device
        memset(&single_device, 0, sizeof(single_device));
        single_device.address = (uint8_t)target_address;
        single_device.transport = &transport;
        
        // Try to detect device capabilities
        pmbus_detect_device_capabilities(&single_device);
        
        devices = &(pmbus_device_t*){&single_device};
        num_devices = 1;
    } else {
        // Discover all devices
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
        
        result = pmbus_discovery_scan(&discovery);
        if (result != PMBUS_SUCCESS) {
            fprintf(stderr, "Device discovery failed: %s\n",
                    pmbus_result_to_string(result));
            pmbus_discovery_cleanup(&discovery);
            pmbus_transport_cleanup(&transport);
            return 1;
        }
        
        result = pmbus_discovery_get_devices(&discovery, &devices, &num_devices);
        if (result != PMBUS_SUCCESS) {
            fprintf(stderr, "Failed to retrieve discovered devices: %s\n",
                    pmbus_result_to_string(result));
            pmbus_discovery_cleanup(&discovery);
            pmbus_transport_cleanup(&transport);
            return 1;
        }
    }
    
    if (num_devices == 0) {
        printf("No PMBus devices found for monitoring.\n");
        if (target_address < 0) {
            pmbus_discovery_cleanup(&discovery);
        }
        pmbus_transport_cleanup(&transport);
        return 1;
    }
    
    printf("Starting telemetry monitoring for %zu device(s)...\n\n", num_devices);
    
    // Main monitoring loop
    while (monitoring_active) {
        for (size_t i = 0; i < num_devices && monitoring_active; i++) {
            print_telemetry(devices[i]);
        }
        
        if (monitoring_active) {
            sleep(interval);
        }
    }
    
    // Print final statistics
    printf("\nTransport Statistics:\n");
    pmbus_transport_stats_t transport_stats;
    pmbus_transport_get_stats(&transport, &transport_stats);
    printf("  Total transactions: %u\n", transport_stats.total_transactions);
    printf("  Successful transactions: %u\n", transport_stats.successful_transactions);
    printf("  Failed transactions: %u\n", transport_stats.failed_transactions);
    printf("  PEC errors: %u\n", transport_stats.pec_errors);
    printf("  Timeout errors: %u\n", transport_stats.timeout_errors);
    printf("  Retry count: %u\n", transport_stats.retry_count);
    printf("  Average transaction time: %.2f ms\n", transport_stats.avg_transaction_time_ms);
    
    // Cleanup
    if (target_address < 0) {
        pmbus_discovery_cleanup(&discovery);
    }
    pmbus_transport_cleanup(&transport);
    
    printf("\nTelemetry monitoring stopped.\n");
    
    return 0;
}
