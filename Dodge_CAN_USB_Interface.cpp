#include <Arduino.h>
#include <usb_rawhid.h> //to communicate with PC or Android
#include <EEPROM.h> //Use EEPROM to store last AMP settings and restore on powerup
#include <Metro.h> //To schedule events for processing at different intervals

#include <CAN.h>
#include <CAN_K2X.h>
#include <sn65hvd234.h>
#include "Dodge_CAN_USB_Interface.h"
#include "radioEmulator.h"
#include <TEA5767N.h>
//#include <CAN_SAM3X.h>
#include "ClickEncoder.h"
#include <TimerOne.h>

//Used to access a float as a byte array for use in USB communication
union {
    float fval;
    byte bval[4];
} floatAsBytes;

#define _DEBUG_ENABLED
#define _WATCHDOG_ENABLED //Enable watchdog timer code complilation
// Define our CAN speed (bitrate).
#define bitrate CAN_BPS_83K//CAN_BPS_500K

//Global variables and objects
#ifdef _WATCHDOG_ENABLED
Metro* wdtimer = new Metro(500); //schedule a timer to kick the watchdog
#endif
RadioEmulator *radio;
//Metro* usbUpdateTimer = new Metro(500); //schedule a timer to send updates over USB
//FM Radio control
TEA5767N *fmRadio;

// data counter just to show a dynamic change in data messages
uint32_t standard_counter = 0;
const int ledPin = 13;
const int powerStatusPin = 6;
uint8_t powerStatus = 0;

//USB HID Buffer
uint8_t usbmsg[64] = {};

//Rotary encoders
ClickEncoder *rightEncoder;
ClickEncoder *leftEncoder;
int16_t last, value;
int16_t last2, value2;

//Interrupt used to measure encoder positions
void timerIsr() {
  rightEncoder->service();
  leftEncoder->service();
}

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

#ifdef _DEBUG_ENABLED
void PrintBuffer(char *msg)
{
	//print everything we get from USB to serial port
	Serial1.print(msg);
	for (byte i = 0; i < 11; i++)
	{
		Serial1.print(usbmsg[i] < 16 ? "0" : ""); //print leading HEX 0 if needed
		Serial1.print(usbmsg[i], HEX);
		Serial1.print(" ");
	}
	Serial1.println("");
}
#endif

void setup()
{
	pinMode(ledPin, OUTPUT); //led
	pinMode( A9, OUTPUT ) ; //CAN Chip Enable Pin
	digitalWrite(A9, HIGH ) ; //Set CAN ENable to high (turns chip on)
	pinMode(A8, INPUT_PULLUP);
	delay(100);
	powerStatus = digitalRead(powerStatusPin);

#ifdef _DEBUG_ENABLED
//Change to alternate PINS for RX1/TX1 (pin# 5 and 21)
Serial1.setRX(21);
Serial1.setTX(5);
Serial1.begin(9600); // Initialize Serial communications with computer to use serial monitor
Serial1.print("Welcome to Teensy Dodge Interface.");
#endif
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
	radio->status._balance = 0x0a;//EEPROM.read(1);
	radio->status._fade =  0x0a;//EEPROM.read(2);
	radio->status._bass =  0x0a;//EEPROM.read(3);
	radio->status._mid =  0x0a;//EEPROM.read(4);
	radio->status._treble =  0x0a;//EEPROM.read(5);

	fmRadio = new TEA5767N();
	fmRadio->setSoftMuteOn();
	fmRadio->setHighCutControlOn();
	fmRadio->setSearchHighStopLevel();
	fmRadio->selectFrequency(96.5);

	//Sub Amp audio source selector (USB audio or AUX(FM Radio))
	pinMode(20, OUTPUT);
	digitalWrite(20, LOW); //Default to USB audio

	//Setup rotary encoder interrupt and variables
	rightEncoder = new ClickEncoder(0, 1, 8);
	leftEncoder = new ClickEncoder(22, 23, 7);
	Timer1.initialize(1000);
	Timer1.attachInterrupt(timerIsr);
	last = -1;
	last2 = -1;
}

