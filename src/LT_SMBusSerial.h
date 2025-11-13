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

#ifndef LT_SMBusSerial_H_
#define LT_SMBusSerial_H_

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "LT_SMBus.h"

//! USB-to-I2C Bridge Protocol Commands
#define USB_I2C_CMD_WRITE_BYTE      0x01
#define USB_I2C_CMD_READ_BYTE       0x02
#define USB_I2C_CMD_WRITE_WORD      0x03
#define USB_I2C_CMD_READ_WORD       0x04
#define USB_I2C_CMD_WRITE_BLOCK     0x05
#define USB_I2C_CMD_READ_BLOCK      0x06
#define USB_I2C_CMD_SEND_BYTE       0x07
#define USB_I2C_CMD_PROBE           0x08
#define USB_I2C_CMD_SET_PEC         0x09
#define USB_I2C_CMD_SET_SPEED       0x0A

#define USB_I2C_RESPONSE_OK         0x00
#define USB_I2C_RESPONSE_NACK       0x01
#define USB_I2C_RESPONSE_ERROR      0x02

//! Class for USB-to-I2C serial bridge communication
//! Supports DC1613A and similar USB-to-I2C adapters
class LT_SMBusSerial : public LT_SMBus
{
  protected:
    static bool open_;
    static uint8_t found_address_[256];
    static int32_t file_;
    bool pec;
    char device_path_[256];
    
    void setPec();
    
    //! Send command to USB bridge and get response
    int sendCommand(uint8_t cmd, const uint8_t *data, uint16_t data_len, 
                    uint8_t *response, uint16_t *response_len);
    
    //! Configure serial port for USB bridge
    int configureSerialPort();

  public:
    LT_SMBusSerial();
    LT_SMBusSerial(const char *dev);
    LT_SMBusSerial(uint32_t speed);
    virtual ~LT_SMBusSerial();

    void clearBuffer();

    //! Change the speed of the bus.
    void changeSpeed(uint32_t speed);

    //! Get the speed of the bus.
    uint32_t getSpeed();

    //! SMBus write byte command
    //! @return error < 0
    int writeByte(uint8_t address,     //!< Slave address
                   uint8_t command,     //!< Command byte
                   uint8_t data         //!< Data to send
                  );

    //! SMBus write byte command for a list of addresses
    //! @return error < 0
    int writeBytes(uint8_t *addresses,         //!< Slave Addresses
                    uint8_t *commands,          //!< Command bytes
                    uint8_t *data,              //!< Data to send
                    uint8_t no_addresses
                   );

    //! SMBus read byte command
    //! @return error < 0
    int readByte(uint8_t address,        //!< Slave Address
                     uint8_t command         //!< Command byte
                    );

    //! SMBus write word command
    //! @return error < 0
    int writeWord(uint8_t address,     //!< Slave Address
                   uint8_t command,     //!< Command byte
                   uint16_t data        //!< Data to send
                  );

    //! SMBus read word command
    //! @return error < 0
    int readWord(uint8_t address,      //!< Slave Address
                      uint8_t command       //!< Command byte
                     );

    //! SMBus write block command
    //! @return error < 0
    int writeBlock(uint8_t address,        //!< Slave Address
                    uint8_t command,        //!< Command byte
                    uint8_t *block,         //!< Data to send
                    uint16_t block_size
                   );

    //! SMBus write then read block command
    //! @return error < 0 | count
    int writeReadBlock(uint8_t address,         //!< Slave Address
                           uint8_t command,         //!< Command byte
                           uint8_t *block_out,      //!< Data to send
                           uint16_t block_out_size, //!< Size of data to send
                           uint8_t *block_in,       //!< Memory to receive data
                           uint16_t block_in_size   //!< Size of receive data memory
                          );

    //! SMBus read block command
    //! @return error < 0
    int readBlock(uint8_t address,         //!< Slave Address
                      uint8_t command,         //!< Command byte
                      uint8_t *block,          //!< Memory to receive data
                      uint16_t block_size      //!< Size of receive data memory
                     );

    //! SMBus send byte command
    //! @return error < 0
    int sendByte(uint8_t address,      //!< Slave Address
                  uint8_t command       //!< Command byte
                 );

    //! Perform ARA
    //! @return error < 0
    int readAlert(void);

    //! Read with the address and command in loop until ack, then issue stop
    //! @return error < 0
    int waitForAck(uint8_t address,        //!< Slave Address
                       uint8_t command         //!< Command byte
                      );

    //! SMBus bus probe
    //! @return array of addresses
    uint8_t *probe(uint8_t command      //!< Command byte
                  );

    //! SMBus bus probe
    //! @return array of unique addresses (no global addresses)
    uint8_t *probeUnique(uint8_t command      //!< Command byte
                        );
};

#endif /* LT_SMBusSerial_H_ */
