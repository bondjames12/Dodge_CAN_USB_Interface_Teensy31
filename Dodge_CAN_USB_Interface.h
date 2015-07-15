// Only modify this file to include
// - function definitions (prototypes)
// - include files
// - extern variable definitions
// In the appropriate section

#ifndef _Dodge_CAN_USB_Interface_H_
#define _Dodge_CAN_USB_Interface_H_
#include "Arduino.h"
//add your includes for the project Dodge_CAN_USB_Interface_Teensy31 here


//end of add your includes here
#ifdef __cplusplus

/* Return type (8/16/32/64 int size) is specified by argument size. */
template<class TYPE> inline TYPE BIT(const TYPE & x)
{ return TYPE(1) << x; }

template<class TYPE> inline bool IsBitSet(const TYPE & x, const TYPE & y)
{ return 0 != (x & y); }
/*Usage:
//    IsBitSet( foo, BIT(3) | BIT(6) );  // Checks if Bit 3 OR 6 is set.
benefits to using these CPP templates to check for set bits is
Amongst other things, this approach:

Accommodates 8/16/32/64 bit integers.
Detects IsBitSet(int32,int64) calls without my knowledge & consent.
Inlined Template, so no function calling overhead.
const& references, so nothing needs to be duplicated/copied. And we are guaranteed that the compiler will pick up any typo's that attempt to change the arguments.
0!= makes the code more clear & obvious. The primary point to writing code is always to communicate clearly and efficiently with other programmers, including those of lesser skill.
While not applicable to this particular case... In general, templated functions avoid the issue of evaluating arguments multiple times. A known problem with some #define macros.
E.g.: #define ABS(X) (((X)<0) ? - (X) : (X))
      ABS(i++);
*/
extern "C" {
#endif
void loop();
void setup();
#ifdef __cplusplus
} // extern "C"
#endif

//add your function definitions for the project Dodge_CAN_USB_Interface_Teensy31 here
void readCANMessage();
void sendCANMessage();
void printCANMessage(CAN_Frame msg);
void printCANMessageUSBHID(CAN_Frame message);
void SendUSBAmpUpdate();
void SaveVolumeEEPROM();
void SaveBalanceEEPROM();
void SaveFadeEEPROM();
void SaveBassEEPROM();
void SaveMidEEPROM();
void SaveTrebleEEPROM();

//Do not add code below this line
#endif /* _Dodge_CAN_USB_Interface_Teensy31_H_ */
