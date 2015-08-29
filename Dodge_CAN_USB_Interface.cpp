/* Pins used in this project
 * 0 - Serial1 RX Broken out on Header
 * 1 - Serial1 TX Broken out on Header
 * 2 -
 * 3 - CAN TX (LOW for dominant)
 * 4 - CAN RX (HIGH for recessive)
 * 5 -
 * 6 -
 * 7 - Serial3 RX Broken out on Header
 * 8 - Serial3 TX Broken out on Header
 * 9 - SD card detection (shorted to GND when card inserted, floating otherwise)
 * 10- CS/CD (SD Card SPI Slave Select)
 * 11- DOUT  (SD Card SPI Master Input <-- Slave Output) FAT library uses these hardcoded
 * 12- DIN   (SD Card SPI Master Output --> Slave Input) FAT library uses these hardcoded
 * 13- SCK   (SD Card SPI Serial Clock) and Builtin LED  FAT library uses these hardcoded
 * 17    - Physically blocked by SD Card slot
 * 18/A4 - I2C SDA0 (Used to communicate with FM Board)
 * 19/A5 - I2C SCL0 (Used to communicate with FM Board)
 * 20/A6 - FM Board SEN
 * 21/A7 - FM Board Reset
 * 23/A9 -IO-1 and CAN Chip Enable Pin (some variants of my CAN board need this)
 * 22/A8 -IO-2 Broken out on Header
 * 21/A7 -IO-3 Broken out on Header
 * 20/A6 -IO-4 Broken out on Header
 */
#include <Arduino.h>
#include <usb_rawhid.h> //to communicate with PC or Android
#include <EEPROM.h> //Use EEPROM to store last AMP settings and restore on powerup
#include <Metro.h> //To schedule events for processing at different intervals

#include <CAN.h>
#include <CAN_K2X.h>
#include <sn65hvd234.h>
#include "Dodge_CAN_USB_Interface.h"
#include "radioEmulator.h"
//#include <CAN_SAM3X.h>

//Pre-processor compile directives
#define _WATCHDOG_ENABLED //Enable watchdog timer code compilation
#define _SI4703FM_ENABLED //Enable compilation of code related the the Si4703 FM radio board
// Define our CAN speed (bitrate).
#define bitrate CAN_BPS_83K//CAN_BPS_500K

//FM Radio specific headers and global variables
#ifdef _SI4703FM_ENABLED
#include <SparkFunSi4703.h>
#include <Wire.h>

int FMresetPin = 21;
int SDIO = A4;
int SCLK = A5;

Si4703_Breakout Si4703Radio(FMresetPin, SDIO, SCLK);
#endif

//Global variables and objects
#ifdef _WATCHDOG_ENABLED
Metro* wdtimer = new Metro(500); //schedule a timer to kick the watchdog
#endif
RadioEmulator *radio;
//Metro* usbUpdateTimer = new Metro(500); //schedule a timer to send updates over USB

// data counter just to show a dynamic change in data messages
uint32_t standard_counter = 0;

#ifdef _WATCHDOG_ENABLED
#ifdef __cplusplus
extern "C" {
#endif
void startup_early_hook() { //Define the startup early function its called by teensy very early at startup
	//Now we will set out watchdog timer settings
	WDOG_TOVALL = 30000; // The next 2 lines sets the time-out value. This is the value that the watchdog timer compa
	WDOG_TOVALH = 0;
	WDOG_PRESC = 0; // This sets prescale clock so that the watchdog timer ticks at 1kHZ instead of the default 1kHZ/4 = 200 HZ
	//for a 1 second timer use WDOG_TOVALL=(1000/5)  WDOG_TOVALH = 0  WDOG_PRESC=0
	//for a 1 about minute timer use WDOG_TOVALL=30000  WDOG_TOVALH = 10000  WDOG_PRESC=0
	WDOG_STCTRLH = (WDOG_STCTRLH_ALLOWUPDATE | WDOG_STCTRLH_WDOGEN | WDOG_STCTRLH_WAITEN | WDOG_STCTRLH_STOPEN); // Enable WDG
}
#ifdef __cplusplus
}
#endif