#ifdef _WATCHDOG_ENABLED
void printResetType() {
	#ifdef _DEBUG_ENABLED
    if (RCM_SRS1 & RCM_SRS1_SACKERR)   Serial1.println("[RCM_SRS1] - Stop Mode Acknowledge Error Reset");
    if (RCM_SRS1 & RCM_SRS1_MDM_AP)    Serial1.println("[RCM_SRS1] - MDM-AP Reset");
    if (RCM_SRS1 & RCM_SRS1_SW)        Serial1.println("[RCM_SRS1] - Software Reset");
    if (RCM_SRS1 & RCM_SRS1_LOCKUP)    Serial1.println("[RCM_SRS1] - Core Lockup Event Reset");
    if (RCM_SRS0 & RCM_SRS0_POR)       Serial1.println("[RCM_SRS0] - Power-on Reset");
    if (RCM_SRS0 & RCM_SRS0_PIN)       Serial1.println("[RCM_SRS0] - External Pin Reset");
    if (RCM_SRS0 & RCM_SRS0_WDOG)      Serial1.println("[RCM_SRS0] - Watchdog(COP) Reset");
    if (RCM_SRS0 & RCM_SRS0_LOC)       Serial1.println("[RCM_SRS0] - Loss of External Clock Reset");
    if (RCM_SRS0 & RCM_SRS0_LOL)       Serial1.println("[RCM_SRS0] - Loss of Lock in PLL Reset");
    if (RCM_SRS0 & RCM_SRS0_LVD)       Serial1.println("[RCM_SRS0] - Low-voltage Detect Reset");
    #endif
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
#ifdef _DEBUG_ENABLED
printCANMessage(false,standard_message); //send message to serial debug port
#endif
}

// Create a function to read message and display it through Serial Monitor
void readCANMessage()
{

}

void printCANMessage(bool CANRecievedDirection, CAN_Frame message)
{
	if(CANRecievedDirection = true)
		Serial1.print("RXCAN:");
	else
		Serial1.print("TXCAN:");
	//Serial1.print(millis());
	//Serial1.print(F(",0x"));
	Serial1.print(message.id, HEX); //display message ID
	Serial1.print(' ');
	//Serial1.print(message.rtr); //display message RTR
	//Serial1.print(',');
	//Serial1.print(message.extended); //display message EID
	//Serial1.print(',');
	//if (message.rtr == 1)
	//{
	//  Serial1.print(F(" REMOTE REQUEST MESSAGE ")); //technically if its RTR frame/message it will not have data So display this
	//}
	//else
	//{
	  Serial1.print(message.length, HEX); //display message length
	  for (byte i = 0; i < message.length; i++)
	  {
		Serial1.print(' ');
		if (message.data[i] < 0x10) // If the data is less than 10 hex it will assign a zero to the front as leading zeros are ignored...
		{
		  Serial1.print('0');
		}
		Serial1.print(message.data[i], HEX); //display data based on length
	  }
	//}
  Serial1.println(); // adds a line
}

void printCANMessageUSBHID(CAN_Frame message)
{
	memset(usbmsg,0,sizeof(usbmsg));
	usbmsg[0] = message.length + 5; //Total length of this data packet
	//2nd byte is a command reference
	usbmsg[1] = 0x01; //USBRECEIVE_CAN 0x01 means this is a received CAN frame
	//Message ID (11bits total stored over 2 bytes)
	usbmsg[2] = (message.id >> 8);
	usbmsg[3] = (message.id & 0xFF);
	//The CAN frame data length
	usbmsg[4] = (message.length);
	//Now fill in data bytes
	for (byte i = 0; i < message.length; i++)
	{
		usbmsg[i+5] = message.data[i];
	}
	//send to PC, allow 100ms for sending the frame
	RawHID.send(usbmsg, 200);
}

void SendHID(String text)
{
	memset(usbmsg,0,sizeof(usbmsg));
	text.getBytes(usbmsg,text.length()+1); //convert text to bytes
	//send to PC, allow 100ms for sending the frame
	RawHID.send(usbmsg, 200);
}

