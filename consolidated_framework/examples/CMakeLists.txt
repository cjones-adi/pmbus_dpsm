# Examples CMakeLists.txt

# Example executables
set(EXAMPLES
    basic_discovery
    telemetry_monitor
    command_shell
)

# Build each example
foreach(EXAMPLE ${EXAMPLES})
    add_executable(${EXAMPLE} ${EXAMPLE}.c)
    target_link_libraries(${EXAMPLE} pmbus)
    target_include_directories(${EXAMPLE} PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/include
    )
    
    # Set output directory
    set_target_properties(${EXAMPLE} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
    )
    
    # Install examples
    install(TARGETS ${EXAMPLE}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/pmbus-examples
        COMPONENT examples
    )
endforeach()

# Install example source code and documentation
install(FILES 
    basic_discovery.c
    telemetry_monitor.c
    command_shell.c
    README.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/examples
    COMPONENT examples
)