void WatchdogChange()
{
	WDOG_UNLOCK = WDOG_UNLOCK_SEQ1;
	WDOG_UNLOCK = WDOG_UNLOCK_SEQ2;
	//Watchdog timeout register low
	//Defines the lower 16 bits of the 32-bit time-out value for the watchdog timer. It is defined in terms of cycles of the watchdog clock
	WDOG_TOVALL = 10000; //10 The next 2 lines sets the time-out value. This is the value that the watchdog timer compares itself to
	//Watchdog timeout register high (upper 16bits)
	WDOG_TOVALH = 0;
	//10,000 results in a 10second watchdog
	WDOG_PRESC = 0; // This sets prescale clock so that the watchdog timer ticks at 1kHZ instead of the default 1kHZ/4 = 200 HZ
	WDOG_STCTRLH = (WDOG_STCTRLH_ALLOWUPDATE | WDOG_STCTRLH_WDOGEN); // Enable WDG
}

void WatchdogKick()
{
	// use the following 4 lines to kick the dog
	noInterrupts();
	WDOG_REFRESH = 0xA602;
	WDOG_REFRESH = 0xB480;
	interrupts();
	// if you don't refresh the watchdog timer before it runs out, the system will be rebooted
	//delay(1); // the smallest delay needed between each refresh is 1ms. anything faster and it will also reboot.
}
#endif

void setup()
{
	pinMode( 23, OUTPUT ) ; //CAN Chip Enable Pin
	digitalWrite(23, HIGH ) ; //Set CAN ENable to high (turns chip on)
	delay(100);

	//Serial.begin(115200); // Initialize Serial communications with computer to use serial monitor
	//Set CAN speed.
	CAN.begin(bitrate);
	CAN_Filter filter_3A0;//STEER CONTROLS
	filter_3A0.extended=0;
	filter_3A0.rtr=0;
	filter_3A0.id = 0x3A0;
	CAN_Filter filter_002;//SPEED
	filter_002.extended=0;
	filter_002.rtr=0;
	filter_002.id = 0x002;
	CAN_Filter filter_014;//ODOMETER
	filter_014.extended=0;
	filter_014.rtr=0;
	filter_014.id = 0x014;
	CAN_Filter filter_015;//BAT VOLTAGE
	filter_015.extended=0;
	filter_015.rtr=0;
	filter_015.id = 0x015;
	//Set CAN RX filters up to 8 available
	CAN.setFilter(filter_3A0,0);
	CAN.setFilter(filter_3A0,1);
	CAN.setFilter(filter_3A0,2);
	CAN.setFilter(filter_002,3);
	CAN.setFilter(filter_3A0,4);
	CAN.setFilter(filter_3A0,5);
	CAN.setFilter(filter_3A0,6);
	CAN.setFilter(filter_3A0,7);
	//delay(4000); // Delay added just so we can have time to open up Serial Monitor and CAN bus monitor. It can be removed later...
	//printResetType(); //print what kind of reset happened to serial interface
#ifdef _WATCHDOG_ENABLED
	WatchdogChange();
#endif
	radio = new RadioEmulator(&CAN, false);
	radio->status._volume = 10;//EEPROM.read(0);
	radio->status._balance = EEPROM.read(1);
	radio->status._fade = EEPROM.read(2);
	radio->status._bass = EEPROM.read(3);
	radio->status._mid = EEPROM.read(4);
	radio->status._treble = EEPROM.read(5);

#ifdef _SI4703FM_ENABLED
	//Si4703Radio.powerOn();
	//Si4703Radio.setVolume(15);
#endif
}