void loop()
{
#ifdef _WATCHDOG_ENABLED
//Prevent watchdog resets
if(wdtimer->check())
		WatchdogKick();
#endif
/*
#ifdef _DEBUG_ENABLED
if (Serial1.available()>0) {
    int inByte = Serial1.read();
    if (inByte == '+'){
    	fmRadio->setSearchUp();
    	fmRadio->searchNextMuting();
    	Serial1.println();
		Serial1.print(fmRadio->readFrequencyInMHz());
		if (fmRadio->isStereo())
			Serial1.print(" Stereo ");
		else
			Serial1.print(" Mono ");
		Serial1.print(fmRadio->getSignalLevel()*10);
    }
    if (inByte == '-'){
    	fmRadio->setSearchDown();
    	fmRadio->searchNextMuting();
    	Serial1.println();
		Serial1.print(fmRadio->readFrequencyInMHz());
		if (fmRadio->isStereo())
			Serial1.print(" Stereo ");
		else
			Serial1.print(" Mono ");
		Serial1.print(fmRadio->getSignalLevel()*10);
    }
    if (inByte == 'm')
        fmRadio->mute();
    if (inByte == 'u')
            fmRadio->turnTheSoundBackOn();
    if (inByte == 'r'){
        if(digitalRead(20) == HIGH)
          digitalWrite(20, LOW);
         else
          digitalWrite(20, HIGH);
    }
    if (inByte == 'a')
            fmRadio->setStereoNoiseCancellingOn();
    if (inByte == 's')
            fmRadio->setStereoNoiseCancellingOff();
    if (inByte == 'z')
                fmRadio->setStereoReception();
        if (inByte == 'x')
                fmRadio->setMonoReception();
    if (inByte == 'd'){
    	//Run FM Radio process loop
    	Serial1.println();
    	Serial1.print(fmRadio->readFrequencyInMHz());
		if (fmRadio->isStereo())
			Serial1.print(" Stereo ");
		else
			Serial1.print(" Mono ");
		Serial1.print(fmRadio->getSignalLevel()*10);
    }
}
#endif


	//Check status of powerStatusPin (switched from supply power to monitor when external power is about to turn off
	if (powerStatus != digitalRead(powerStatusPin)) //If pin state has changed
	{
		powerStatus = digitalRead(powerStatusPin); //save new pinstate
		SendUSBTeensyUpdate(); //send this new pin state to android over USB
	}

//	{ // powerStatusPin  is high due to pullup resistor (supply power will go off in about 5 secs this happens)
//	} else { // powerStatusPin is low due to supply power being on
//	}

	CAN_Frame message; // Create message object to use CAN message structure
	message.valid = false; //init to false
	if (CAN.available() == true) // Check to see if a valid message has been received.
	{
		message = CAN.read(); //read message, it will follow the CAN structure of ID,RTR, legnth, data. Allows both Extended or Standard
		printCANMessageUSBHID(message);
		#ifdef _DEBUG_ENABLED
		printCANMessage(true,message); //send message to serial debug port
		#endif
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
*/

	//Calculate rotary encoder positions---------------------------------------------------------------------
	value += rightEncoder->getValue();
	value2 += leftEncoder->getValue();
	if (value > last) {
		fmRadio->selectFrequency(fmRadio->readFrequencyInMHz() + (abs(value-last) * 0.2));
		SendUSBFMData(0x06,(uint8_t)fmRadio->getSignalLevel()*10,(uint8_t)fmRadio->isStereo(),0,(uint8_t)round(((fmRadio->readFrequencyInMHz() - 87.5)/0.2)));
	    Serial1.print((value-last));
	    last = value;
	    Serial1.print(" RightEncoder Value: ");
	    Serial1.println(value);
	} else if (value < last) {
		fmRadio->selectFrequency(fmRadio->readFrequencyInMHz() - (abs(value-last) * 0.2));
		SendUSBFMData(0x06,(uint8_t)fmRadio->getSignalLevel()*10,(uint8_t)fmRadio->isStereo(),0,(uint8_t)round(((fmRadio->readFrequencyInMHz() - 87.5)/0.2)));
	    Serial1.print((value-last));
	    last = value;
	    Serial1.print(" RightEncoder Value: ");
	    Serial1.println(value);
	}

	if (value2 > last2) {
		SendUSBFMData(0x08,abs(value2-last2),0);
		Serial1.print((value2-last2));
		last2 = value2;
		Serial1.print(" LeftEncoder Value: ");
		Serial1.println(value2);
	} else if (value2 < last2) {
		SendUSBFMData(0x08,abs(value2-last2)*-1,0);
		Serial1.print((value2-last2));
	    last2 = value2;
	    Serial1.print(" LeftEncoder Value: ");
	    Serial1.println(value2);
	}

	ClickEncoder::Button a = rightEncoder->getButton();
	if (a != ClickEncoder::Open) {
	Serial1.print("Button A: ");
	#define VERBOSECASE(label) case label: Serial1.println(#label); break;
	switch (a) {
		VERBOSECASE(ClickEncoder::Pressed);
		VERBOSECASE(ClickEncoder::Held)
		VERBOSECASE(ClickEncoder::Released)
		case ClickEncoder::Clicked:
			Serial1.println("ClickEncoder::Clicked");
		break;
		case ClickEncoder::DoubleClicked:
			Serial1.println("ClickEncoder::DoubleClicked");
			rightEncoder->setAccelerationEnabled(!rightEncoder->getAccelerationEnabled());
			Serial1.print("  Acceleration is ");
			Serial1.println((rightEncoder->getAccelerationEnabled()) ? "enabled" : "disabled");
		break;
	}
	}

	ClickEncoder::Button b = leftEncoder->getButton();
	if (b != ClickEncoder::Open) {
	Serial1.print("Button B: ");
	#define VERBOSECASE(label) case label: Serial1.println(#label); break;
	switch (b) {
		VERBOSECASE(ClickEncoder::Pressed);
		VERBOSECASE(ClickEncoder::Held)
		VERBOSECASE(ClickEncoder::Released)
		case ClickEncoder::Clicked: //mute/unmute
			Serial1.println("ClickEncoder::Clicked");
			if(fmRadio->isMuted())
			{
				SendUSBFMData(0x08,0,0);
				fmRadio->turnTheSoundBackOn();
				PreAMPAUXOn();
			} else {
				SendUSBFMData(0x08,0,1);
				fmRadio->mute();
				PreAMPAUXOff();
			}
		break;
		case ClickEncoder::DoubleClicked:
			Serial1.println("ClickEncoder::DoubleClicked");
			leftEncoder->setAccelerationEnabled(!leftEncoder->getAccelerationEnabled());
			Serial1.print("  Acceleration is ");
			Serial1.println((leftEncoder->getAccelerationEnabled()) ? "enabled" : "disabled");
		break;
	}
	}

	//USB HID------------------------------------------------------------------------
	int bytesAvaliable = RawHID.available();
	if(bytesAvaliable > 0) //we got something over USB!
	{
		memset(usbmsg,0,sizeof(usbmsg));
		RawHID.recv(usbmsg,100);

		#ifdef _DEBUG_ENABLED
		PrintBuffer("RXUSB:");
		#endif
		//1st byte is a command reference a 0x01 is a CAN FRAME
		switch(usbmsg[0])
		{
		case 0x01: //Send CAN Frame Command
			//2nd byte and 3rd byte are the CAN ID
			//4th byte is CAN data length
			//4th - 12 are CAN data bytes
			CAN_Frame msg; // Create message object to use CAN message structure
			//these 2 lines below combine 2 8bit variables into one 16bit variable (although only 11 bits are used for CAN IDs)
			msg.id = (usbmsg[1]<<8); //shift first byte left 8 bits
			msg.id = (msg.id | usbmsg[2]); //OR in 2nd byte into right 8 bits
			msg.valid = true;
			msg.rtr = 0;
			msg.extended = CAN_STANDARD_FRAME;
			msg.length = usbmsg[3]; // Data length
			// Load the data directly into CAN_Frame
			//Now fill in data bytes
			for (byte i = 0; i < msg.length; i++)
			{
				msg.data[i] = usbmsg[i+4];
			}
			CAN.write(msg); // Load message and send
			break;
		case 0x02: //Start infinite loop, should trigger a watchdog reset
			while(1);
			break;
		case 0x03: //Volume increment
				//first byte is signed increment value
				radio->VolumeIncrement((int8_t)usbmsg[1]);
				SendUSBAmpUpdate();
				//SaveVolumeEEPROM();
			break;
		case 0x04: //Balance increment
				//first byte is signed increment value
				radio->BalanceIncrement((int8_t)usbmsg[1]);
				SendUSBAmpUpdate();
				SaveBalanceEEPROM();
			break;
		case 0x05: //Fade increment
				//first byte is signed increment value
				radio->FadeIncrement((int8_t)usbmsg[1]);
				SendUSBAmpUpdate();
				SaveFadeEEPROM();
			break;
		case 0x06: //Bass increment
				//first byte is signed increment value
				radio->BassIncrement((int8_t)usbmsg[1]);
				SendUSBAmpUpdate();
				SaveBassEEPROM();
			break;
		case 0x07: //Mid increment
				//first byte is signed increment value
				radio->MidIncrement((int8_t)usbmsg[1]);
				SendUSBAmpUpdate();
				SaveMidEEPROM();
			break;
		case 0x08: //Treble increment
				//first byte is signed increment value
				radio->TrebleIncrement((int8_t)usbmsg[1]);
				SendUSBAmpUpdate();
				SaveTrebleEEPROM();
			break;
		case 0x09: //Set amp parameter
				//first byte is parameter code
				switch(usbmsg[1])
				{   //usbmsg[2] is the parameter value
					case 0x03: //Volume
							radio->VolumeSet(usbmsg[2]);
							SendUSBAmpUpdate();
							SaveVolumeEEPROM();
						break;
					case 0x04: //Balance
							radio->BalanceSet(usbmsg[2]);
							SendUSBAmpUpdate();
							SaveBalanceEEPROM();
						break;
					case 0x05: //Fade
							radio->FadeSet(usbmsg[2]);
							SendUSBAmpUpdate();
							SaveFadeEEPROM();
						break;
					case 0x06: //Bass
							radio->BassSet(usbmsg[2]);
							SendUSBAmpUpdate();
							SaveBassEEPROM();
						break;
					case 0x07: //Mid
							radio->MidSet(usbmsg[2]);
							SendUSBAmpUpdate();
							SaveMidEEPROM();
						break;
					case 0x08: //Treble
							radio->TrebleSet(usbmsg[2]);
							SendUSBAmpUpdate();
							SaveTrebleEEPROM();
						break;
					default:
						break;
				}
			break;
		case 0x0A: //FM Radio command
			//first byte is parameter code
			switch(usbmsg[1])
			{
				case 0x01: //USB_FM_SEEK_UP
					fmRadio->setSearchMidStopLevel();
					fmRadio->setSearchUp();
					fmRadio->searchNextMuting();
					SendUSBFMData(0x03, (uint8_t)round(((fmRadio->readFrequencyInMHz() - 87.5)/0.2)), 0);
					break;
				case 0x02: //USB_FM_SEEK_DOWN
					fmRadio->setSearchMidStopLevel();
					fmRadio->setSearchDown();
					fmRadio->searchNextMuting();
					SendUSBFMData(0x03, (uint8_t)round(((fmRadio->readFrequencyInMHz() - 87.5)/0.2)), 0);
					break;
				case 0x03: //USB_FM_SET_VOLUME
					break;
				case 0x04: //USB_FM_SET_CHANNEL
					Serial1.print((usbmsg[2] * 0.2) + 87.50);
					fmRadio->selectFrequency((usbmsg[2] * 0.2) + 87.50);
					SendUSBFMData(0x03, (uint8_t)round(((fmRadio->readFrequencyInMHz() - 87.5)/0.2)), 0);
					break;
				case 0x05: //USB_FM_GET_RDS
					break;
				case 0x06: //USB_FM_GET_RSSI
					SendUSBFMData(0x06,(uint8_t)fmRadio->getSignalLevel()*10,(uint8_t)fmRadio->isStereo(),0,(uint8_t)round(((fmRadio->readFrequencyInMHz() - 87.5)/0.2)));
					break;
				case 0x07: //USB_FM_SET_POWER
					switch(usbmsg[2])
					{
						case 0x00: //off
							fmRadio->mute();
							PreAMPAUXOff();
						break;
						case 0x01: //on
							fmRadio->turnTheSoundBackOn();
							PreAMPAUXOn();
						break;
					}
					break;
				case 0x08: //USB_FM_GET_POWER
					break;

				default:
					break;
			}
		break;


		default:
			break;
		}//END SWITCH
	}//END USB BYTES AVALIABLE

} //END LOOP


