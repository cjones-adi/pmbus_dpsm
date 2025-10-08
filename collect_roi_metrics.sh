#!/bin/bash

# PMBus Consolidation ROI Measurement Script
# This script collects baseline and ongoing metrics for the PMBus consolidation initiative

set -e

# Configuration
PMBUS_DPSM_DIR="/home/cj/pmbus_dpsm"
NO_OS_DIR="/home/cj/innersource/no-OS_upstream"
CONSOLIDATED_DIR="${PMBUS_DPSM_DIR}/consolidated_framework"
OUTPUT_DIR="${PMBUS_DPSM_DIR}/roi_metrics"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Create output directory
mkdir -p "${OUTPUT_DIR}"

echo "PMBus Consolidation ROI Metrics Collection"
echo "=========================================="
echo "Timestamp: ${TIMESTAMP}"
echo ""

# Function to count lines of code
count_loc() {
    local dir=$1
    local pattern=$2
    local label=$3
    
    echo "Analyzing ${label}..."
    local files=$(find "${dir}" -name "*.cpp" -o -name "*.h" -o -name "*.c" -type f)
    if [ -n "$files" ]; then
        echo "$files" | xargs wc -l | tail -1 | awk '{print $1}' > "${OUTPUT_DIR}/${label}_loc_${TIMESTAMP}.txt"
    else
        echo "0" > "${OUTPUT_DIR}/${label}_loc_${TIMESTAMP}.txt"
    fi
    local count=$(cat "${OUTPUT_DIR}/${label}_loc_${TIMESTAMP}.txt")
    echo "  Lines of Code: ${count}"
    return 0
}

# Function to count files
count_files() {
    local dir=$1
    local pattern=$2
    local label=$3
    
    echo "Counting files for ${label}..."
    local count=$(find "${dir}" -name "*.cpp" -o -name "*.h" -o -name "*.c" -type f | wc -l)
    echo "$count" > "${OUTPUT_DIR}/${label}_files_${TIMESTAMP}.txt"
    echo "  File Count: ${count}"
    return 0
}

# Function to analyze code complexity
analyze_complexity() {
    local dir=$1
    local label=$2
    
    echo "Analyzing complexity for ${label}..."
    
    # Check if lizard is available
    if command -v lizard &> /dev/null; then
        lizard "${dir}" -x "*/build/*" -x "*/test/*" > "${OUTPUT_DIR}/${label}_complexity_${TIMESTAMP}.txt" 2>&1 || echo "Lizard analysis failed for ${label}"
    else
        echo "  Lizard not available, skipping complexity analysis"
        echo "SKIPPED: lizard not installed" > "${OUTPUT_DIR}/${label}_complexity_${TIMESTAMP}.txt"
    fi
}

# Function to check for code duplication
analyze_duplication() {
    local dir1=$1
    local dir2=$2
    local label=$3
    
    echo "Analyzing code duplication between ${label}..."
    
    # Simple duplication check using grep for common patterns
    grep -r "PMBus\|pmbus" "${dir1}" --include="*.cpp" --include="*.h" --include="*.c" | \
        awk -F: '{print $2}' | sort | uniq -c | sort -nr > "${OUTPUT_DIR}/${label}_patterns_${TIMESTAMP}.txt" 2>&1 || echo "Pattern analysis failed"
}

# Function to generate API inventory
generate_api_inventory() {
    local dir=$1
    local label=$2
    
    echo "Generating API inventory for ${label}..."
    
    # Extract function declarations
    grep -r "^[a-zA-Z_][a-zA-Z0-9_]*\s*(" "${dir}" --include="*.h" | \
        grep -v "^\s*//" | grep -v "^\s*\*" > "${OUTPUT_DIR}/${label}_api_inventory_${TIMESTAMP}.txt" 2>&1 || echo "API inventory failed"
}

# Function to analyze PMBus commands
analyze_pmbus_commands() {
    local dir=$1
    local label=$2
    
    echo "Analyzing PMBus commands for ${label}..."
    
    # Extract PMBus command definitions
    grep -r "#define.*0x[0-9A-Fa-f]" "${dir}" --include="*.h" --include="*.cpp" --include="*.c" | \
        grep -i "pmbus\|read_\|write_\|status_\|vout\|iout\|vin\|iin" > "${OUTPUT_DIR}/${label}_pmbus_commands_${TIMESTAMP}.txt" 2>&1 || echo "PMBus command analysis failed"
}

