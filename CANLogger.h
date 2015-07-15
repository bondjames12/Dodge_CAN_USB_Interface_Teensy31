/*
  Simple CAN data to (micro)SD card logger
  (c) Pawelsky 20140527

  Logs incoming CAN data @ 1Mbps to a (micro)SD card. Periodically sends Naza heartbeat message.
  Uses EEPROM to store log file number which is increased at each boot.

  Log format (15 bytes per CAN message): TS TS TS TS CI CI LE FF DA DA DA DA DA DA DA DA, where
  TS - timestamp   (4 byte number - little endian)
  CI - CAN ID      (2 byte number - little endian)
  LE - data length (1 byte number)
  FF - stuffing, always 0xFF, just to make the log entry 16 bytes long for easier reading
  DA - data        (8 bytes)

  Requires (micro)SD card shield and CAN transciever

  Requires FlexCAN library (tested with https://github.com/teachop/FlexCAN_Library/releases/tag/v0.1-beta)
  Requires SdFat library (tested with https://sdfatlib.googlecode.com/files/sdfatlib20131225.zip)
*/

#ifndef CANLOGGER_H_
#define CANLOGGER_H_

#include "Arduino.h"
#include <SPI.h>
#include <SD.h>
#include <CAN.h>



class CANLogger
{
public:
	CANLogger(int eepromLogAddress, int chipselectPin, int SDCardPresentPin);
    ~CANLogger();

    union
    {
      struct  __attribute__((packed))
      {
        uint32_t timestamp;
        uint16_t canId;
        uint8_t  dataLen;
        uint8_t  stuffing;
      } header;
      uint8_t      bytes[8];
    } headerData;

    int Log(CAN_Frame message);

private:
    File logFile;
    uint32_t currentTime = 0;
    uint32_t syncTime = 0;
    byte fileNumber;
    char fileName[12];
};

#endif /* CANLOGGER_H_ */