void SendUSBAmpUpdate()
{
	memset(usbmsg,0,sizeof(usbmsg));
	usbmsg[0] = 0x0A; //total length of this data packet
	//2nd byte is a command reference
	usbmsg[1] = 0x02; //USBRECEIVE_AMP 0x02 means this is a AMP settings update
	usbmsg[2] = radio->status._volume;
	usbmsg[3] = radio->status._balance;
	usbmsg[4] = radio->status._fade;
	usbmsg[5] = radio->status._bass;
	usbmsg[6] = radio->status._mid;
	usbmsg[7] = radio->status._treble;
	usbmsg[8] = radio->status._radioMode;
	usbmsg[9] = radio->status.SWCButtons;

	//send to PC, allow 200ms for sending the frame
	RawHID.send(usbmsg, 200);
	#ifdef _DEBUG_ENABLED
	PrintBuffer("TXUSB:");
	#endif
}

void SendUSBTeensyUpdate()
{
	memset(usbmsg,0,sizeof(usbmsg));
	usbmsg[0] = 0x03; //total length of this data packet
	//2nd byte is a command reference
	usbmsg[1] = 0x07; //USBRECEIVE_TEENSY_STATUS 0x07 means this is a Teensy status update
	usbmsg[2] = powerStatus;

	//send to PC, allow 200ms for sending the frame
	RawHID.send(usbmsg, 200);
	#ifdef _DEBUG_ENABLED
	PrintBuffer("TXUSB:");
	#endif
}

