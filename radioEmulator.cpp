#include "Arduino.h"
#include <Metro.h>
#include <CAN.h>

#include "radioEmulator.h"

//DigitalOut led1(LED1);
bool led2;
//DigitalOut led3(LED3);
//DigitalOut led4(LED4);
//DigitalOut reverse(p15);

#undef CHECK_HW_SHUTDOWN

//LocalFileSystem local("local");
//#include "SDFileSystem.h"

//SDFileSystem sd(p5, p6, p7, p8, "sd"); // the pinout on the mbed Cool Components workshop board

char RadioEmulator::unlock[6] = {0x03,0x02,0x00,0x40,0x87,0xa5};
char RadioEmulator::lock[6] =   {0x01, 0x02, 0x00, 0x40, 0x87, 0xa5};
char RadioEmulator::trunk[6] =  {0x05, 0x02, 0x00, 0x40, 0x87, 0xa5};

RadioEmulator::RadioEmulator(CANClass *can, bool wdTO)
{
    //printf("RadioEmulator Initializing\r\n");
    led2 = false;
    //HostSock = new UDPSock(new Host(IpAddr(), 50000, NULL), 64, this);

    CANDevice = can;
    //can_RS_Pin = rs;
    //can_IRQ_Pin = irq;

    prevSWC = 0;
    
    memset(&status, 0, sizeof(status));  
    memset(&siriusdata, 0, 512);  
    status.marker1 = 0x42;  
    status.marker2 = 0x42;  
    status.marker3 = 0x42;  
    status.marker4 = 0x42;  

    status._radioMode = AUX;

//    readInitFile();
    status._volume = 10;
    status._bass = 10;
    status._mid = 10;
    status._treble = 10;
    status._balance = 10;
    status._fade = 10;

    for (int i = 0; i < 8; i++)
    {
        if (wdTO)
        {
            sprintf(&siriusdata[i * 64], "WATCH DOG TIMED OUT");
        }
        else
        {
            sprintf(&siriusdata[i * 64], "Fun line text # %d", i);
        }
    }

    InitEvents();
    
    //printf("RadioEmulator initialized\n\r");
}

/*
void RadioEmulator::readInitFile()
{
    FILE *fp = fopen("/sd/stereo.txt", "r");  // Open "out.txt" on the local file system for writing
    char temp[100];
        
    while ( fscanf(fp, "%s", temp) > 0)
    {
        if (strcmp(temp, "volume") == 0)
        {
            fscanf(fp, "%d", &status._volume);
        }
        if (strcmp(temp, "bass") == 0)
        {
            fscanf(fp, "%d", &status._bass);
        }
        if (strcmp(temp, "mid") == 0)
        {
            fscanf(fp, "%d", &status._mid);
        }
        if (strcmp(temp, "treble") == 0)
        {
            fscanf(fp, "%d", &status._treble);
        }
        if (strcmp(temp, "balance") == 0)
        {
            fscanf(fp, "%d", &status._balance);
        }
        if (strcmp(temp, "fade") == 0)
        {
            fscanf(fp, "%d", &status._fade);
        }
        if (strcmp(temp, "MAC") == 0)
        {
            char temp2[64];
            fscanf(fp, "%s", temp2);
            char *pEnd;
            hostMACAddress[0] = strtoul(temp2, &pEnd, 16);
            hostMACAddress[1] = strtoul(pEnd, &pEnd, 16);
            hostMACAddress[2] = strtoul(pEnd, &pEnd, 16);
            hostMACAddress[3] = strtoul(pEnd, &pEnd, 16);
            hostMACAddress[4] = strtoul(pEnd, &pEnd, 16);
            hostMACAddress[5] = strtoul(pEnd, &pEnd, 16);
        }
    }
    
    fclose(fp);
}

void RadioEmulator::writeInitFile()
{
    FILE *fp = fopen("/sd/stereo.txt", "w");  // Open "out.txt" on the local file system for writing

    fprintf(fp,"volume %d\r\n", status._volume);
    fprintf(fp,"bass %d\r\n", status._bass);
    fprintf(fp,"mid %d\r\n", status._mid);
    fprintf(fp,"treble %d\r\n", status._treble);
    fprintf(fp,"balance %d\r\n", status._balance);
    fprintf(fp,"fade %d\r\n", status._fade);
    fclose(fp);
}
*/


