#ifndef RADIOSTATE_H
#define RADIOSTATE_H

enum radioMode { AM, FM, CD, SAT, VES, MAX_MODE,AUX ,OFF};
/*
union SiriusText_u
{
    char TextLine[8][64];
    char data[512];
};
*/
struct RadioState
{
    char marker1;
    char marker2;
    char marker3;
    char marker4;
    radioMode _radioMode;
    
    int _amPreset;
    int _amFreq;
    
    int _fmPreset;
    int _fmFreq;
    
    int _cdNum;
    int _cdTrackNum;
    int _cdHours;
    int _cdMinutes;
    int _cdSeconds;
    char _cdTime[8];
    
    int _siriusPreset;
    int _siriusChan;
    
    int _evicMode;
    int _evicPreset;
    int _evicFreq;
    
    int8_t _volume;
    int8_t _balance;
    int8_t _fade;
    int8_t _bass;
    int8_t _mid;
    int8_t _treble;
    
    float _batteryVoltage;
    int _driverHeatedSeatLevel;
    int _passHeatedSeatLevel;
    char _vin[24];
    int _headlights;
    int _dimmerMode;
    int _dimmer;
    int _gear;
    int _brake;
    int _parkingBrake;
    char _vesControls[32];
    int _keyPosition;
    int _rpm;
    int _fanRequested;
    int _fanOn;
    int _rearDefrost;
    int _fuel;
    int _speed;
    int _odometer;
    int prevSWCButtons;
    int SWCButtons;
        
    int count ;
};


#endif
