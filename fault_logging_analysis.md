# PMBus DPSM - Fault Logging Architecture Implementation Analysis

## Overview
The PMBus DPSM project implements a sophisticated **dual-strategy fault logging system** that handles the fundamental architectural differences between device families. This analysis explores the deep implementation details of how the system manages fault logging across 27 different devices using two distinct approaches.

## UML Architecture Diagrams
📊 **Complete UML visualization available in:** [`fault_logging_uml.puml`](./fault_logging_uml.puml)

The UML diagrams illustrate:
- **Strategy Pattern implementation** with base `LT_FaultLog` class
- **EEPROM vs Command access strategies** for different device families
- **Concrete device implementations** (LTC3880 Controller vs LTC2974 Manager)
- **Data structure relationships** and format definitions
- **Hardware abstraction layers** and SMBus interfacesBus DPSM - Fault Logging Architecture Implementation Analysis

## Overview
The PMBus DPSM project implements a sophisticated dual-strategy fault logging system that handles the fundamental architectural differences between device families. This analysis explores the deep implementation details of how the system manages fault logging across 27 different devices using two distinct approaches.

---

## Strategy Pattern Implementation

### **Base Abstraction: LT_FaultLog**

The fault logging system is built on a classic Strategy pattern with `LT_FaultLog` as the abstract base class defining the common interface.

```cpp
// Base fault logging interface
class LT_FaultLog
{
  protected:
    LT_PMBus *pmbus_;

  public:
    LT_FaultLog(LT_PMBus *pmbus);
    virtual ~LT_FaultLog() {}
    
    // Pure virtual interface - Strategy Pattern
    virtual bool hasFaultLog(uint8_t address) = 0;
    virtual void read(uint8_t address) = 0;
    virtual char *getFaultLog() = 0;
    virtual void print() = 0;
    virtual void clearFaultLog(uint8_t address) = 0;
    virtual void enableFaultLog(uint8_t address) = 0;
    virtual void disableFaultLog(uint8_t address) = 0;
    virtual void storeFaultLog(uint8_t address) = 0;
    virtual void release() = 0;
    virtual void dumpBinary() = 0;
};
```

---

## Strategy 1: EEPROM-Based Fault Logging (LT_EEDataFaultLog)

### **Target Devices: Controller Family**
- **LTC3800 Series**: LTC3880, LTC3882, LTC3883, LTC3884, LTC3886, LTC3887, LTC3888, LTC3889
- **LTC7800 Series**: LTC7880  
- **LTM4600 Series**: LTM4664, LTM4675, LTM4676, LTM4677, LTM4678, LTM4680, LTM4686, LTM4700
- **Special Case**: LTC2978 (Manager device but uses EEPROM strategy)

### **Architecture Philosophy**
Controller devices are **active regulators** with complex switching behavior. They generate detailed fault information including:
- **Switching events** and timing violations
- **Current limit** and overcurrent conditions  
- **Temperature** and thermal shutdown events
- **Voltage regulation** accuracy and transients

This detailed information requires **high-bandwidth fault storage**, making direct EEPROM access the optimal strategy.

---

## Strategy 2: Command-Based Fault Logging (LT_CommandPlusFaultLog)  

### **Target Devices: Manager Family**
- **LTC2900 Series**: LTC2972, LTC2974, LTC2975, LTC2977, LTC2979, LTC2980, LTM2987

### **Architecture Philosophy**  
Manager devices are **passive supervisors** monitoring multiple rails. They generate:
- **Voltage threshold** violations per channel
- **Sequencing** and timing faults  
- **Temperature** monitoring across rails
- **System-level** coordination events

This structured, lower-volume data fits well with **command-based access** through standard PMBus registers.

---

## Implementation Deep-Dive Analysis

### **Strategy 1: EEPROM-Based Implementation (LT_EEDataFaultLog)**

#### **Low-Level Hardware Interface**
The EEPROM strategy uses direct memory access through a sophisticated hardware protocol:

```cpp
void getNvmBlock(uint8_t address, uint16_t offset, uint16_t numWords, bool crc, uint8_t *data) 
{
  // Initialize EEPROM access sequence
  pmbus_->smbus()->writeByte(address, 0xBD, 0x2B);   // Set EEPROM mode
  pmbus_->smbus()->writeByte(address, 0xBD, 0x91);   // Enable read access  
  pmbus_->smbus()->writeByte(address, 0xBD, 0xE4);   // Position to start
  
  // Sequential word-based reading with endianness handling
  for (uint16_t i = 0; i < numWords; i++) {
    uint16_t w = pmbus_->smbus()->readWord(address, 0xBF);
    
    // Critical: Handle little-endian conversion
    data[pos] = 0xFF & w;        // Low byte
    data[pos+1] = 0xFF & (w >> 8);   // High byte
  }
}
```

