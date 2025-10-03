# PMBus DPSM - High-Level Architecture Analysis

## Overview
This document provides a comprehensive analysis of the PMBus DPSM project's high-level architecture, focusing on the structural design patterns, abstraction layers, and component relationships.

## Architectural Design Pattern: **Layered Architecture with Strategy Pattern**

The PMBus DPSM project follows a **4-layer architecture** with clear separation of concerns and dependency inversion principles.

```
┌─────────────────────────────────────┐
│        APPLICATION LAYER            │  ← LT_PMBusApp
├─────────────────────────────────────┤
│      DEVICE ABSTRACTION LAYER       │  ← LT_PMBusDevice hierarchy
├─────────────────────────────────────┤
│        PROTOCOL LAYER               │  ← LT_PMBus + LT_PMBusDetect
├─────────────────────────────────────┤
│   HARDWARE ABSTRACTION LAYER        │  ← LT_SMBus hierarchy
└─────────────────────────────────────┘
```

---

## Layer 1: Hardware Abstraction Layer (HAL)

### **Core Component: LT_SMBus Hierarchy**

```cpp
                 LT_SMBus (Abstract Base)
                     ↑
              LT_SMBusBase (Common Implementation)
                     ↑
         ┌───────────┼───────────┐
         │           │           │
  LT_SMBusNoPec  LT_SMBusPec  LT_SMBusGroup
```

### **Design Pattern: Strategy Pattern**
- **LT_SMBus**: Abstract interface defining SMBus operations
- **LT_SMBusBase**: Template method pattern with common SMBus logic
- **LT_SMBusNoPec**: Strategy for communication without packet error checking
- **LT_SMBusPec**: Strategy for communication with packet error checking  
- **LT_SMBusGroup**: Strategy for group/broadcast operations

### **Key Responsibilities:**
- **Hardware I/O abstraction** via `/dev/i2c` interface
- **PEC calculation and validation**
- **Bus speed management and clock stretching**
- **Low-level SMBus transaction handling**
- **Alert Response Address (ARA) support**

### **Abstraction Benefits:**
- **Hardware independence** - supports different I2C adapters (Raspberry Pi, DLN-2)
- **PEC strategy selection** at runtime
- **Bus error handling** isolation
- **Transaction-level debugging** capability

---

## Layer 2: Protocol Layer

### **Core Components: LT_PMBus + Supporting Classes**

```cpp
           LT_PMBus (PMBus Protocol Engine)
               ↓ (uses)
    ┌──────────────────────────────┐
    │                              │
LT_PMBusDetect              LT_PMBusRail
(Auto Discovery)          (Multi-phase Rail)
```

### **LT_PMBus: Central Protocol Engine**
- **Dependency Injection**: Takes any LT_SMBus implementation
- **Device Type Resolution**: `deviceType()` via `MFR_SPECIAL_ID`
- **PMBus Command Translation**: Converts high-level operations to SMBus transactions
- **Data Format Conversion**: L11/L16 mathematical transformations
- **Device Family Logic**: PEC enable/disable per device family

### **LT_PMBusDetect: Discovery Service**
- **Bus Enumeration**: Scans all possible PMBus addresses
- **Device Instantiation**: Creates appropriate device objects via factory pattern
- **Capability Probing**: Determines device capabilities and configurations

### **LT_PMBusRail: Multi-Phase Abstraction**
- **Rail Grouping**: Manages multi-phase regulator configurations
- **Unified Interface**: Presents multiple pages/devices as single rail
- **Load Sharing Logic**: Coordinates parallel power delivery

### **Design Pattern: Factory + Facade**
- **Factory Pattern**: Device creation in LT_PMBusDetect
- **Facade Pattern**: LT_PMBus hides SMBus complexity
- **Template Method**: Common PMBus operations with device-specific variations

---

## Layer 3: Device Abstraction Layer

### **Core Hierarchy: LT_PMBusDevice Tree**

