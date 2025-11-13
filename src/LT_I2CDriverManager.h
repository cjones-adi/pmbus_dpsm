/*
Copyright (c) 2025, Analog Devices Inc
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:
  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
  * Neither the name of the Analog Devices, Inc. nor the names of its
    contributors may be used to endorse or promote products derived from this
    software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL ANALOG DEVICES, INC. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef LT_I2C_DRIVER_MANAGER_H_
#define LT_I2C_DRIVER_MANAGER_H_

#include <string>
#include <vector>

/**
 * @brief Manages Linux kernel I2C driver binding/unbinding for safe EEPROM programming
 * 
 * This class provides functionality to temporarily unbind kernel drivers from I2C
 * devices to allow safe EEPROM programming without conflicts.
 */
class LT_I2CDriverManager {
public:
    /**
     * @brief Construct a new driver manager
     * @param i2c_device Path to I2C device (e.g., "/dev/i2c-0")
     */
    explicit LT_I2CDriverManager(const std::string& i2c_device);
    
    /**
     * @brief Destructor - automatically rebinds drivers if unbind() was called
     */
    ~LT_I2CDriverManager();
    
    /**
     * @brief Unbind all kernel drivers from devices on this I2C bus
     * @return true if successful, false otherwise
     */
    bool unbind();
    
    /**
     * @brief Rebind all previously unbound drivers
     * @return true if successful, false otherwise
     */
    bool rebind();
    
    /**
     * @brief Check if any drivers are currently bound to devices on this bus
     * @return true if drivers are bound, false otherwise
     */
    bool hasDriversBound();
    
    /**
     * @brief Get list of devices that had drivers unbound
     * @return Vector of device names (e.g., "0-005b")
     */
    std::vector<std::string> getUnboundDevices() const { return unbound_devices_; }
    
private:
    std::string i2c_device_;                    // I2C device path (e.g., "/dev/i2c-0")
    int bus_number_;                            // Extracted bus number
    std::vector<std::string> unbound_devices_;  // List of devices we unbound
    bool drivers_unbound_;                      // Track if we've unbound drivers
    
    /**
     * @brief Extract bus number from device path
     * @param device_path Device path (e.g., "/dev/i2c-0")
     * @return Bus number, or -1 if invalid
     */
    int extractBusNumber(const std::string& device_path);
    
    /**
     * @brief Find all devices on this I2C bus that have drivers bound
     * @return Vector of device names (e.g., "0-005b")
     */
    std::vector<std::string> findBoundDevices();
    
    /**
     * @brief Unbind a specific device from its driver
     * @param device_name Device name (e.g., "0-005b")
     * @param driver_name Driver name (e.g., "pmbus")
     * @return true if successful, false otherwise
     */
    bool unbindDevice(const std::string& device_name, const std::string& driver_name);
    
    /**
     * @brief Rebind a specific device to its driver
     * @param device_name Device name (e.g., "0-005b")
     * @param driver_name Driver name (e.g., "pmbus")
     * @return true if successful, false otherwise
     */
    bool rebindDevice(const std::string& device_name, const std::string& driver_name);
    
    /**
     * @brief Get driver name for a device
     * @param device_name Device name (e.g., "0-005b")
     * @return Driver name, or empty string if not found
     */
    std::string getDriverName(const std::string& device_name);
};

#endif  // LT_I2C_DRIVER_MANAGER_H_