#### **Controller Fault Data Structure (LTC3880 Example)**
Controller devices capture **real-time telemetry snapshots** during fault conditions:

```cpp
struct FaultLogLtc3880 {
  struct FaultLogPreambleLtc3880 {
    uint8_t position_fault;                    // Fault sequence position
    FaultLogTimeStamp shared_time;            // High-resolution timestamp (200μs)
    
    // Peak value tracking across fault event
    FaultLogTelemetrySummaryLtc3880 peaks {
      Lin16WordReverse mfr_vout_peak_p0;      // Peak output voltage page 0
      Lin16WordReverse mfr_vout_peak_p1;      // Peak output voltage page 1
      Lin5_11WordReverse mfr_iout_peak_p0;    // Peak output current page 0
      Lin5_11WordReverse mfr_iout_peak_p1;    // Peak output current page 1
      Lin5_11WordReverse mfr_vin_peak;        // Peak input voltage
      Lin5_11WordReverse read_temperature_1_p0; // Instantaneous temp page 0
      Lin5_11WordReverse read_temperature_1_p1; // Instantaneous temp page 1
      Lin5_11WordReverse mfr_temperature_1_peak_p0; // Peak temp page 0
      Lin5_11WordReverse mfr_temperature_1_peak_p1; // Peak temp page 1
    };
  } preamble;
  
  // Six sequential snapshots of system state
  FaultLogReadLoopLtc3880 fault_log_loop[6] {
    Lin16WordReverse read_vout_p0, read_vout_p1;     // Output voltages
    Lin5_11WordReverse read_iout_p0, read_iout_p1;   // Output currents  
    Lin5_11WordReverse read_vin, read_iin;           // Input voltage/current
    RawByte status_vout_p0, status_vout_p1;         // Output status flags
    RawWordReverse status_word_p0, status_word_p1;  // Complete status words
    RawByte status_mfr_specificP0, status_mfr_specificP1; // Manufacturer flags
  };
};
```

**Key Insight**: Controller devices need **6 sequential snapshots** to capture the fault transient behavior, requiring ~500 bytes of high-speed EEPROM storage.

---

### **Strategy 2: Command-Based Implementation (LT_CommandPlusFaultLog)**

#### **Register-Based Access Protocol**
Manager devices use standardized PMBus command sequences:

```cpp
void getNvmBlock(uint8_t address, uint16_t offset, uint16_t numWords, 
                 uint8_t command, uint8_t *data) 
{
  // Initiate command-plus sequence
  pmbus_->smbus()->writeWord(address, command, 0x00EE);
  
  // Read size header (consumed but not stored)
  uint16_t w = pmbus_->smbus()->readWord(address, command + 1);
  
  // Skip to desired offset
  for (uint16_t i = 0; i < offset; i++) {
    pmbus_->smbus()->readWord(address, command + 1);
  }
  
  // Read fault log data with consistent endianness
  for (uint16_t i = 0; i < numWords; i++) {
    w = pmbus_->smbus()->readWord(address, command + 1);
    data[pos] = 0xFF & w;        // Low byte
    data[pos+1] = 0xFF & (w >> 8);   // High byte  
  }
}
```

#### **Manager Fault Data Structure (LTC2974 Example)**
Manager devices focus on **multi-channel supervision** and **sequencing events**:

```cpp
struct FaultLogLtc2974 {
  struct FaultLogPreambleLtc2974 {
    uint8_t position_last;                     // Last fault position
    FaultLogTimeStamp shared_time;            // System timestamp
    
    // Peak tracking for ALL channels (4-channel manager)
    FaultLogPeaksLtc2974 peaks {
      Peak16Words vout0_peaks, vout1_peaks, vout2_peaks, vout3_peaks;
      Peak5_11Words iout0_peaks, iout1_peaks, iout2_peaks, iout3_peaks;  
      Peak5_11Words temp0_peaks, temp1_peaks, temp2_peaks, temp3_peaks;
      Peak5_11Words vin_peaks;  // Single input for all channels
    };
    
    // Channel status at fault time
    FaultLogReadStatusLtc2974 fault_log_status {
      ChanStatus chan_status0, chan_status1, chan_status2, chan_status3;
    };
  } preamble;
  
  // Variable-length telemetry buffer (184 bytes)
  uint8_t telemetryData[184];
  FaultLogReadLoopLtc2974 *loops;  // Overlaid on telemetry data
  
  // Circular buffer management
  uint8_t firstValidByte;  // Start of valid data
  uint8_t lastValidByte;   // End of valid data
  
  // Helper for circular buffer validation
  bool isValidData(void *pos, uint8_t size = 2);
};
```

