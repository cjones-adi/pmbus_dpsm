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

#include "LT_I2CDriverManager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

LT_I2CDriverManager::LT_I2CDriverManager(const std::string& i2c_device)
    : i2c_device_(i2c_device), drivers_unbound_(false)
{
    bus_number_ = extractBusNumber(i2c_device);
}

LT_I2CDriverManager::~LT_I2CDriverManager()
{
    // Auto-rebind on destruction if we unbound drivers
    if (drivers_unbound_) {
        std::cout << "Auto-rebinding drivers on cleanup..." << std::endl;
        rebind();
    }
}

int LT_I2CDriverManager::extractBusNumber(const std::string& device_path)
{
    // Extract number from "/dev/i2c-N"
    size_t pos = device_path.rfind("-");
    if (pos != std::string::npos) {
        try {
            return std::stoi(device_path.substr(pos + 1));
        } catch (...) {
            return -1;
        }
    }
    return -1;
}

std::vector<std::string> LT_I2CDriverManager::findBoundDevices()
{
    std::vector<std::string> devices;
    
    if (bus_number_ < 0) {
        std::cerr << "Invalid bus number" << std::endl;
        return devices;
    }
    
    // Scan /sys/bus/i2c/devices/ for devices on our bus
    DIR* dir = opendir("/sys/bus/i2c/devices");
    if (!dir) {
        std::cerr << "Cannot open /sys/bus/i2c/devices" << std::endl;
        return devices;
    }
    
    struct dirent* entry;
    std::string bus_prefix = std::to_string(bus_number_) + "-";
    
    while ((entry = readdir(dir)) != nullptr) {
        std::string name(entry->d_name);
        
        // Look for entries like "0-005b" (bus-address format)
        if (name.find(bus_prefix) == 0) {
            // Check if this device has a driver bound
            std::string driver_path = "/sys/bus/i2c/devices/" + name + "/driver";
            struct stat st;
            if (stat(driver_path.c_str(), &st) == 0) {
                devices.push_back(name);
            }
        }
    }
    
    closedir(dir);
    return devices;
}

std::string LT_I2CDriverManager::getDriverName(const std::string& device_name)
{
    // Read the symlink at /sys/bus/i2c/devices/<device>/driver
    std::string driver_link = "/sys/bus/i2c/devices/" + device_name + "/driver";
    
    char buf[256];
    ssize_t len = readlink(driver_link.c_str(), buf, sizeof(buf) - 1);
    if (len == -1) {
        return "";
    }
    buf[len] = '\0';
    
    // Extract driver name from path like "../../../bus/i2c/drivers/pmbus"
    std::string driver_path(buf);
    size_t pos = driver_path.rfind('/');
    if (pos != std::string::npos) {
        return driver_path.substr(pos + 1);
    }
    
    return "";
}

bool LT_I2CDriverManager::unbindDevice(const std::string& device_name, const std::string& driver_name)
{
    std::string unbind_path = "/sys/bus/i2c/drivers/" + driver_name + "/unbind";
    
    std::ofstream unbind_file(unbind_path);
    if (!unbind_file) {
        std::cerr << "Cannot open " << unbind_path << " (may need sudo)" << std::endl;
        return false;
    }
    
    unbind_file << device_name;
    unbind_file.close();
    
    if (unbind_file.fail()) {
        std::cerr << "Failed to unbind device " << device_name << std::endl;
        return false;
    }
    
    // Verify unbinding succeeded
    usleep(100000);  // Wait 100ms for unbind to complete
    std::string driver_path = "/sys/bus/i2c/devices/" + device_name + "/driver";
    struct stat st;
    if (stat(driver_path.c_str(), &st) == 0) {
        std::cerr << "Device " << device_name << " still has driver bound" << std::endl;
        return false;
    }
    
    return true;
}