# Function to calculate metrics summary
calculate_summary() {
    echo "Calculating summary metrics..."
    
    local dpsm_loc=$(cat "${OUTPUT_DIR}/pmbus_dpsm_loc_${TIMESTAMP}.txt" 2>/dev/null || echo "0")
    local no_os_loc=$(cat "${OUTPUT_DIR}/no_os_power_loc_${TIMESTAMP}.txt" 2>/dev/null || echo "0")
    local consolidated_loc=$(cat "${OUTPUT_DIR}/consolidated_framework_loc_${TIMESTAMP}.txt" 2>/dev/null || echo "0")
    
    local total_original=$((dpsm_loc + no_os_loc))
    
    # Calculate reduction percentage safely
    local reduction_pct=0
    if [ "$total_original" -gt 0 ]; then
        reduction_pct=$(( (total_original - consolidated_loc) * 100 / total_original ))
    fi
    
    cat > "${OUTPUT_DIR}/summary_${TIMESTAMP}.txt" << EOF
PMBus Consolidation Metrics Summary
Generated: ${TIMESTAMP}

Original Implementation:
- pmbus_dpsm LOC: ${dpsm_loc}
- no-OS power LOC: ${no_os_loc}
- Total Original LOC: ${total_original}

Consolidated Framework:
- Consolidated LOC: ${consolidated_loc}

Potential Metrics:
- Code Reduction Potential: $((total_original - consolidated_loc)) lines
- Reduction Percentage: ${reduction_pct}%

Note: These are preliminary metrics. Actual consolidation impact will vary based on implementation approach.
EOF

    echo "Summary written to: ${OUTPUT_DIR}/summary_${TIMESTAMP}.txt"
}

# Main analysis execution
main() {
    echo "Starting ROI metrics collection..."
    
    # Analyze pmbus_dpsm
    if [ -d "${PMBUS_DPSM_DIR}/src" ]; then
        count_loc "${PMBUS_DPSM_DIR}/src" "*.cpp -o -name *.h" "pmbus_dpsm"
        count_files "${PMBUS_DPSM_DIR}/src" "*.cpp -o -name *.h" "pmbus_dpsm"
        analyze_complexity "${PMBUS_DPSM_DIR}/src" "pmbus_dpsm"
        generate_api_inventory "${PMBUS_DPSM_DIR}/src" "pmbus_dpsm"
        analyze_pmbus_commands "${PMBUS_DPSM_DIR}/src" "pmbus_dpsm"
    else
        echo "Warning: pmbus_dpsm src directory not found"
    fi
    
    echo ""
    
    # Analyze no-OS power drivers
    if [ -d "${NO_OS_DIR}/drivers/power" ]; then
        count_loc "${NO_OS_DIR}/drivers/power" "*.c -o -name *.h" "no_os_power"
        count_files "${NO_OS_DIR}/drivers/power" "*.c -o -name *.h" "no_os_power"
        analyze_complexity "${NO_OS_DIR}/drivers/power" "no_os_power"
        generate_api_inventory "${NO_OS_DIR}/drivers/power" "no_os_power"
        analyze_pmbus_commands "${NO_OS_DIR}/drivers/power" "no_os_power"
    else
        echo "Warning: no-OS power directory not found"
    fi
    
    echo ""
    
    # Analyze consolidated framework (if it exists)
    if [ -d "${CONSOLIDATED_DIR}" ]; then
        count_loc "${CONSOLIDATED_DIR}" "*.c -o -name *.h" "consolidated_framework"
        count_files "${CONSOLIDATED_DIR}" "*.c -o -name *.h" "consolidated_framework"
        analyze_complexity "${CONSOLIDATED_DIR}" "consolidated_framework"
        generate_api_inventory "${CONSOLIDATED_DIR}" "consolidated_framework"
        analyze_pmbus_commands "${CONSOLIDATED_DIR}" "consolidated_framework"
    else
        echo "Warning: consolidated framework directory not found"
    fi
    
    echo ""
    
    # Generate summary
    calculate_summary
    
    echo ""
    echo "Metrics collection complete!"
    echo "Results stored in: ${OUTPUT_DIR}"
    echo ""
    echo "Key files generated:"
    ls -la "${OUTPUT_DIR}"/*_${TIMESTAMP}.txt | awk '{print "  " $9}'
}

# Check dependencies
check_dependencies() {
    echo "Checking dependencies..."
    
    # Check for required tools
    local missing_tools=()
    
    if ! command -v find &> /dev/null; then
        missing_tools+=("find")
    fi
    
    if ! command -v wc &> /dev/null; then
        missing_tools+=("wc")
    fi
    
    if ! command -v grep &> /dev/null; then
        missing_tools+=("grep")
    fi
    
    if [ ${#missing_tools[@]} -ne 0 ]; then
        echo "Error: Missing required tools: ${missing_tools[*]}"
        exit 1
    fi
    
    # Optional tools
    if ! command -v lizard &> /dev/null; then
        echo "Warning: 'lizard' not found. Install with: pip install lizard"
        echo "         Complexity analysis will be skipped."
    fi
    
    echo "Dependency check complete."
    echo ""
}

# Help function
show_help() {
    cat << EOF
PMBus Consolidation ROI Measurement Script

Usage: $0 [OPTIONS]

OPTIONS:
    -h, --help      Show this help message
    -o, --output    Specify output directory (default: ${OUTPUT_DIR})

DESCRIPTION:
    This script collects baseline metrics for the PMBus consolidation initiative,
    including lines of code, file counts, complexity analysis, and API inventories.

OUTPUTS:
    All metrics are saved to timestamped files in the output directory.
    A summary file provides consolidated results.

REQUIREMENTS:
    - find, wc, grep (standard Unix tools)
    - lizard (optional, for complexity analysis): pip install lizard

EXAMPLES:
    $0                          # Run with default settings
    $0 -o /custom/output/path   # Use custom output directory

EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -o|--output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use -h or --help for usage information."
            exit 1
            ;;
    esac
done

# Run the analysis
check_dependencies
main