void RadioEmulator::InitEvents()
{
    //needToParseCANMessage = false;
    //ReceivedCANMsg = false;
    // Wake up the CAN Transceiver
    
    //sleeping = false;

    writeCANFlag = false;
    //CANBusTicker.attach(this, &RadioEmulator::WriteCANMessages, 1);
    //CANBusTicker.begin(isr, 150000);  // blinkLED to run every 0.15 seconds
    CANBusTicker = new Metro(1000);

    //ChangeSiriusStation(status._siriusChan, true);

    //ReceivedHostMsg = false;
    statusFlag = false;
    //statusTicker.attach(this, &RadioEmulator::SendStatusToHost, 0.1);
    statusTicker = new Metro(100);

    opMode = standalone;
    //hostTimeoutFlag = false;
    //HostTimeout.attach(this, &RadioEmulator::CheckHostTimeout, 1);
        
    //CANTimeoutFlag = false;
    //canIRQ->rise(0);
// only enable this if trying to power up/down the processor
//    CANTimeout.attach(this, &RadioEmulator::CheckCANTimeout, 1);
}

void RadioEmulator::Operate()
{
	//If CANBusTicker metro timer is overdue then prepare all the CAN Messages and transmit them
    if (CANBusTicker->check())
    {
        //writeCANFlag = false;
        //Flip LED state
        led2 = !led2;
        if(led2)
        	digitalWrite(13, HIGH);
        else
        	digitalWrite(13, LOW);
        SendOnMsg();
        SendRadioModeMsg();
        SendEVICMsg();
        SendStereoSettingsMsg();
    }//END CANBusTicker



}

void RadioEmulator::SendOnMsg()
{
	CAN_Frame msg;
    msg.id = 0x416;
    msg.length = 8;
    msg.valid = 1;
    msg.timeout = 100;
    char temp[8] = {0xfd, 0x1c, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff};
    memcpy(msg.data, temp, 8);
    CANDevice->write(msg);
}

void RadioEmulator::SendRadioModeMsg()
{
	CAN_Frame msg;
    msg.id = 0x09F;
    msg.length = 8;
    msg.valid = 1;
    msg.timeout = 100;
    msg.data[7] = 0xc1;
    msg.data[6] = 0x00;
    msg.data[5] = 0x00;
    msg.data[4] = 0x00;
    msg.data[3] = 0x00;
    msg.data[2] = 0x00;
    msg.data[1] = 0x00;
    msg.data[0] = 0x0b;

    if (status._radioMode == AM)
    {
        if ((status._amPreset >= 0) && (status._amPreset < 16))
        {
            msg.data[0] = (status._amPreset + 1) << 4;
        }
        msg.data[1] = (status._amFreq & 0xFF00) >> 8;
        msg.data[2] = (status._amFreq & 0x00FF);
    }
    else if (status._radioMode == FM)
    {
        if ((status._fmPreset >= 0) && (status._fmPreset < 16))
        {
            msg.data[0] = (status._fmPreset + 1) << 4;
        }
        msg.data[0] |= 0x01;
        msg.data[1] = (status._fmFreq & 0xFF00) >> 8;
        msg.data[2] = (status._fmFreq & 0x00FF);
    }
    else if (status._radioMode == CD)
    {
        msg.data[0] = status._cdNum << 4;
        msg.data[1] = 0x20;
        msg.data[0] |= 0x03;
        msg.data[2] = status._cdTrackNum;
        msg.data[4] = status._cdHours;
        msg.data[5] = status._cdMinutes;
        msg.data[6] = status._cdSeconds;
    }
    else if (status._radioMode == SAT)
    {
        if ((status._siriusPreset >= 0) && (status._siriusPreset < 16))
        {
            msg.data[0] = (status._siriusPreset + 1) << 4;
        }
        msg.data[0] |= 0x04;
        msg.data[1] = 0;
        msg.data[2] = status._siriusChan;
    }
    else if (status._radioMode == VES)
    {
        msg.data[0] = 0x16;
        msg.data[1] = 0x10;
        msg.data[2] = 0x01;
    }
    else if (status._radioMode == AUX)
    {
        msg.data[0] = 0x0B;
        msg.data[1] = 0x00;
        msg.data[2] = 0x00;
        msg.data[3] = 0x00;
        msg.data[4] = 0x00;
        msg.data[5] = 0x00;
        msg.data[6] = 0x00;
        msg.data[7] = 0xC1;
    }
    else if (status._radioMode == OFF)
        {
            msg.data[0] = 0x07;
            msg.data[1] = 0x00;
            msg.data[2] = 0x00;
            msg.data[3] = 0x00;
            msg.data[4] = 0x00;
            msg.data[5] = 0x00;
            msg.data[6] = 0x00;
            msg.data[7] = 0xC1;
        }

    msg.data[1] |= 0x10;

    CANDevice->write(msg);
}