#ifdef _WATCHDOG_ENABLED
void printResetType() {
    if (RCM_SRS1 & RCM_SRS1_SACKERR)   Serial.println("[RCM_SRS1] - Stop Mode Acknowledge Error Reset");
    if (RCM_SRS1 & RCM_SRS1_MDM_AP)    Serial.println("[RCM_SRS1] - MDM-AP Reset");
    if (RCM_SRS1 & RCM_SRS1_SW)        Serial.println("[RCM_SRS1] - Software Reset");
    if (RCM_SRS1 & RCM_SRS1_LOCKUP)    Serial.println("[RCM_SRS1] - Core Lockup Event Reset");
    if (RCM_SRS0 & RCM_SRS0_POR)       Serial.println("[RCM_SRS0] - Power-on Reset");
    if (RCM_SRS0 & RCM_SRS0_PIN)       Serial.println("[RCM_SRS0] - External Pin Reset");
    if (RCM_SRS0 & RCM_SRS0_WDOG)      Serial.println("[RCM_SRS0] - Watchdog(COP) Reset");
    if (RCM_SRS0 & RCM_SRS0_LOC)       Serial.println("[RCM_SRS0] - Loss of External Clock Reset");
    if (RCM_SRS0 & RCM_SRS0_LOL)       Serial.println("[RCM_SRS0] - Loss of Lock in PLL Reset");
    if (RCM_SRS0 & RCM_SRS0_LVD)       Serial.println("[RCM_SRS0] - Low-voltage Detect Reset");
}
#endif

// Create a function to load and send a standard frame message
void sendCANMessage()
{
  CAN_Frame standard_message; // Create message object to use CAN message structure

  standard_message.id = 0x555; // Random Standard Message ID
  standard_message.valid = true;
  standard_message.rtr = 0;
  standard_message.extended = CAN_STANDARD_FRAME;
  standard_message.length = 8; // Data length

  // counter in the first 4 bytes, timing in the last 4 bytes
  uint32_t timehack = millis();

  // Load the data directly into CAN_Frame
  standard_message.data[0] = (standard_counter >> 24);
  standard_message.data[1] = (standard_counter >> 16);
  standard_message.data[2] = (standard_counter >> 8);
  standard_message.data[3] = (standard_counter & 0xFF);
  standard_message.data[4] = (timehack >> 24);
  standard_message.data[5] = (timehack >> 16);
  standard_message.data[6] = (timehack >> 8);
  standard_message.data[7] = (timehack & 0xFF);

  CAN.write(standard_message); // Load message and send
  standard_counter++; // increase count
  //printMessage(standard_message); //print what we sent to serial port for testing
}

// Create a function to read message and display it through Serial Monitor
void readCANMessage()
{

}

void printCANMessage(CAN_Frame message)
{
	//Serial.print(millis());
	//Serial.print(F(",0x"));
	Serial.print(message.id, HEX); //display message ID
	Serial.print(' ');
	//Serial.print(message.rtr); //display message RTR
	//Serial.print(',');
	//Serial.print(message.extended); //display message EID
	//Serial.print(',');
	//if (message.rtr == 1)
	//{
	//  Serial.print(F(" REMOTE REQUEST MESSAGE ")); //technically if its RTR frame/message it will not have data So display this
	//}
	//else
	//{
	  Serial.print(message.length, HEX); //display message length
	  for (byte i = 0; i < message.length; i++)
	  {
		Serial.print(' ');
		if (message.data[i] < 0x10) // If the data is less than 10 hex it will assign a zero to the front as leading zeros are ignored...
		{
		  Serial.print('0');
		}
		Serial.print(message.data[i], HEX); //display data based on length
	  }
	//}
  Serial.println(); // adds a line
}

void printCANMessageUSBHID(CAN_Frame message)
{
	uint8_t sendMessage[64] = {}; //buffer
	sendMessage[0] = message.length + 5; //Total length of this data packet
	//2nd byte is a command reference
	sendMessage[1] = 0x01; //0x01 means this is a received CAN frame
	//Message ID (11bits total stored over 2 bytes)
	sendMessage[2] = (message.id >> 8);
	sendMessage[3] = (message.id & 0xFF);
	//The CAN frame data length
	sendMessage[4] = (message.length);
	//Now fill in data bytes
	for (byte i = 0; i < message.length; i++)
	{
		sendMessage[i+5] = message.data[i];
	}
	//send to PC, allow 100ms for sending the frame
	RawHID.send(sendMessage, 200);
}

