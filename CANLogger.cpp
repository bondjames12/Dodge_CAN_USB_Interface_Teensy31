#include "Arduino.h"
#include <EEPROM.h>
#include "CANLogger.h"

CANLogger::CANLogger(int eepromLogAddress, int chipselectPin, int SDCardPresentPin)
{
	pinMode( SDCardPresentPin, INPUT_PULLUP );
	if(digitalRead(SDCardPresentPin) != LOW) //NO SD Card is inserted
	{
		#ifdef _CANLOGGER_DEBUG_ENABLED
		Serial.print("CANLogger::No SD Card present. Error on SDCardPresentPin");
		#endif
		return;
	}

	#ifdef _CANLOGGER_DEBUG_ENABLED
	Serial.print("CANLogger::Initializing SD card...");
	#endif
	headerData.header.stuffing = 0xFF;
	// see if the card is present and can be initialized:
	if (!SD.begin(chipselectPin)) {
		#ifdef _CANLOGGER_DEBUG_ENABLED
		Serial.println("CANLogger::Card failed, or not present");
		#endif
		// don't do anything more:
		return;
	}
	fileNumber = EEPROM.read(eepromLogAddress);
	if(fileNumber >= 255) fileNumber = 0; else fileNumber++;
	EEPROM.write(eepromLogAddress, fileNumber);
	sprintf(fileName, "file%03u.log", fileNumber);

	logFile = SD.open(fileName, O_CREAT | O_APPEND | O_WRITE);
	if (!logFile) {
		#ifdef _CANLOGGER_DEBUG_ENABLED
		Serial.println("CANLogger::Failed to create or open log file.");
		#endif
		return;
	}
	#ifdef _CANLOGGER_DEBUG_ENABLED
	Serial.println("CANLogger::card initialized.");
	#endif
}

int CANLogger::Log(CAN_Frame message)
{
	if (logFile) {
		currentTime = micros();
		headerData.header.timestamp = (uint32_t)currentTime;
		headerData.header.canId = (uint16_t)message.id;
		headerData.header.dataLen = (uint8_t)message.length;
		logFile.write(headerData.bytes, 8);
		logFile.write(message.data, 8);

		if(syncTime < currentTime)
		{
			syncTime = currentTime + 5000000;
			logFile.flush();
		}
	} else {
		#ifdef _CANLOGGER_DEBUG_ENABLED
		Serial.println("CANLogger::Failed to write to log file.");
		#endif
	}
}

CANLogger::~CANLogger()
{
	if (logFile) {
		#ifdef _CANLOGGER_DEBUG_ENABLED
		Serial.println("CANLogger::Closing log file");
		#endif
		logFile.close();
	} else {
		#ifdef _CANLOGGER_DEBUG_ENABLED
		Serial.println("CANLogger::Attempted to close log file in constructor but failed.");
		#endif
	}
}