**Key Insight**: Manager devices use a **circular buffer** approach with variable-length records, optimized for monitoring multiple channels with lower data rates.

---

## Data Format Analysis

### **Linear Data Encoding**
PMBus devices use two primary numeric formats:

#### **LINEAR16 Format (Voltages)**
- **16-bit signed integer** with implied scaling
- Used for: `read_vout`, `mfr_vout_peak`
- Endianness: **Reverse byte order** in structures

#### **LINEAR5_11 Format (Currents, Temperatures)**  
- **5-bit signed exponent + 11-bit mantissa**
- Used for: `read_iout`, `read_temp`, `mfr_iout_peak`
- Range: ±1024 × 2^(±15) for wide dynamic range

### **Endianness Handling**
The implementation carefully manages byte ordering:

```cpp
struct Lin16WordReverse {
  uint8_t hi_byte;  // MSB first for network byte order
  uint8_t lo_byte;  // LSB second
};

struct Lin5_11WordReverse {
  uint8_t hi_byte;  // Exponent + upper mantissa bits
  uint8_t lo_byte;  // Lower mantissa bits  
};
```

---

## Memory Management Strategy

### **EEPROM Strategy Characteristics**
- **Direct Hardware Access**: Register 0xBD controls EEPROM state machine
- **Block Operations**: Reads in 16-bit word chunks for efficiency  
- **CRC Validation**: Optional CRC checking every 16 words
- **Non-Volatile**: Fault data persists across power cycles

### **Command Strategy Characteristics**
- **Protocol Compliance**: Uses standard PMBus command extensions
- **Structured Access**: Size headers and offset management
- **Circular Buffering**: Automatic overwrite of oldest data
- **Real-Time**: Lower latency for live monitoring

---

## Architectural Benefits

### **Strategy Pattern Advantages**
1. **Device-Optimal Access**: Each family uses its most efficient method
2. **Uniform Interface**: Applications see identical `LT_FaultLog` API
3. **Extensibility**: New devices add concrete strategies without API changes
4. **Hardware Abstraction**: Applications unaware of underlying differences

### **Performance Characteristics**
- **EEPROM Strategy**: Higher capacity (KB range), persistence, lower speed
- **Command Strategy**: Lower capacity (hundreds of bytes), real-time, higher speed

This dual-strategy architecture perfectly demonstrates the **Strategy Pattern** solving a real hardware constraint problem while maintaining clean abstractions.

---

## Complete Usage Flow Analysis

### **LTC3880 Controller Example - EEPROM Strategy**

#### **1. Fault Log Reading Process**
```cpp
void LT_3880FaultLog::read(uint8_t address) {
  // Allocate memory for raw EEPROM data (54 words × 2 bytes = 108 bytes)
  uint8_t *data = (uint8_t *) malloc(sizeof(uint8_t) * 54 * 2);
  
  // Read from EEPROM offset 384 (192*2), 54 words, with CRC enabled
  getNvmBlock(address, 192*2, 54, true, data);
  
  // Cast raw data to structured fault log
  faultLog3880 = (struct FaultLogLtc3880 *) (data);
}
```

#### **2. Data Interpretation & Display**
```cpp
void LT_3880FaultLog::printLoop(uint8_t index) {
  // Convert LINEAR5_11 input voltage to float
  float vin = math_.lin11_to_float(
    getLin5_11WordReverseVal(faultLog3880->fault_log_loop[index].read_vin)
  );
  
  // Convert LINEAR16 output voltage to float (0x14 = exponent)
  float vout_p0 = math_.lin16_to_float(
    getLin16WordReverseVal(faultLog3880->fault_log_loop[index].read_vout_p0), 
    0x14
  );
  
  // Display status information
  printf("STATUS_WORD: 0x%04x\n", 
    getRawWordReverseVal(faultLog3880->fault_log_loop[index].status_word_p0)
  );
}
```

**Critical Insight**: The LTC3880 stores **4 complete telemetry snapshots** taken at 200μs intervals during the fault event, providing precise transient analysis capability.

---

## Advanced Implementation Features

### **1. Memory Management Patterns**

