cmake_minimum_required(VERSION 3.12)

project(ConsolidatedPMBus VERSION 1.0.0 LANGUAGES C CXX)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set CXX standard for potential C++ components
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build configuration options
option(PMBUS_BUILD_EXAMPLES "Build example applications" ON)
option(PMBUS_BUILD_TESTS "Build unit tests" ON)
option(PMBUS_BUILD_SHARED_LIBS "Build shared libraries" ON)
option(PMBUS_ENABLE_LOGGING "Enable detailed logging" ON)
option(PMBUS_ENABLE_STATISTICS "Enable performance statistics" ON)
option(PMBUS_ENABLE_CACHING "Enable conversion caching" ON)

# Platform detection
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PMBUS_PLATFORM_LINUX ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(PMBUS_PLATFORM_WINDOWS ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(PMBUS_PLATFORM_MACOS ON)
else()
    set(PMBUS_PLATFORM_GENERIC ON)
endif()

# Compiler-specific options
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0 -DDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -DNDEBUG")
endif()

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Generate config header
configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/pmbus_config.h.in"
    "${PROJECT_BINARY_DIR}/include/pmbus/pmbus_config.h"
    @ONLY
)
include_directories(${PROJECT_BINARY_DIR}/include)

# Source files
set(PMBUS_TRANSPORT_SOURCES
    src/transport/transport.c
)

set(PMBUS_PROTOCOL_SOURCES
    src/protocol/protocol.c
    src/protocol/data_conversion.c
)

set(PMBUS_DEVICES_SOURCES
    src/devices/discovery.c
)

set(PMBUS_CORE_SOURCES
    ${PMBUS_TRANSPORT_SOURCES}
    ${PMBUS_PROTOCOL_SOURCES}
    ${PMBUS_DEVICES_SOURCES}
)

# Platform-specific sources
if(PMBUS_PLATFORM_LINUX)
    list(APPEND PMBUS_CORE_SOURCES src/platform/linux_i2c.c)
    list(APPEND PMBUS_PLATFORM_LIBS rt pthread)
elseif(PMBUS_PLATFORM_WINDOWS)
    list(APPEND PMBUS_CORE_SOURCES src/platform/windows_i2c.c)
    list(APPEND PMBUS_PLATFORM_LIBS kernel32 user32)
else()
    list(APPEND PMBUS_CORE_SOURCES src/platform/generic_i2c.c)
endif()

# Create main library
if(PMBUS_BUILD_SHARED_LIBS)
    add_library(pmbus SHARED ${PMBUS_CORE_SOURCES})
    set_target_properties(pmbus PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION 1)
else()
    add_library(pmbus STATIC ${PMBUS_CORE_SOURCES})
endif()

# Link platform-specific libraries
if(PMBUS_PLATFORM_LIBS)
    target_link_libraries(pmbus ${PMBUS_PLATFORM_LIBS})
endif()

# Math library (for some platforms)
find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
    target_link_libraries(pmbus ${MATH_LIBRARY})
endif()

# Set library properties
target_compile_definitions(pmbus PRIVATE
    $<$<BOOL:${PMBUS_ENABLE_LOGGING}>:PMBUS_ENABLE_LOGGING>
    $<$<BOOL:${PMBUS_ENABLE_STATISTICS}>:PMBUS_ENABLE_STATISTICS>
    $<$<BOOL:${PMBUS_ENABLE_CACHING}>:PMBUS_ENABLE_CACHING>
)

# Export symbols for shared library
if(PMBUS_BUILD_SHARED_LIBS)
    target_compile_definitions(pmbus PRIVATE PMBUS_BUILDING_DLL)
    if(PMBUS_PLATFORM_WINDOWS)
        target_compile_definitions(pmbus INTERFACE PMBUS_USING_DLL)
    endif()
endif()

# Install library and headers
install(TARGETS pmbus
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/pmbus
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

install(FILES "${PROJECT_BINARY_DIR}/include/pmbus/pmbus_config.h"
    DESTINATION include/pmbus
)

# Examples
if(PMBUS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Unit tests
if(PMBUS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Documentation
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in
                   ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    
    add_custom_target(docs
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen" VERBATIM
    )
endif()

# Package configuration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Generate pkg-config file
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/pmbus.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/pmbus.pc"
    @ONLY
)

# Add examples subdirectory
add_subdirectory(examples)

install(FILES "${PROJECT_BINARY_DIR}/pmbus.pc"
    DESTINATION lib/pkgconfig
)

# CPack configuration for packaging
set(CPACK_PACKAGE_NAME "consolidated-pmbus")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Consolidated PMBus Framework")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_CONTACT "pmbus-support@analogdevices.com")

if(PMBUS_PLATFORM_LINUX)
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.17)")
    set(CPACK_RPM_PACKAGE_REQUIRES "glibc >= 2.17")
elseif(PMBUS_PLATFORM_WINDOWS)
    set(CPACK_GENERATOR "NSIS;ZIP")
elseif(PMBUS_PLATFORM_MACOS)
    set(CPACK_GENERATOR "Bundle;TGZ")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)

# Print build configuration summary
message(STATUS "=== Consolidated PMBus Framework Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Shared Libraries: ${PMBUS_BUILD_SHARED_LIBS}")
message(STATUS "Build Examples: ${PMBUS_BUILD_EXAMPLES}")
message(STATUS "Build Tests: ${PMBUS_BUILD_TESTS}")
message(STATUS "Enable Logging: ${PMBUS_ENABLE_LOGGING}")
message(STATUS "Enable Statistics: ${PMBUS_ENABLE_STATISTICS}")
message(STATUS "Enable Caching: ${PMBUS_ENABLE_CACHING}")
message(STATUS "=================================================")