void SendUSBFMData(uint8_t cmd, uint8_t arg1,  uint8_t arg2)
{
	memset(usbmsg,0,sizeof(usbmsg));
	usbmsg[0] = 0x04; //total length of this data packet
	//2nd byte is a command reference
	usbmsg[1] = cmd;
	usbmsg[2] = arg1;
	usbmsg[3] = arg2;
	//send to PC, allow 200ms for sending the frame
	RawHID.send(usbmsg, 200);
	#ifdef _DEBUG_ENABLED
	PrintBuffer("TXUSB:");
	#endif
}

void SendUSBFMData(uint8_t cmd, uint8_t arg1,  uint8_t arg2,uint8_t arg3)
{
	memset(usbmsg,0,sizeof(usbmsg));
	usbmsg[0] = 0x05; //total length of this data packet
	//2nd byte is a command reference
	usbmsg[1] = cmd;
	usbmsg[2] = arg1;
	usbmsg[3] = arg2;
	usbmsg[4] = arg3;
	//send to PC, allow 200ms for sending the frame
	RawHID.send(usbmsg, 200);
	#ifdef _DEBUG_ENABLED
	PrintBuffer("TXUSB:");
	#endif
}

void SendUSBFMData(uint8_t cmd, uint8_t arg1,  uint8_t arg2,uint8_t arg3,uint8_t arg4)
{
	memset(usbmsg,0,sizeof(usbmsg));
	usbmsg[0] = 0x06; //total length of this data packet
	//2nd byte is a command reference
	usbmsg[1] = cmd;
	usbmsg[2] = arg1;
	usbmsg[3] = arg2;
	usbmsg[4] = arg3;
	usbmsg[5] = arg4;
	//send to PC, allow 200ms for sending the frame
	RawHID.send(usbmsg, 200);
	#ifdef _DEBUG_ENABLED
	PrintBuffer("TXUSB:");
	#endif
}

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

//Turn pre-amp aux audio on (fm radio audio)
void PreAMPAUXOn(){
	digitalWrite(20, HIGH);
}

//Turn pre-amp aux audio off (fm radio audio)
void PreAMPAUXOff(){
	digitalWrite(20, LOW);
}