void RadioEmulator::SendEVICMsg()
{
	CAN_Frame msg;
    msg.id = 0x394;
    msg.length = 6;
    msg.valid = 1;
    msg.timeout = 100;
    memset(msg.data, 0, 8);

    if (status._radioMode == AM)
    {
        if ((status._amPreset >= 0) && (status._amPreset < 16))
        {
            msg.data[0] = (status._amPreset + 1) << 4;
        }
        msg.data[1] = (status._amFreq & 0xFF00) >> 8;
        msg.data[2] = (status._amFreq & 0x00FF);
    }
    else
    {
        if ((status._fmPreset >= 0) && (status._fmPreset < 16))
        {
            msg.data[0] = (status._fmPreset + 1) << 4;
        }
        msg.data[0] |= 0x01;
        msg.data[1] = (status._fmFreq & 0xFF00) >> 8;
        msg.data[2] = (status._fmFreq & 0x00FF);
    }

    CANDevice->write(msg);
}

void RadioEmulator::SendStereoSettingsMsg()
{
	CAN_Frame msg;
    msg.id = 0x3D0;
    msg.length = 7;
    msg.valid = 1;
    msg.timeout = 100;
    msg.data[0] = status._volume;
    msg.data[1] = status._balance;
    msg.data[2] = status._fade;
    msg.data[3] = status._bass;
    msg.data[4] = status._mid;
    msg.data[5] = status._treble;

    //if (status._radioMode == OFF) //if our mode is set to off then volume is zero
    //{
    //	msg.data[0] = 0x0;
    //}

    CANDevice->write(msg);
}

void RadioEmulator::ChangeSiriusStation(int station, bool turn_on)
{
    if (station == 0)
    {
        return;
    }
    
    CAN_Frame msg;
    msg.id = 0x3B0;
    msg.length = 6;
    msg.valid = 1;
    msg.timeout = 100;
    if (turn_on)
    {
        msg.data[0] = 21;
    }
    else
    {
        msg.data[0] = 23;
    }
    msg.data[1] = station;

    CANDevice->write(msg);

    memset(msg.data, 0, 8);
    msg.data[1] = station;
    CANDevice->write(msg);
    
    status._siriusChan = station;

    memset(&siriusdata, 0, 512);  
}

