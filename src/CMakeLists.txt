# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fault Log Sources (organized alphabetically)
set(FAULT_LOG_SOURCES
    LT_2972FaultLog.cpp
    LT_2974FaultLog.cpp
    LT_2975FaultLog.cpp
    LT_2977FaultLog.cpp
    LT_2978FaultLog.cpp
    LT_3880FaultLog.cpp
    LT_3882FaultLog.cpp
    LT_3883FaultLog.cpp
    LT_3884FaultLog.cpp
    LT_3886FaultLog.cpp
    LT_3887FaultLog.cpp
    LT_3888FaultLog.cpp
    LT_3889FaultLog.cpp
    LT_7880FaultLog.cpp
    LT_FaultLog.cpp)

# PMBus Device Sources (organized alphabetically)
set(PMBUS_DEVICE_SOURCES
    LT_PMBusDeviceLTC2972.cpp
    LT_PMBusDeviceLTC2974.cpp
    LT_PMBusDeviceLTC2975.cpp
    LT_PMBusDeviceLTC2977.cpp
    LT_PMBusDeviceLTC2978.cpp
    LT_PMBusDeviceLTC2979.cpp
    LT_PMBusDeviceLTC2980.cpp
    LT_PMBusDeviceLTC3880.cpp
    LT_PMBusDeviceLTC3882.cpp
    LT_PMBusDeviceLTC3883.cpp
    LT_PMBusDeviceLTC3884.cpp
    LT_PMBusDeviceLTC3886.cpp
    LT_PMBusDeviceLTC3887.cpp
    LT_PMBusDeviceLTC3888.cpp
    LT_PMBusDeviceLTC3889.cpp
    LT_PMBusDeviceLTC7880.cpp
    LT_PMBusDeviceLTM2987.cpp
    LT_PMBusDeviceLTM4664.cpp
    LT_PMBusDeviceLTM4675.cpp
    LT_PMBusDeviceLTM4676.cpp
    LT_PMBusDeviceLTM4677.cpp
    LT_PMBusDeviceLTM4678.cpp
    LT_PMBusDeviceLTM4680.cpp
    LT_PMBusDeviceLTM4686.cpp
    LT_PMBusDeviceLTM4700.cpp)

# SMBus Sources
set(SMBUS_SOURCES
    LT_SMBus.cpp
    LT_SMBusBase.cpp
    LT_SMBusGroup.cpp
    LT_SMBusNoPec.cpp
    LT_SMBusPec.cpp)

# Core PMBus Sources
set(PMBUS_CORE_SOURCES
    LT_PMBus.cpp
    LT_PMBusDetect.cpp
    LT_PMBusDevice.cpp
    LT_PMBusMath.cpp
    LT_PMBusRail.cpp
    LT_PMBusSpeedTest.cpp)

# Utility Sources
set(UTIL_SOURCES
    hex_file_parser.cpp
    httoi.cpp
    main_record_processor.cpp
    nvm_data_helpers.cpp)

# Other Core Sources
set(OTHER_SOURCES
    LT_Exception.cpp
    LT_Nvm.cpp)

# Create the executable
add_executable(lt-pmbusapp 
    LT_PMBusApp.cpp
    ${FAULT_LOG_SOURCES}
    ${PMBUS_DEVICE_SOURCES}
    ${SMBUS_SOURCES}
    ${PMBUS_CORE_SOURCES}
    ${UTIL_SOURCES}
    ${OTHER_SOURCES})

# Modern CMake: Use target-specific properties instead of global settings
target_include_directories(lt-pmbusapp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lt-pmbusapp PRIVATE i2c)
