#ifndef RADIOEMULATOR_H
#define RADIOEMULATOR_H

#include "Arduino.h"
#include <Metro.h>
#include <CAN.h>

#include "RadioState.h"

class RadioEmulator
{
public:
    RadioEmulator(CANClass *can, bool wdTO);
    ~RadioEmulator() {};
    
    void Operate();
    void ParseCANMessage(CAN_Frame can_MsgRx);
    virtual void ReceivedData(int status, int size, char *data);
    RadioState status;

    void WriteCANMessages(void);
    void SendStatusToHost(void);
    //void CheckCANTimeout(void);
    void CheckHostTimeout(void);

    void VolumeIncrement(int8_t increment) {status._volume += increment; if (status._volume > 38) status._volume = 38; if (status._volume < 0) status._volume = 0;}
    //All these are from 1-19 with 10 being the middle value
    void BalanceIncrement(int8_t increment) {status._balance += increment; if (status._balance > 19) status._balance = 19; if (status._balance < 1) status._balance = 1;}
    void FadeIncrement(int8_t increment) {status._fade += increment; if (status._fade > 19) status._fade = 19; if (status._fade < 1) status._fade = 1;}
    void BassIncrement(int8_t increment) {status._bass += increment; if (status._bass > 19) status._bass = 19; if (status._bass < 1) status._bass = 1;}
    void MidIncrement(int8_t increment) {status._mid += increment; if (status._mid > 19) status._mid = 19; if (status._mid < 1) status._mid = 1;}
    void TrebleIncrement(int8_t increment) {status._treble += increment; if (status._treble > 19) status._treble = 19; if (status._treble < 1) status._treble = 1;}
    //Set functions
    void VolumeSet(uint8_t value) {status._volume = value; if (status._volume > 38) status._volume = 38; if (status._volume < 0) status._volume = 0;}
	//All these are from 1-19 with 10 being the middle value
	void BalanceSet(uint8_t value) {status._balance = value; if (status._balance > 19) status._balance = 19; if (status._balance < 1) status._balance = 1;}
	void FadeSet(uint8_t value) {status._fade = value; if (status._fade > 19) status._fade = 19; if (status._fade < 1) status._fade = 1;}
	void BassSet(uint8_t value) {status._bass = value; if (status._bass > 19) status._bass = 19; if (status._bass < 1) status._bass = 1;}
	void MidSet(uint8_t value) {status._mid = value; if (status._mid > 19) status._mid = 19; if (status._mid < 1) status._mid = 1;}
	void TrebleSet(uint8_t value) {status._treble = value; if (status._treble > 19) status._treble = 19; if (status._treble < 1) status._treble = 1;}

private:
    CANClass *CANDevice;
    char siriusdata[512];
    int prevSWC;

//    void readInitFile();
//    void writeInitFile();

    void InitEvents();
    //void PowerDown();
    
    enum {standalone, slave} opMode;

    void readCANbus(void);

    void SendOnMsg();
    void SendEVICMsg();
    void SendRadioModeMsg();
    void SendStereoSettingsMsg();
    
    void ChangeSiriusStation(int station, bool turn_on);
    void ReadSiriusText(char *data);

    Metro*  CANBusTicker;//Used to trigger writes to the CAN Bus at an interval
    bool writeCANFlag;

    //std::list<CANMessage> hostMessages;
    
    Metro*  statusTicker; //Used to trigger reads from the CAN Bus at an interval
    bool statusFlag;

    //Metro*  CANTimeout; //Used to trigger a check to see if the CANBus timed out for when its sleeping!
    //bool CANTimeoutFlag;

    //void CANActivity(void);
    //bool ReceivedCANMsg;
    //bool needToParseCANMessage;
    //int powerUpIRQCounter;
    //bool sleeping;
    //bool needToWakeUp;

    Metro*  HostTimeout;  //Used to trigger a check to see if we recived any commands from our host device
    //bool hostTimeoutFlag;

    //bool ReceivedHostMsg;
    
    
    static char unlock[6];
    static char lock[6];
    static char trunk[6];
};

#endif
