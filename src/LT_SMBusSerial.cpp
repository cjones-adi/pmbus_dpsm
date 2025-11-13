/*
Copyright (c) 2020, Analog Devices Inc
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

#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#ifdef DMALLOC
#include <dmalloc.h>
#else
#include <stdlib.h>
#endif

#include "LT_Exception.h"
#include "LT_SMBusSerial.h"

#define FOUND_SIZE 0xFF
#define READ_TIMEOUT_SEC 2
#define WRITE_TIMEOUT_SEC 1

bool LT_SMBusSerial::open_ = false;
uint8_t LT_SMBusSerial::found_address_[FOUND_SIZE + 1];
int32_t LT_SMBusSerial::file_;

LT_SMBusSerial::LT_SMBusSerial() : LT_SMBus()
{
  throw LT_Exception("LT_SMBusSerial requires device path");
}

LT_SMBusSerial::LT_SMBusSerial(const char *dev) : LT_SMBus()
{
  if (!LT_SMBusSerial::open_)
  {
    strncpy(device_path_, dev, sizeof(device_path_) - 1);
    device_path_[sizeof(device_path_) - 1] = '\0';
    
    file_ = open(dev, O_RDWR | O_NOCTTY | O_NDELAY);
    if (file_ < 0)
    {
      char msg[256];
      snprintf(msg, sizeof(msg), "Failed to open serial device %s: %s", dev, strerror(errno));
      throw LT_Exception(msg);
    }
    
    if (configureSerialPort() < 0)
    {
      close(file_);
      throw LT_Exception("Failed to configure serial port");
    }
    
    clearBuffer();
    LT_SMBusSerial::open_ = true;
    printf("Opened USB-to-I2C bridge at %s\n", dev);
  }
  pec = false;
}

LT_SMBusSerial::LT_SMBusSerial(uint32_t speed) : LT_SMBus()
{
  throw LT_Exception("LT_SMBusSerial requires device path, not speed");
}

LT_SMBusSerial::~LT_SMBusSerial()
{
  if (LT_SMBusSerial::open_)
  {
    close(LT_SMBusSerial::file_);
    LT_SMBusSerial::open_ = false;
  }
}

int LT_SMBusSerial::configureSerialPort()
{
  struct termios tty;
  
  if (tcgetattr(file_, &tty) != 0)
  {
    return -1;
  }
  
  // Set baud rate to 115200
  cfsetospeed(&tty, B115200);
  cfsetispeed(&tty, B115200);
  
  // 8N1, no parity
  tty.c_cflag &= ~PARENB;
  tty.c_cflag &= ~CSTOPB;
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;
  
  // No flow control
  tty.c_cflag &= ~CRTSCTS;
  
  // Enable reading
  tty.c_cflag |= CREAD | CLOCAL;
  
  // Raw mode
  tty.c_lflag &= ~ICANON;
  tty.c_lflag &= ~ECHO;
  tty.c_lflag &= ~ECHOE;
  tty.c_lflag &= ~ECHONL;
  tty.c_lflag &= ~ISIG;
  
  tty.c_iflag &= ~(IXON | IXOFF | IXANY);
  tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
  
  tty.c_oflag &= ~OPOST;
  tty.c_oflag &= ~ONLCR;
  
  // Timeouts
  tty.c_cc[VTIME] = 20;  // 2 seconds
  tty.c_cc[VMIN] = 0;
  
  if (tcsetattr(file_, TCSANOW, &tty) != 0)
  {
    return -1;
  }
  
  return 0;
}

void LT_SMBusSerial::clearBuffer()
{
  char buf[256];
  ssize_t s;
  // Clear any pending data
  do {
    s = read(file_, &buf, 256);
  } while (s > 0);
}

void LT_SMBusSerial::setPec()
{
  uint8_t cmd_data[1] = { pec ? 1 : 0 };
  uint8_t response[16];
  uint16_t response_len;
  
  if (sendCommand(USB_I2C_CMD_SET_PEC, cmd_data, 1, response, &response_len) < 0)
  {
    throw LT_Exception("Failed to set PEC mode");
  }
}

int LT_SMBusSerial::sendCommand(uint8_t cmd, const uint8_t *data, uint16_t data_len,
                                 uint8_t *response, uint16_t *response_len)
{
  uint8_t packet[512];
  uint16_t packet_len = 0;
  ssize_t written, bytes_read;
  fd_set readfds;
  struct timeval tv;
  
  // Build packet: [CMD][LEN_LOW][LEN_HIGH][DATA...]
  packet[packet_len++] = cmd;
  packet[packet_len++] = (data_len & 0xFF);
  packet[packet_len++] = ((data_len >> 8) & 0xFF);
  
  if (data && data_len > 0)
  {
    memcpy(&packet[packet_len], data, data_len);
    packet_len += data_len;
  }
  
  // Send packet
  written = write(file_, packet, packet_len);
  if (written != packet_len)
  {
    return -1;
  }
  
  // Wait for response with timeout
  FD_ZERO(&readfds);
  FD_SET(file_, &readfds);
  tv.tv_sec = READ_TIMEOUT_SEC;
  tv.tv_usec = 0;
  
  int ret = select(file_ + 1, &readfds, NULL, NULL, &tv);
  if (ret <= 0)
  {
    return -1;  // Timeout or error
  }
  
  // Read response: [STATUS][LEN_LOW][LEN_HIGH][DATA...]
  uint8_t header[3];
  bytes_read = read(file_, header, 3);
  if (bytes_read != 3)
  {
    return -1;
  }
  
  uint8_t status = header[0];
  uint16_t resp_data_len = header[1] | (header[2] << 8);
  
  if (status != USB_I2C_RESPONSE_OK)
  {
    return -1;
  }
  
  if (resp_data_len > 0 && response && response_len)
  {
    if (resp_data_len > *response_len)
    {
      resp_data_len = *response_len;
    }
    
    bytes_read = read(file_, response, resp_data_len);
    if (bytes_read != resp_data_len)
    {
      return -1;
    }
    *response_len = resp_data_len;
  }
  else if (response_len)
  {
    *response_len = 0;
  }
  
  return 0;
}

void LT_SMBusSerial::changeSpeed(uint32_t speed)
{
  uint8_t cmd_data[4];
  cmd_data[0] = (speed & 0xFF);
  cmd_data[1] = ((speed >> 8) & 0xFF);
  cmd_data[2] = ((speed >> 16) & 0xFF);
  cmd_data[3] = ((speed >> 24) & 0xFF);
  
  uint8_t response[16];
  uint16_t response_len = sizeof(response);
  
  if (sendCommand(USB_I2C_CMD_SET_SPEED, cmd_data, 4, response, &response_len) < 0)
  {
    throw LT_Exception("Failed to change I2C speed");
  }
}

uint32_t LT_SMBusSerial::getSpeed()
{
  // Not implemented for serial bridge
  return 0;
}

int LT_SMBusSerial::writeByte(uint8_t address, uint8_t command, uint8_t data)
{
  setPec();
  
  uint8_t cmd_data[3] = { address, command, data };
  uint8_t response[16];
  uint16_t response_len = sizeof(response);
  
  if (sendCommand(USB_I2C_CMD_WRITE_BYTE, cmd_data, 3, response, &response_len) < 0)
  {
    throw LT_Exception("Write Byte: fail");
  }
  
  return 0;
}

int LT_SMBusSerial::writeBytes(uint8_t *addresses, uint8_t *commands, 
                                uint8_t *data, uint8_t no_addresses)
{
  for (uint8_t i = 0; i < no_addresses; i++)
  {
    if (writeByte(addresses[i], commands[i], data[i]) < 0)
    {
      return -1;
    }
  }
  return 0;
}

int LT_SMBusSerial::readByte(uint8_t address, uint8_t command)
{
  setPec();
  
  uint8_t cmd_data[2] = { address, command };
  uint8_t response[16];
  uint16_t response_len = sizeof(response);
  
  if (sendCommand(USB_I2C_CMD_READ_BYTE, cmd_data, 2, response, &response_len) < 0)
  {
    throw LT_Exception("Read Byte: fail");
  }
  
  if (response_len < 1)
  {
    throw LT_Exception("Read Byte: invalid response");
  }
  
  return (int)response[0];
}

int LT_SMBusSerial::writeWord(uint8_t address, uint8_t command, uint16_t data)
{
  setPec();
  
  uint8_t cmd_data[4] = { address, command, (uint8_t)(data & 0xFF), (uint8_t)((data >> 8) & 0xFF) };
  uint8_t response[16];
  uint16_t response_len = sizeof(response);
  
  if (sendCommand(USB_I2C_CMD_WRITE_WORD, cmd_data, 4, response, &response_len) < 0)
  {
    throw LT_Exception("Write Word: fail");
  }
  
  return 0;
}

int LT_SMBusSerial::readWord(uint8_t address, uint8_t command)
{
  setPec();
  
  uint8_t cmd_data[2] = { address, command };
  uint8_t response[16];
  uint16_t response_len = sizeof(response);
  
  if (sendCommand(USB_I2C_CMD_READ_WORD, cmd_data, 2, response, &response_len) < 0)
  {
    char msg[132];
    snprintf(msg, sizeof(msg), "Read Word: fail with address 0x%02x command 0x%02x", address, command);
    throw LT_Exception(msg);
  }
  
  if (response_len < 2)
  {
    throw LT_Exception("Read Word: invalid response");
  }
  
  return (int)(response[0] | (response[1] << 8));
}

int LT_SMBusSerial::writeBlock(uint8_t address, uint8_t command,
                                 uint8_t *block, uint16_t block_size)
{
  setPec();
  
  uint8_t cmd_data[258];
  cmd_data[0] = address;
  cmd_data[1] = command;
  cmd_data[2] = (uint8_t)block_size;
  
  if (block_size > 0 && block_size <= 255)
  {
    memcpy(&cmd_data[3], block, block_size);
  }
  else
  {
    throw LT_Exception("Write Block: invalid block size");
  }
  
  uint8_t response[16];
  uint16_t response_len = sizeof(response);
  
  if (sendCommand(USB_I2C_CMD_WRITE_BLOCK, cmd_data, 3 + block_size, response, &response_len) < 0)
  {
    throw LT_Exception("Write Block: fail");
  }
  
  return 0;
}

int LT_SMBusSerial::writeReadBlock(uint8_t address, uint8_t command,
                                     uint8_t *block_out, uint16_t block_out_size,
                                     uint8_t *block_in, uint16_t block_in_size)
{
  throw LT_Exception("Write/Read Block: not supported on serial bridge");
}

int LT_SMBusSerial::readBlock(uint8_t address, uint8_t command,
                                uint8_t *block, uint16_t block_size)
{
  setPec();
  
  uint8_t cmd_data[3] = { address, command, (uint8_t)block_size };
  uint8_t response[256];
  uint16_t response_len = sizeof(response);
  
  if (sendCommand(USB_I2C_CMD_READ_BLOCK, cmd_data, 3, response, &response_len) < 0)
  {
    throw LT_Exception("Read Block: fail");
  }
  
  if (response_len > block_size)
  {
    response_len = block_size;
  }
  
  if (response_len > 0)
  {
    memcpy(block, response, response_len);
  }
  
  return response_len;
}

int LT_SMBusSerial::sendByte(uint8_t address, uint8_t command)
{
  setPec();
  
  uint8_t cmd_data[2] = { address, command };
  uint8_t response[16];
  uint16_t response_len = sizeof(response);
  
  if (sendCommand(USB_I2C_CMD_SEND_BYTE, cmd_data, 2, response, &response_len) < 0)
  {
    throw LT_Exception("Send Byte: fail");
  }
  
  return 0;
}

int LT_SMBusSerial::readAlert(void)
{
  // ARA not implemented for serial bridge
  throw LT_Exception("Read Alert: not supported on serial bridge");
}

int LT_SMBusSerial::waitForAck(uint8_t address, uint8_t command)
{
  // Simple implementation - try reading
  int retries = 100;
  while (retries-- > 0)
  {
    try
    {
      readByte(address, command);
      return 0;
    }
    catch (...)
    {
      usleep(10000);  // 10ms delay
    }
  }
  return -1;
}

uint8_t* LT_SMBusSerial::probe(uint8_t command)
{
  setPec();
  
  uint8_t cmd_data[1] = { command };
  uint8_t response[128];
  uint16_t response_len = sizeof(response);
  
  if (sendCommand(USB_I2C_CMD_PROBE, cmd_data, 1, response, &response_len) < 0)
  {
    found_address_[0] = 0;
    return found_address_;
  }
  
  // Response contains list of found addresses
  uint8_t count = (response_len < FOUND_SIZE) ? response_len : FOUND_SIZE;
  memcpy(found_address_, response, count);
  found_address_[count] = 0;
  
  return found_address_;
}

uint8_t* LT_SMBusSerial::probeUnique(uint8_t command)
{
  uint8_t *all_addresses = probe(command);
  uint8_t unique_count = 0;
  
  // Filter out broadcast addresses
  for (uint8_t i = 0; all_addresses[i] != 0 && i < FOUND_SIZE; i++)
  {
    uint8_t addr = all_addresses[i];
    // Skip global/broadcast addresses (0x00-0x0F)
    if (addr >= 0x10)
    {
      found_address_[unique_count++] = addr;
    }
  }
  
  found_address_[unique_count] = 0;
  return found_address_;
}