bool LT_I2CDriverManager::rebindDevice(const std::string& device_name, const std::string& driver_name)
{
    std::string bind_path = "/sys/bus/i2c/drivers/" + driver_name + "/bind";
    
    std::ofstream bind_file(bind_path);
    if (!bind_file) {
        std::cerr << "Cannot open " << bind_path << " (may need sudo)" << std::endl;
        return false;
    }
    
    bind_file << device_name;
    bind_file.close();
    
    if (bind_file.fail()) {
        std::cerr << "Failed to rebind device " << device_name << std::endl;
        return false;
    }
    
    // Verify rebinding succeeded
    usleep(500000);  // Wait 500ms for driver to initialize
    std::string driver_path = "/sys/bus/i2c/devices/" + device_name + "/driver";
    struct stat st;
    if (stat(driver_path.c_str(), &st) != 0) {
        std::cerr << "Device " << device_name << " failed to rebind to driver" << std::endl;
        return false;
    }
    
    return true;
}

bool LT_I2CDriverManager::unbind()
{
    if (drivers_unbound_) {
        std::cout << "Drivers already unbound" << std::endl;
        return true;
    }
    
    std::vector<std::string> devices = findBoundDevices();
    
    if (devices.empty()) {
        std::cout << "No devices with bound drivers found on bus " << bus_number_ << std::endl;
        return true;
    }
    
    std::cout << "Found " << devices.size() << " device(s) with drivers bound on bus " 
              << bus_number_ << std::endl;
    
    // Unbind each device and track which ones we unbind
    for (const auto& device : devices) {
        std::string driver = getDriverName(device);
        if (driver.empty()) {
            std::cerr << "Cannot determine driver for device " << device << std::endl;
            continue;
        }
        
        std::cout << "Unbinding device " << device << " from driver " << driver << "..." << std::endl;
        
        if (unbindDevice(device, driver)) {
            // Store device and driver for rebinding later
            unbound_devices_.push_back(device + ":" + driver);
            std::cout << "  ✓ Successfully unbound" << std::endl;
        } else {
            std::cerr << "  ✗ Failed to unbind" << std::endl;
        }
    }
    
    if (!unbound_devices_.empty()) {
        drivers_unbound_ = true;
        std::cout << "Successfully unbound " << unbound_devices_.size() << " device(s)" << std::endl;
        std::cout << "Telemetry is now stopped. Programming can proceed safely." << std::endl;
        return true;
    }
    
    return false;
}

bool LT_I2CDriverManager::rebind()
{
    if (!drivers_unbound_) {
        std::cout << "No drivers to rebind" << std::endl;
        return true;
    }
    
    if (unbound_devices_.empty()) {
        std::cout << "No devices were unbound" << std::endl;
        drivers_unbound_ = false;
        return true;
    }
    
    std::cout << "Rebinding " << unbound_devices_.size() << " device(s)..." << std::endl;
    
    bool all_success = true;
    for (const auto& entry : unbound_devices_) {
        // Split "device:driver"
        size_t pos = entry.find(':');
        if (pos == std::string::npos) continue;
        
        std::string device = entry.substr(0, pos);
        std::string driver = entry.substr(pos + 1);
        
        std::cout << "Rebinding device " << device << " to driver " << driver << "..." << std::endl;
        
        if (rebindDevice(device, driver)) {
            std::cout << "  ✓ Successfully rebound" << std::endl;
        } else {
            std::cerr << "  ✗ Failed to rebind" << std::endl;
            all_success = false;
        }
    }
    
    unbound_devices_.clear();
    drivers_unbound_ = false;
    
    if (all_success) {
        std::cout << "All drivers rebound successfully. Telemetry should resume." << std::endl;
    } else {
        std::cerr << "Some drivers failed to rebind. Manual intervention may be required." << std::endl;
    }
    
    return all_success;
}

bool LT_I2CDriverManager::hasDriversBound()
{
    std::vector<std::string> devices = findBoundDevices();
    return !devices.empty();
}