void SendHID(String text)
{
  uint8_t sendMessage[64] = {}; //buffer
  text.getBytes(sendMessage,text.length()+1); //convert text to bytes
//send to PC, allow 100ms for sending the frame
  RawHID.send(sendMessage, 200);
}

void loop()
{
#ifdef _WATCHDOG_ENABLED
//Prevent watchdog resets
if(wdtimer->check())
		WatchdogKick();
#endif

	CAN_Frame message; // Create message object to use CAN message structure
	message.valid = false; //init to false
	if (CAN.available() == true) // Check to see if a valid message has been received.
	{
		message = CAN.read(); //read message, it will follow the CAN structure of ID,RTR, legnth, data. Allows both Extended or Standard
		printCANMessageUSBHID(message);
		//printMessage(message);
	}

	//Run radio emulator will send out any CAN messages if needed and parse any message we got above
	radio->Operate();
	//If we have a Valid CAN message frame then parse it!
	if(message.valid)
	{
		radio->ParseCANMessage(message);
		if(message.id == 0x3A0)
		{
			SendUSBAmpUpdate();
		}
	}

	//look for anything we received over USB HID
	int bytesAvaliable = RawHID.available();
	if(bytesAvaliable > 0) //we got something over USB!
	{
		//get the bytes
		uint8_t buf[64];
		RawHID.recv(buf,100);
/*
		//print everything to serial and back to usb
		for (byte i = 0; i < 64; i++)
		{
			Serial.print(buf[i]);
			RawHID.send(buf,100);
		}
*/
		//1st byte is a command reference a 0x01 is a CAN FRAME
		switch(buf[0])
		{
		case 0x01: //Send CAN Frame Command
			//2nd byte and 3rd byte are the CAN ID
			//4th byte is CAN data length
			//4th - 12 are CAN data bytes
			CAN_Frame msg; // Create message object to use CAN message structure
			//these 2 lines below combine 2 8bit variables into one 16bit variable (although only 11 bits are used for CAN IDs)
			msg.id = (buf[1]<<8); //shift first byte left 8 bits
			msg.id = (msg.id | buf[2]); //OR in 2nd byte into right 8 bits
			msg.valid = true;
			msg.rtr = 0;
			msg.extended = CAN_STANDARD_FRAME;
			msg.length = buf[3]; // Data length
			// Load the data directly into CAN_Frame
			//Now fill in data bytes
			for (byte i = 0; i < msg.length; i++)
			{
				msg.data[i] = buf[i+4];
			}
			CAN.write(msg); // Load message and send
			break;
		case 0x02: //Start infinite loop, should trigger a watchdog reset
			while(1);
			break;
		case 0x03: //Volume increment
				//first byte is signed increment value
				radio->VolumeIncrement((int8_t)buf[1]);
				SendUSBAmpUpdate();
				//SaveVolumeEEPROM();
			break;
		case 0x04: //Balance increment
				//first byte is signed increment value
				radio->BalanceIncrement((int8_t)buf[1]);
				SendUSBAmpUpdate();
				SaveBalanceEEPROM();
			break;
		case 0x05: //Fade increment
				//first byte is signed increment value
				radio->FadeIncrement((int8_t)buf[1]);
				SendUSBAmpUpdate();
				SaveFadeEEPROM();
			break;
		case 0x06: //Bass increment
				//first byte is signed increment value
				radio->BassIncrement((int8_t)buf[1]);
				SendUSBAmpUpdate();
				SaveBassEEPROM();
			break;
		case 0x07: //Mid increment
				//first byte is signed increment value
				radio->MidIncrement((int8_t)buf[1]);
				SendUSBAmpUpdate();
				SaveMidEEPROM();
			break;
		case 0x08: //Treble increment
				//first byte is signed increment value
				radio->TrebleIncrement((int8_t)buf[1]);
				SendUSBAmpUpdate();
				SaveTrebleEEPROM();
			break;
		case 0x09: //Set amp parameter
				//first byte is parameter code
				switch(buf[1])
				{   //buf[2] is the parameter value
					case 0x03: //Volume
							radio->VolumeSet(buf[2]);
							SendUSBAmpUpdate();
							SaveVolumeEEPROM();
						break;
					case 0x04: //Balance
							radio->BalanceSet(buf[2]);
							SendUSBAmpUpdate();
							SaveBalanceEEPROM();
						break;
					case 0x05: //Fade
							radio->FadeSet(buf[2]);
							SendUSBAmpUpdate();
							SaveFadeEEPROM();
						break;
					case 0x06: //Bass
							radio->BassSet(buf[2]);
							SendUSBAmpUpdate();
							SaveBassEEPROM();
						break;
					case 0x07: //Mid
							radio->MidSet(buf[2]);
							SendUSBAmpUpdate();
							SaveMidEEPROM();
						break;
					case 0x08: //Treble
							radio->TrebleSet(buf[2]);
							SendUSBAmpUpdate();
							SaveTrebleEEPROM();
						break;
					default:
						break;
				}
			break;
		case 0x0A: //FM Radio command
			#ifdef _SI4703FM_ENABLED
			switch(buf[1]) //next byte is which command
			{
				case 0x01: //Seek Up
				{
					uint8_t channel = Si4703Radio.seekUp();
					char sendMessage[64] = {}; //declare and init a buffer
					sendMessage[0] = 0x03; //total length of this data packet
					//2nd byte is a command reference
					sendMessage[1] = 0x03; //0x03 means this is a FM Radio channel update
					sendMessage[2] = channel;
					//send to PC, allow 200ms for sending the frame
					RawHID.send(sendMessage, 200);
					break;
				}
				case 0x02: //Seek Down
				{
					uint8_t channel = Si4703Radio.seekDown();
					char sendMessage[64] = {}; //declare and init a buffer
					sendMessage[0] = 0x03; //total length of this data packet
					//2nd byte is a command reference
					sendMessage[1] = 0x03; //0x03 means this is a FM Radio channel update
					sendMessage[2] = channel;
					//send to PC, allow 200ms for sending the frame
					RawHID.send(sendMessage, 200);
					break;
				}
				case 0x03: //Set volume (next bit is our volume level to set can be from 0-15)
					Si4703Radio.setVolume(buf[2]);
					break;
				case 0x04: //Set channel (next bit is our channel to set can be from 1-102 (USA FM Radio))
				{
					////Freq (MHz) = 0.2 x Channel + 87.5 MHz.
					//Channel 1 = 87.5
					Si4703Radio.setChannel(buf[2]);
					//char sendMessage[64] = {}; //declare and init a buffer
					//sendMessage[0] = 0x03; //total length of this data packet
					//2nd byte is a command reference
					//sendMessage[1] = 0x03; //0x03 means this is a FM Radio channel update
					//sendMessage[2] = buf[2];
					//send to PC, allow 200ms for sending the frame
					//RawHID.send(sendMessage, 200);
					break;
				}
				case 0x05: //Read RDS data
				{
					char rdsBuffer[10];
					Si4703Radio.readRDS(rdsBuffer, 1000);
					char sendMessage[64] = {}; //declare and init a buffer
					sendMessage[0] = 0x0C; //total length of this data packet
					//2nd byte is a command reference
					sendMessage[1] = 0x04; //0x04 means this is a FM Radio RDS update
					sendMessage[2] = rdsBuffer[0];
					sendMessage[3] = rdsBuffer[1];
					sendMessage[4] = rdsBuffer[2];
					sendMessage[5] = rdsBuffer[3];
					sendMessage[6] = rdsBuffer[4];
					sendMessage[7] = rdsBuffer[5];
					sendMessage[8] = rdsBuffer[6];
					sendMessage[9] = rdsBuffer[7];
					sendMessage[10] = rdsBuffer[8];
					sendMessage[11] = rdsBuffer[9];

					//send to PC, allow 200ms for sending the frame
					RawHID.send(sendMessage, 200);
					break;
				}
				case 0x06: //Read RSSI level, stereo status, rds status, channel number
				{
					byte rssi=254;
					byte stereo=254;
					byte rds=254;
					byte channel=0;
					Si4703Radio.getStatus(&rssi, &stereo, &rds,&channel);
					char sendMessage[64] = {}; //declare and init a buffer
					sendMessage[0] = 0x06; //total length of this data packet
					//2nd byte is a command reference
					sendMessage[1] = 0x06; //0x06 means this is a FM Radio RSSI update
					sendMessage[2] = rssi;
					sendMessage[3] = stereo;
					sendMessage[4] = rds;
					sendMessage[5] = channel;
					//send to PC, allow 200ms for sending the frame
					RawHID.send(sendMessage, 200);
					if(rds == 1)
					{
						char rdsBuffer[10];
						Si4703Radio.readRDS(rdsBuffer, 1000);
						char sendMessage[64] = {}; //declare and init a buffer
						sendMessage[0] = 0x0C; //total length of this data packet
						//2nd byte is a command reference
						sendMessage[1] = 0x04; //0x04 means this is a FM Radio RDS update
						sendMessage[2] = rdsBuffer[0];
						sendMessage[3] = rdsBuffer[1];
						sendMessage[4] = rdsBuffer[2];
						sendMessage[5] = rdsBuffer[3];
						sendMessage[6] = rdsBuffer[4];
						sendMessage[7] = rdsBuffer[5];
						sendMessage[8] = rdsBuffer[6];
						sendMessage[9] = rdsBuffer[7];
						sendMessage[10] = rdsBuffer[8];
						sendMessage[11] = rdsBuffer[9];

						//send to PC, allow 200ms for sending the frame
						RawHID.send(sendMessage, 200);
					}
					break;
				}
				case 0x07: //Set Radio power on/off (next bit is our on/off flag 0=off 1=on)
				{
					if(buf[2] == 0)
						Si4703Radio.powerOff();
					if(buf[2] == 1)
					{
						Si4703Radio.powerOn();
						Si4703Radio.setVolume(15);
					}
					break;
				}
				case 0x08: //Get Radio power status (next bit is our on/off flag 0=off 1=on)
				{
					char sendMessage[64] = {}; //declare and init a buffer
					sendMessage[0] = 0x03; //total length of this data packet
					//2nd byte is a command reference
					sendMessage[1] = 0x05; //0x05 means this is a FM Radio power update
					sendMessage[2] = Si4703Radio.getPowerStatus();
					//send to PC, allow 200ms for sending the frame
					RawHID.send(sendMessage, 200);
					break;
				}
				default:
					break;
			}
			#endif
			break;
		default:
			break;
		}//END SWITCH
	}//END USB BYTES AVALIABLE

} //END LOOP