```cpp
                    LT_PMBusDevice (Abstract Base)
                           ↑
              ┌────────────┼────────────┐
              │                         │
   LT_PMBusDeviceManager      LT_PMBusDeviceController
      (8 Manager ICs)            (17 Controller ICs)
              │                         │
    ┌─────────┼─────────┐               ├─────────────────┐
    │         │         │               │                 │
LTC2972   LTC2978   LTM2987      [Basic Controllers]  [Advanced Controllers]
  ...       ...       ...          (9 devices)         (10 devices)
                                 ┌──────────────┐    ┌──────────────┐
                               LTC3880        LTC3883
                               LTC3882        LTC3884
                                ...            ...
```

### **Architectural Patterns:**

#### **Template Method Pattern**
- **Base class** defines common device operations
- **Derived classes** implement device-specific behaviors
- **Virtual methods** for capabilities, fault logging, rail management

#### **Strategy Pattern (Fault Logging)**
```cpp
LT_FaultLog (Base)
    ↑
┌───┼───┐
│       │
LT_EEDataFaultLog    LT_CommandPlusFaultLog
(EEPROM-based)       (Command-based)
    ↑                     ↑
[Controller ICs]      [Manager ICs]
```

### **Capability-Based Design**
- **Bit-masked capabilities**: `HAS_VOUT | HAS_VIN | HAS_IOUT | HAS_PIN`
- **Runtime capability checking**: `hasCapability(uint32_t)`
- **Feature polymorphism**: Same interface, different implementations

### **Device Specialization Patterns:**

#### **Manager Pattern** (LT_PMBusDeviceManager)
- **Multi-rail supervision**: 4-8 independent channels  
- **Monitoring focus**: Voltage/temperature only
- **Independent pages**: Each channel managed separately
- **16-bit register interface**: Extended configuration space

#### **Controller Pattern** (LT_PMBusDeviceController)
- **Active regulation**: 1-2 high-current outputs
- **Full telemetry**: Voltage, current, power, efficiency
- **Rail-aware**: Multi-phase capability detection
- **8-bit register interface**: Compact configuration

---

## Layer 4: Application Layer

### **Core Component: LT_PMBusApp**

```cpp
LT_PMBusApp
    ↓ (orchestrates)
┌─────────────────────────────────────┐
│  Interactive Menu System            │
│  ┌─────────────────────────────────┐│
│  │ • Device Discovery & Enumeration││
│  │ • Telemetry Reading & Display   ││
│  │ • Power Control & Sequencing    ││  
│  │ • Fault Management & Logging    ││
│  │ • EEPROM Programming & Verify   ││
│  └─────────────────────────────────┘│
└─────────────────────────────────────┘
    ↓ (uses)
Command Line Interface + NVM Programming
```

### **Design Patterns:**

#### **Command Pattern**
- Menu options map to discrete commands
- Command-line arguments trigger specific operations
- Batch processing capability for automated testing

#### **Facade Pattern** 
- Simplifies complex multi-layer operations
- Hides device-specific implementation details
- Provides unified interface for different device families

### **Application Architecture Benefits:**
- **Separation of concerns**: Each layer has distinct responsibility
- **Testability**: Layers can be tested independently  
- **Maintainability**: Changes isolated to specific layers
- **Extensibility**: New devices/protocols added without core changes
- **Reusability**: Lower layers reusable in different applications

---

## Cross-Cutting Concerns

### **Memory Management**
- **RAII Pattern**: Constructor/destructor resource management
- **Smart deletion**: Proper cleanup in application layer
- **Memory debugging**: Optional `mtrace()` integration

### **Error Handling** 
- **Exception-safe design**: LT_Exception for error propagation
- **Graceful degradation**: Continue operation when possible
- **Error isolation**: Failures don't cascade across layers

### **Configuration Management**
- **Device capabilities**: Runtime capability discovery
- **Protocol variants**: PEC enable/disable per device family
- **Hardware adaptation**: Multiple I2C interface support

This layered architecture provides a **robust, maintainable, and extensible** foundation for PMBus communication, supporting both development/testing and production deployment scenarios.