#### **RAII-Style Resource Management**
```cpp
class LT_3880FaultLog : public LT_EEDataFaultLog {
  private:
    FaultLogLtc3880 *faultLog3880;  // Raw fault data
    char *buffer;                   // Formatted output buffer
    
  public:
    void read(uint8_t address) {
      // Allocate and read
      faultLog3880 = (FaultLogLtc3880 *) malloc(108);
      getNvmBlock(address, 192*2, 54, true, (uint8_t*)faultLog3880);
    }
    
    void release() {
      free(faultLog3880);
      faultLog3880 = NULL;  // Prevent double-free
    }
};
```

### **2. Circular Buffer Management (Manager Devices)**

#### **LTC2974 Dynamic Buffer Validation**
```cpp
struct FaultLogLtc2974 {
  uint8_t telemetryData[184];      // Fixed-size circular buffer
  uint8_t firstValidByte;          // Dynamic start marker
  uint8_t lastValidByte;           // Dynamic end marker
  
  // Validates if pointer references valid telemetry data
  bool isValidData(void *pos, uint8_t size = 2) {
    uint16_t offset = (uint8_t*)pos - telemetryData;
    return (offset >= firstValidByte && 
            offset + size <= lastValidByte);
  }
};
```

**Key Insight**: Manager devices use **dynamic circular buffering** where the valid data window shifts based on fault occurrence patterns, maximizing storage efficiency.

### **3. Endianness Conversion Utilities**

#### **Platform-Independent Data Access**
```cpp
class LT_FaultLog {
  protected:
    // Handle different data format conversions
    uint16_t getLin5_11WordReverseVal(Lin5_11WordReverse value) {
      return (value.hi_byte << 8) | value.lo_byte;
    }
    
    uint16_t getLin16WordReverseVal(Lin16WordReverse value) {
      return (value.hi_byte << 8) | value.lo_byte;  
    }
    
    uint8_t getRawByteVal(RawByte value) {
      return value.uint8_tValue;
    }
};
```

---

## Architecture Pattern Summary

### **Strategy Pattern Implementation Benefits**

1. **Hardware Abstraction**: Applications interact with unified `LT_FaultLog` interface
2. **Optimal Performance**: Each device family uses its most efficient access method  
3. **Code Reuse**: Common data structures and utilities shared via inheritance
4. **Extensibility**: New device families add strategies without breaking existing code

### **Design Trade-offs Analysis**

| Aspect | EEPROM Strategy | Command Strategy |
|--------|----------------|------------------|
| **Data Volume** | High (100+ bytes) | Moderate (50-100 bytes) |
| **Access Speed** | Slower (EEPROM) | Faster (register-based) |
| **Persistence** | Non-volatile | Volatile |
| **Complexity** | Hardware state machine | Standard PMBus commands |
| **Best For** | Detailed transient analysis | Real-time monitoring |

---

## Real-World Usage Patterns

### **Application Integration Example**
```cpp
// Unified fault log handling regardless of device type
void analyzeFaultLog(LT_PMBus *pmbus, uint8_t address, uint16_t deviceId) {
  LT_FaultLog *faultLogger;
  
  // Factory pattern selects appropriate strategy
  if (isControllerDevice(deviceId)) {
    faultLogger = new LT_3880FaultLog(pmbus);  // EEPROM strategy
  } else {
    faultLogger = new LT_2974FaultLog(pmbus);  // Command strategy  
  }
  
  // Uniform interface regardless of underlying implementation
  if (faultLogger->hasFaultLog(address)) {
    faultLogger->read(address);
    faultLogger->print();        // Device-specific formatting
    faultLogger->dumpBinary();   // Raw data export
    faultLogger->release();      // Clean resource management
  }
  
  delete faultLogger;
}
```

This implementation demonstrates how the **Strategy Pattern** elegantly solves the fundamental architectural difference between device families while providing a clean, unified interface for applications.

---

## Key Architectural Lessons

1. **Hardware Constraints Drive Architecture**: The fundamental difference in fault data storage mechanisms (EEPROM vs. registers) required distinct access strategies

2. **Strategy Pattern for Hardware Abstraction**: Classic GoF pattern applied to hardware interface design, enabling optimal device-specific implementations behind uniform APIs

3. **Data Structure as Documentation**: Complex `#pragma pack(push, 1)` structures serve as both data layout specification and hardware interface documentation

4. **Resource Management Patterns**: Careful memory management with RAII-style allocation/deallocation prevents resource leaks in embedded environments

5. **Endianness Considerations**: Explicit handling of byte ordering ensures cross-platform compatibility and correct data interpretation