void RadioEmulator::ParseCANMessage(CAN_Frame can_MsgRx)
{
	switch(can_MsgRx.id)
	{
	case 0x000:
		/*
		if (can_MsgRx.data[0] > 1)
		{
			radioOn = true;
		}
		else
		{
			radioOn = false;
		}
		*/
		status._keyPosition = can_MsgRx.data[0];
		break;
	case 0x002:
		status._rpm = (can_MsgRx.data[0] << 8) + can_MsgRx.data[1];
		status._speed = ((can_MsgRx.data[2] << 8) + can_MsgRx.data[3]) >> 7;
		// what are the other 4 bytes?
		break;
	case 0x003:
		status._brake = can_MsgRx.data[3] & 0x01;
		status._gear = can_MsgRx.data[4];
		if (status._gear == 'R')
		{
			//reverse = 1;
		}
		else
		{
			//reverse = 0;
		}
		break;
	case 0x012:
		if (memcmp(can_MsgRx.data, unlock, 6) == 0) //memory compare 0=thesame
		{
		}
		else if (memcmp(can_MsgRx.data, lock, 6) == 0)
		{
		}
		else if (memcmp(can_MsgRx.data, trunk, 6) == 0)
		{
		}
		break;
	case 0x014:
		status._odometer = (can_MsgRx.data[0] << 16) + (can_MsgRx.data[1] << 8) + can_MsgRx.data[2];
		// what are the other 4 bytes?
		break;
	case 0x015:
		status._batteryVoltage = (float)(can_MsgRx.data[1]) / 10;
		break;
	case 0x01b:
	{
		// vin number
		int part = can_MsgRx.data[0];
		if ((part >= 0) && (part < 3))
		{
			for (int i = 1; i < 8; i++)
			{
				status._vin[(part*7) + i-1] = can_MsgRx.data[i];
			}
		}
		break;
	}
	case 0x0d0:
		if (can_MsgRx.data[0] == 0x80)
		{
			status._parkingBrake = true;
		}
		else
		{
			status._parkingBrake = false;
		}
		break;
	case 0x0EC:
		if ((can_MsgRx.data[0] & 0x40) == 0x40)
		{
			status._fanRequested = true;
		}
		else
		{
			status._fanRequested = false;
		}

		if ((can_MsgRx.data[0] & 0x01) == 0x01)
		{
			status._fanOn = true;
		}
		else
		{
			status._fanOn = false;
		}
		break;
	case 0x159:
		status._fuel = can_MsgRx.data[5];
		break;
	case 0x1a2:
		if ((can_MsgRx.data[0] & 0x80) == 0x80)
		{
			status._rearDefrost = true;
		}
		else
		{
			status._rearDefrost = false;
		}

		if ((can_MsgRx.data[0] & 0x40) == 0x40)
		{
			status._fanRequested = true;
		}
		else
		{
			status._fanRequested = false;
		}

		if ((can_MsgRx.data[0] & 0x01) == 0x01)
		{
			status._fanOn = true;
		}
		else
		{
			status._fanOn = false;
		}
		break;
	case 0x1bd:
		// SDAR status
		if (status._siriusChan == 0)
			status._siriusChan = can_MsgRx.data[1];
		if (can_MsgRx.data[0] == 0x85)
			ChangeSiriusStation(status._siriusChan, true);
		if (status._siriusChan != can_MsgRx.data[1])
			ChangeSiriusStation(status._siriusChan, true);
		break;
	case 0x1c8:
		status._headlights = can_MsgRx.data[0];
		break;
	case 0x210:
		status._dimmerMode = can_MsgRx.data[0];
		if (can_MsgRx.data[0] == 0x03)
		{
			status._dimmer = -1;
		}
		else if (can_MsgRx.data[0] == 0x02)
		{
			status._dimmer = can_MsgRx.data[1];
		}
		break;
	case 0x3a0:
	// note = 0x01	// volume up = 0x02	// volume down = 0x04 // up arrow = 0x08	// down arrow = 0x10// right arrow = 0x20
		status.SWCButtons = can_MsgRx.data[0];
		if ((status.SWCButtons & 0x00000004) != 0)
		{
			if (status._volume > 0)
				status._volume --;
		}
		else if ((status.SWCButtons & 0x00000002) != 0)
		{
			if (status._volume < 38)
				status._volume ++;
		}
		else if ((status.SWCButtons & 0x00000010) != 0)//left down steering wheel button
		{
			//if (status._siriusChan > 0)
			//	ChangeSiriusStation(status._siriusChan-1, false);
		}
		else if ((status.SWCButtons & 0x00000008) != 0) //left up steering wheel button
		{
			//if ((status._siriusChan < 256) && (status._siriusChan > 0))
			//	ChangeSiriusStation(status._siriusChan+1, false);
		}
		else if ((status.SWCButtons & 0x00000001) != 0)//right middle button)
		{
			//First check if this button was also pressed in sequence (to prevent turning on/off too fast or from holding the button down)
			if(status.prevSWCButtons == status.SWCButtons)
				break;
			if(status._radioMode == OFF) //amp  is OFF so toggle it on
			{
				status._radioMode = AUX;
			} else
			{
				status._radioMode = OFF;//AMP was on so toggle it OFF
			}

		}
		else if ((status.SWCButtons & 0x00000020) != 0)//left middle button)
		{
			//does nothing yet
		}
		status.prevSWCButtons = status.SWCButtons; //save what button was pressed for the next time we process this
		break;
	case 0x3bd:
		ReadSiriusText((char *)can_MsgRx.data);
		break;
	case 0x400:// this message seems to be a message requesting all other devices to start announcing their presence
		//if (can_MsgRx.data[0] == 0xfd)
		//{
		//}
		break;
	}
}

