/**
 * @file command_shell.c
 * @brief Interactive PMBus command shell example
 * 
 * This example provides an interactive command-line interface for
 * executing PMBus commands on discovered devices.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#include <pmbus/transport.h>
#include <pmbus/protocol.h>
#include <pmbus/discovery.h>
#include <pmbus/data_conversion.h>

#define MAX_COMMAND_LEN 256
#define MAX_ARGS 10

typedef struct {
    pmbus_transport_t transport;
    pmbus_discovery_t discovery;
    pmbus_device_t **devices;
    size_t num_devices;
    pmbus_device_t *current_device;
} shell_context_t;

static void print_help(void)
{
    printf("\nPMBus Command Shell Help\n");
    printf("========================\n\n");
    printf("General Commands:\n");
    printf("  help                    - Show this help message\n");
    printf("  list                    - List discovered devices\n");
    printf("  select <address>        - Select device by address (hex)\n");
    printf("  current                 - Show current selected device\n");
    printf("  scan                    - Re-scan for devices\n");
    printf("  stats                   - Show transport statistics\n");
    printf("  quit, exit              - Exit the shell\n\n");
    
    printf("PMBus Commands (require device selection):\n");
    printf("  read <command>          - Read PMBus command (hex)\n");
    printf("  write <command> <data>  - Write PMBus command with data (hex)\n");
    printf("  page <page>             - Set page number\n");
    printf("  clear_faults            - Clear all faults\n");
    printf("  operation <mode>        - Set operation mode (on/off/margin)\n\n");
    
    printf("Telemetry Commands:\n");
    printf("  voltage                 - Read voltage measurements\n");
    printf("  current                 - Read current measurements\n");
    printf("  power                   - Read power measurements\n");
    printf("  temperature             - Read temperature measurements\n");
    printf("  status                  - Read status registers\n");
    printf("  telemetry               - Read all telemetry data\n\n");
    
    printf("Data Format Commands:\n");
    printf("  linear11 <value>        - Convert value to LINEAR11 format\n");
    printf("  linear16 <value>        - Convert value to LINEAR16 format\n");
    printf("  direct <value>          - Convert value to DIRECT format\n\n");
}

static void print_device_list(shell_context_t *ctx)
{
    if (ctx->num_devices == 0) {
        printf("No devices discovered. Use 'scan' to search for devices.\n");
        return;
    }
    
    printf("Discovered PMBus Devices:\n");
    printf("=========================\n");
    for (size_t i = 0; i < ctx->num_devices; i++) {
        pmbus_device_t *device = ctx->devices[i];
        printf("%c 0x%02X - %s", 
               (device == ctx->current_device) ? '*' : ' ',
               device->address,
               pmbus_device_type_to_string(device->type));
        
        // Try to read manufacturer and model
        char mfr_id[32] = {0};
        char model[32] = {0};
        if (pmbus_read_string(device, PMBUS_CMD_MFR_ID, mfr_id, sizeof(mfr_id) - 1) == PMBUS_SUCCESS &&
            pmbus_read_string(device, PMBUS_CMD_MFR_MODEL, model, sizeof(model) - 1) == PMBUS_SUCCESS) {
            printf(" (%s %s)", mfr_id, model);
        }
        printf("\n");
    }
    printf("\n* = currently selected device\n\n");
}

static pmbus_device_t *find_device_by_address(shell_context_t *ctx, uint8_t address)
{
    for (size_t i = 0; i < ctx->num_devices; i++) {
        if (ctx->devices[i]->address == address) {
            return ctx->devices[i];
        }
    }
    return NULL;
}

static void execute_read_command(shell_context_t *ctx, const char *cmd_str)
{
    if (!ctx->current_device) {
        printf("Error: No device selected. Use 'select <address>' first.\n");
        return;
    }
    
    uint8_t command = (uint8_t)strtol(cmd_str, NULL, 16);
    uint8_t data[32];
    size_t data_len = sizeof(data);
    
    pmbus_result_t result = pmbus_read_command(ctx->current_device, command, data, &data_len);
    if (result != PMBUS_SUCCESS) {
        printf("Error reading command 0x%02X: %s\n", command, pmbus_result_to_string(result));
        return;
    }
    
    printf("Command 0x%02X returned %zu bytes: ", command, data_len);
    for (size_t i = 0; i < data_len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
    
    // Try to interpret as common data types
    if (data_len >= 2) {
        uint16_t word = (uint16_t)(data[1] << 8) | data[0];
        printf("  As uint16: %u (0x%04X)\n", word, word);
        
        // Try LINEAR11 conversion
        double linear11_val = pmbus_convert_linear11_to_float(word);
        printf("  As LINEAR11: %.6f\n", linear11_val);
    }
    
    if (data_len == 1) {
        printf("  As uint8: %u (0x%02X)\n", data[0], data[0]);
    }
}

static void execute_write_command(shell_context_t *ctx, const char *cmd_str, const char *data_str)
{
    if (!ctx->current_device) {
        printf("Error: No device selected. Use 'select <address>' first.\n");
        return;
    }
    
    uint8_t command = (uint8_t)strtol(cmd_str, NULL, 16);
    
    // Parse data string (space or comma separated hex values)
    uint8_t data[32];
    size_t data_len = 0;
    char *data_copy = strdup(data_str);
    char *token = strtok(data_copy, " ,");
    
    while (token && data_len < sizeof(data)) {
        data[data_len++] = (uint8_t)strtol(token, NULL, 16);
        token = strtok(NULL, " ,");
    }
    free(data_copy);
    
    if (data_len == 0) {
        printf("Error: No data provided for write command.\n");
        return;
    }
    
    pmbus_result_t result = pmbus_write_command(ctx->current_device, command, data, data_len);
    if (result != PMBUS_SUCCESS) {
        printf("Error writing command 0x%02X: %s\n", command, pmbus_result_to_string(result));
        return;
    }
    
    printf("Successfully wrote %zu bytes to command 0x%02X\n", data_len, command);
}

static void show_telemetry(shell_context_t *ctx)
{
    if (!ctx->current_device) {
        printf("Error: No device selected. Use 'select <address>' first.\n");
        return;
    }
    
    pmbus_telemetry_t telemetry;
    pmbus_result_t result = pmbus_read_telemetry(ctx->current_device, &telemetry);
    
    if (result != PMBUS_SUCCESS) {
        printf("Error reading telemetry: %s\n", pmbus_result_to_string(result));
        return;
    }
    
    printf("Device 0x%02X Telemetry Data:\n", ctx->current_device->address);
    printf("=============================\n");
    
    if (telemetry.voltages.input_valid) {
        printf("Input Voltage:   %8.3f V\n", telemetry.voltages.input);
    }
    if (telemetry.voltages.output_valid) {
        printf("Output Voltage:  %8.3f V\n", telemetry.voltages.output);
    }
    if (telemetry.currents.input_valid) {
        printf("Input Current:   %8.3f A\n", telemetry.currents.input);
    }
    if (telemetry.currents.output_valid) {
        printf("Output Current:  %8.3f A\n", telemetry.currents.output);
    }
    if (telemetry.power.input_valid) {
        printf("Input Power:     %8.3f W\n", telemetry.power.input);
    }
    if (telemetry.power.output_valid) {
        printf("Output Power:    %8.3f W\n", telemetry.power.output);
    }
    if (telemetry.temperatures.internal_valid) {
        printf("Internal Temp:   %8.1f °C\n", telemetry.temperatures.internal);
    }
    if (telemetry.temperatures.external_valid) {
        printf("External Temp:   %8.1f °C\n", telemetry.temperatures.external);
    }
}

static void show_transport_stats(shell_context_t *ctx)
{
    pmbus_transport_stats_t stats;
    pmbus_transport_get_stats(&ctx->transport, &stats);
    
    printf("Transport Statistics:\n");
    printf("====================\n");
    printf("Total transactions:     %u\n", stats.total_transactions);
    printf("Successful transactions: %u\n", stats.successful_transactions);
    printf("Failed transactions:    %u\n", stats.failed_transactions);
    printf("Success rate:          %.2f%%\n", 
           stats.total_transactions > 0 ? 
           (100.0 * stats.successful_transactions / stats.total_transactions) : 0.0);
    printf("PEC errors:            %u\n", stats.pec_errors);
    printf("Timeout errors:        %u\n", stats.timeout_errors);
    printf("Retry count:           %u\n", stats.retry_count);
    printf("Avg transaction time:  %.2f ms\n", stats.avg_transaction_time_ms);
}

static int tokenize_command(char *line, char **argv, int max_args)
{
    int argc = 0;
    char *token = strtok(line, " \t\n\r");
    
    while (token && argc < max_args - 1) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\n\r");
    }
    
    argv[argc] = NULL;
    return argc;
}

static void process_command(shell_context_t *ctx, char *line)
{
    char *argv[MAX_ARGS];
    int argc = tokenize_command(line, argv, MAX_ARGS);
    
    if (argc == 0) return;
    
    // Convert command to lowercase
    for (char *p = argv[0]; *p; p++) {
        *p = tolower(*p);
    }
    
    if (strcmp(argv[0], "help") == 0) {
        print_help();
    } else if (strcmp(argv[0], "list") == 0) {
        print_device_list(ctx);
    } else if (strcmp(argv[0], "select") == 0) {
        if (argc < 2) {
            printf("Usage: select <address>\n");
            return;
        }
        uint8_t address = (uint8_t)strtol(argv[1], NULL, 16);
        pmbus_device_t *device = find_device_by_address(ctx, address);
        if (device) {
            ctx->current_device = device;
            printf("Selected device 0x%02X\n", address);
        } else {
            printf("Device 0x%02X not found\n", address);
        }
    } else if (strcmp(argv[0], "current") == 0) {
        if (ctx->current_device) {
            printf("Current device: 0x%02X\n", ctx->current_device->address);
        } else {
            printf("No device selected\n");
        }
    } else if (strcmp(argv[0], "scan") == 0) {
        printf("Scanning for devices...\n");
        pmbus_result_t result = pmbus_discovery_scan(&ctx->discovery);
        if (result == PMBUS_SUCCESS) {
            result = pmbus_discovery_get_devices(&ctx->discovery, &ctx->devices, &ctx->num_devices);
            if (result == PMBUS_SUCCESS) {
                printf("Found %zu device(s)\n", ctx->num_devices);
                print_device_list(ctx);
            }
        } else {
            printf("Scan failed: %s\n", pmbus_result_to_string(result));
        }
    } else if (strcmp(argv[0], "stats") == 0) {
        show_transport_stats(ctx);
    } else if (strcmp(argv[0], "read") == 0) {
        if (argc < 2) {
            printf("Usage: read <command>\n");
            return;
        }
        execute_read_command(ctx, argv[1]);
    } else if (strcmp(argv[0], "write") == 0) {
        if (argc < 3) {
            printf("Usage: write <command> <data>\n");
            return;
        }
        execute_write_command(ctx, argv[1], argv[2]);
    } else if (strcmp(argv[0], "telemetry") == 0) {
        show_telemetry(ctx);
    } else if (strcmp(argv[0], "clear_faults") == 0) {
        if (!ctx->current_device) {
            printf("Error: No device selected.\n");
            return;
        }
        pmbus_result_t result = pmbus_clear_faults(ctx->current_device);
        if (result == PMBUS_SUCCESS) {
            printf("Faults cleared successfully\n");
        } else {
            printf("Failed to clear faults: %s\n", pmbus_result_to_string(result));
        }
    } else if (strcmp(argv[0], "quit") == 0 || strcmp(argv[0], "exit") == 0) {
        printf("Goodbye!\n");
        exit(0);
    } else {
        printf("Unknown command: %s\n", argv[0]);
        printf("Type 'help' for available commands.\n");
    }
}

static void print_usage(const char *prog_name)
{
    printf("Usage: %s [-b bus_number] [-h]\n", prog_name);
    printf("Options:\n");
    printf("  -b bus_number  I2C bus number (default: 1)\n");
    printf("  -h             Show this help message\n");
}

int main(int argc, char *argv[])
{
    int bus_number = 1;
    int opt;
    
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
    
    shell_context_t ctx = {0};
    
    // Initialize transport
    pmbus_transport_config_t transport_config = {
        .bus_number = bus_number,
        .enable_pec = true,
        .retry_count = 3,
        .retry_delay_ms = 10,
        .timeout_ms = 1000
    };
    
    pmbus_result_t result = pmbus_transport_init(&ctx.transport, &transport_config);
    if (result != PMBUS_SUCCESS) {
        fprintf(stderr, "Failed to initialize PMBus transport: %s\n", 
                pmbus_result_to_string(result));
        return 1;
    }
    
    // Initialize discovery
    pmbus_discovery_config_t discovery_config = {
        .scan_reserved_addresses = false,
        .verify_device_id = true,
        .enable_capability_detection = true,
        .discovery_timeout_ms = 5000
    };
    
    result = pmbus_discovery_init(&ctx.discovery, &ctx.transport, &discovery_config);
    if (result != PMBUS_SUCCESS) {
        fprintf(stderr, "Failed to initialize discovery engine: %s\n",
                pmbus_result_to_string(result));
        pmbus_transport_cleanup(&ctx.transport);
        return 1;
    }
    
    printf("PMBus Interactive Command Shell\n");
    printf("================================\n");
    printf("Bus: %d\n", bus_number);
    printf("Type 'help' for available commands, 'scan' to discover devices.\n\n");
    
    // Main command loop
    char line[MAX_COMMAND_LEN];
    while (1) {
        printf("pmbus> ");
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) {
            break; // EOF
        }
        
        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        // Skip empty lines
        if (strlen(line) == 0) continue;
        
        process_command(&ctx, line);
    }
    
    // Cleanup
    pmbus_discovery_cleanup(&ctx.discovery);
    pmbus_transport_cleanup(&ctx.transport);
    
    return 0;
}