void SendUSBAmpUpdate()
{
	uint8_t sendMessage[64] = {}; //declare and init a buffer
	sendMessage[0] = 0x0A; //total length of this data packet
	//2nd byte is a command reference
	sendMessage[1] = 0x02; //0x02 means this is a AMP settings update
	sendMessage[2] = radio->status._volume;
	sendMessage[3] = radio->status._balance;
	sendMessage[4] = radio->status._fade;
	sendMessage[5] = radio->status._bass;
	sendMessage[6] = radio->status._mid;
	sendMessage[7] = radio->status._treble;
	sendMessage[8] = radio->status._radioMode;
	sendMessage[9] = radio->status.SWCButtons;

	//send to PC, allow 200ms for sending the frame
	RawHID.send(sendMessage, 200);
}
//void (*SendUSBAmpUpdateCallbackPointer)() = &SendUSBAmpUpdate;

void SaveVolumeEEPROM()
{
	EEPROM.write(0, radio->status._volume);
}

void SaveBalanceEEPROM()
{
	EEPROM.write(1, radio->status._balance);
}

void SaveFadeEEPROM()
{
	EEPROM.write(2, radio->status._fade);
}

void SaveBassEEPROM()
{
	EEPROM.write(3, radio->status._bass);
}

void SaveMidEEPROM()
{
	EEPROM.write(4, radio->status._mid);
}

void SaveTrebleEEPROM()
{
	EEPROM.write(5, radio->status._treble);
}