void RadioEmulator::ReadSiriusText(char *data)
{
    int num = (data[0] & 0xF0) >> 4;
    if ((num > 7) || (num < 0))
    {
        return;
    }
    
    int part = (data[0] & 0x0E) >> 1;
    if ((part > 7) || (part < 0))
    {
        return;
    }

    if ((data[0] & 0x01) != 0)
    {
        memset(&siriusdata[num * 64], 0, 64);
    }

    memset(&siriusdata[(num * 64) + (part * 7)], 0, 7);
    
    for (int i = 1; i < 8; i++)
    {
        siriusdata[(num * 64) + (part * 7) + (i-1)] = data[i];
    }
/*
    int cls = (data[0] & 0x0F) >> 1;
    if (cls - 1 == 0)
    {
        for (int i = 0; i < 8; i++)
        {
            memset(st.TextLine[i], 0, 64);
            for (int j = 0; j < 8; j++)
            {
                strcat(st.TextLine[i], siriusText[i][j]);
            }
            
            printf("%d: %s\r\n", i, st.TextLine[i]);
        }
    }
*/
}

void RadioEmulator::ReceivedData(int socketStatus, int len, char *msg)
{
    if ((msg[0] == 0x42) && (msg[1] == 0x42) && (msg[2] == 0x42) && (msg[3] == 0x42))
    {
        //ReceivedHostMsg = true;
        
        switch (msg[4])
        {
            case 0x00:
                opMode = slave;
            break;
            
            case 0x01:
                if (len >= 11)
                {
                    status._volume = msg[5];
                    status._balance = msg[6];
                    status._fade = msg[7];
                    status._bass = msg[8];
                    status._mid = msg[9];
                    status._treble = msg[10];
                }
                
//              writeInitFile();
            break;
            
            case 0x02:
                if (len >= 6)
                {
                    status._siriusChan = msg[5];
                    ChangeSiriusStation(msg[5], false);
                }
            break;
            
            case 0x03:
                if (len >= 11)
                {
                    status._radioMode = (radioMode)msg[5];
                    
                    switch (status._radioMode)
                    {
                        case AM:
                            status._amPreset = msg[6];
                            status._amFreq = msg[7] + (msg[8] << 8);
                        break;
                        
                        case FM:
                            status._fmPreset = msg[6];
                            status._fmFreq = msg[7] + (msg[8] << 8);
                        break;
                        
                        case SAT:
                            status._siriusPreset = msg[6];
                            status._siriusChan = msg[7];
                        break;
                        
                        case CD:
                            status._cdNum = msg[6];
                            status._cdTrackNum = msg[7];
                            status._cdHours = msg[8];
                            status._cdMinutes = msg[9];
                            status._cdSeconds =  msg[10];
                        break;
                        
                        case VES:
                        break;
                        case MAX_MODE:
                        break;
                        case AUX:
                        break;
                        case OFF:
                        break;
                    }
                }
            break;
            
            case 0x04:
//                CANMessage canMsg;
//                canMsg.id = msg[5] + (msg[6] << 8);
//                canMsg.len = msg[7];
//                memcpy(canMsg.data, msg + 8, canMsg.len);
                
//                hostMessages.push_back(canMsg);
            break;
                
        }

    }
}

void RadioEmulator::WriteCANMessages(void)
{
    writeCANFlag = true;
}

void RadioEmulator::SendStatusToHost(void)
{
    statusFlag = true;
}

void RadioEmulator::CheckHostTimeout(void)
{
    //hostTimeoutFlag = true;
}

/*
// only enable this if trying to power up/down the processor
void RadioEmulator::CheckCANTimeout(void)
{
    CANTimeoutFlag = true;
}*/


//static void isr_outer_WriteCANMessages()
//{

//}
/*
static void isr_outer_SendStatusToHost();
{

}*/
//static void isr_outer_CheckCANTimeout();
//static void isr_outer_CheckHostTimeout();


/*
void RadioEmulator::CANActivity(void)
{
    if (powerUpIRQCounter == 5)
    {
        canIRQ->rise(0);
        needToWakeUp = true;
    }
    powerUpIRQCounter++;
}*/
