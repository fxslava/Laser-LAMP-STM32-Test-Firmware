
#include "cmsis_os.h"                                           // CMSIS RTOS header file
#include "DGUS.h"
#include "Driver_USART.h"
#include "GlobalVariables.h"
#include "LaserMisc.h"

#include <string.h>
#include <stdlib.h>

/*----------------------------------------------------------------------------
 *      Thread 1 'Thread_Name': Sample thread
 *---------------------------------------------------------------------------*/
 
 /* USART Driver */
#ifdef USE_DGUS_DRIVER
extern ARM_DRIVER_USART Driver_USART1;
extern ARM_DRIVER_USART* DGUS_USART_Driver;
#endif
 
void MainThread (void const *argument);                             // thread function
osThreadId tid_MainThread;                                          // thread id
osThreadDef (MainThread, osPriorityNormal, 1, 0);                   // thread object

//----------------------------- GUI FRAMES ----------------------------------
extern void     PasswordFrame_Process(uint16_t pic_id);
extern void      ServiceFrame_Process(uint16_t pic_id);
extern void ServiceDiodeFrame_Process(uint16_t pic_id);

extern void   LaserDiodeInput_Init   (uint16_t pic_id);
extern void   LaserDiodeInput_Process(uint16_t pic_id);
extern void LaserDiodePrepare_Process(uint16_t pic_id);
extern void    LaserDiodeWork_Process(uint16_t pic_id);

extern void SolidStateLaserPrepare_Process(uint16_t pic_id);
extern void   SolidStateLaserInput_Process(uint16_t pic_id);
extern void    SolidStateLaserWork_Process(uint16_t pic_id);

int Init_Main_Thread (void) {

  tid_MainThread = osThreadCreate (osThread(MainThread), NULL);
  if (!tid_MainThread) return(-1);
	
#ifdef USE_DGUS_DRIVER
	/*Initialize the USART driver */
  Driver_USART1.Initialize(DWIN_USART_callback);
  /*Power up the USART peripheral */
  Driver_USART1.PowerControl(ARM_POWER_FULL);
	
  /*Configure the USART to 115200 Bits/sec */
  Driver_USART1.Control(ARM_USART_MODE_ASYNCHRONOUS |
												ARM_USART_DATA_BITS_8 |
												ARM_USART_PARITY_NONE |
												ARM_USART_STOP_BITS_1 |
												ARM_USART_FLOW_CONTROL_NONE, DGUS_BAUDRATE);
	
	/* Enable Receiver and Transmitter lines */
  Driver_USART1.Control (ARM_USART_CONTROL_TX, 1);
  Driver_USART1.Control (ARM_USART_CONTROL_RX, 1);
	
	/* Set DGUS driver */
	DGUS_USART_Driver = &Driver_USART1;
#endif
	
  return(0);
}

void UpdateLaserState(uint16_t pic_id)
{
	if ((pic_id == FRAME_PICID_LASERDIODE_STARTED) || 
			(pic_id == FRAME_PICID_SOLIDSTATE_WORK) ||
			(pic_id == FRAME_PICID_REMOTECONTROL) ||
			(pic_id == FRAME_PICID_SERVICE_SOLIDSTATELASER) ||
			(pic_id == FRAME_PICID_SERVICE_LASERDIODE))
		footswitch_en = true;
	else
		footswitch_en = false;
	
	// Switch to Solid State Laser
	if ((pic_id == FRAME_PICID_SERVICE_SOLIDSTATELASER) || 
			(pic_id == FRAME_PICID_REMOTECONTROL) ||
			((pic_id >= 35) && (pic_id <= 42)))
		__MISC_RELAY2_ON();
	else
		__MISC_RELAY2_OFF();
	
	// Switch to Laser Diode
	if ((pic_id == FRAME_PICID_SERVICE_LASERDIODE) || 
			((pic_id >= 19) && (pic_id <= 32)))
		__MISC_RELAY3_ON();
	else
	{
		__MISC_LASERDIODE_OFF();
		osDelay(100);
		__MISC_RELAY3_OFF();
	}
	
	// Check for diode laser
	if ((pic_id >= 19) && (pic_id <= 32))
	{
		// Check temperature
		if (temperature > temperature_overheat)
			SetPicId(FRAME_PICID_LASERDIODE_TEMPERATUREOUT, g_wDGUSTimeout);
		
		// Check flow
		if (flow1 < flow_low)
			SetPicId(FRAME_PICID_LASERDIODE_FLOWERROR, g_wDGUSTimeout);
		
		// Check is working
		if ((pic_id == FRAME_PICID_LASERDIODE_INPUT) || 
			  (pic_id == FRAME_PICID_LASERDIODE_PHOTOTYPE))
		{
			// Peltier off
			peltier_en = false;
		}
	}
	else
	{
		CoolOff();
		// Peltier off
		peltier_en = false;
	}
	
	// Check for solid state laser
	if ((pic_id >= 35) && (pic_id <= 43))
	{
		// Check temperature
		if (temperature > temperature_overheat)
			SetPicId(FRAME_PICID_SOLIDSTATE_OVERHEATING, g_wDGUSTimeout);
		
		// Check flow
		if (flow1 < flow_low)
			SetPicId(FRAME_PICID_SOLIDSTATE_FLOWERROR, g_wDGUSTimeout);
		
		// Fault check
		if (__MISC_GETCHARGEMODULEFAULTSTATE())
			SetPicId(FRAME_PICID_SOLIDSTATE_FAULT, g_wDGUSTimeout);
	}
}

DGUS_LASERDIODE_STATE laser_state;
extern volatile float32_t temperature;

void UpdateLaserStatus()
{
	laser_state.temperature = (uint16_t)(temperature * 10.0f);
	laser_state.coolIcon = 1;
	laser_state.flow = 10;
	
	WriteVariableConvert16(FRAMEDATA_LASERSTATE_BASE, &laser_state, sizeof(laser_state));
	osSignalWait(DGUS_EVENT_SEND_COMPLETED, g_wDGUSTimeout);
}

void MainThread (void const *argument) {
	static uint16_t last_pic_id = 0;
	
	LaserDiodeInput_Init(pic_id);

  while (1) {
    ; // Insert thread code here...
		pic_id = GetPicId(g_wDGUSTimeout, pic_id);		
		UpdateLaserState(pic_id);
		
		LaserID = GetLaserID();
		
		// GUI Frames process
		switch (pic_id)
		{
			case FRAME_PICID_LOGO:
				SetPicId(FRAME_PICID_MAINMENU, g_wDGUSTimeout);
				break;
			
			// Frames process
			case FRAME_PICID_PASSWORD:									
				PasswordFrame_Process(pic_id);	
				break;
			case FRAME_PICID_SERVICE_SOLIDSTATELASER:		
				ServiceFrame_Process(pic_id);	
				break;
			case FRAME_PICID_SERVICE_LASERDIODE:				
				ServiceDiodeFrame_Process(pic_id);
				break;
			case FRAME_PICID_SERVICE_BASICSETTINGS:			break; // Blank picture, for future release
			
			// Laser Diode control
			case FRAME_PICID_LASERDIODE_INPUT:
				LaserDiodeInput_Process(pic_id);
				UpdateLaserStatus();
				break;
			case FRAME_PICID_LASERDIODE_TEMPERATUREOUT:
			case FRAME_PICID_LASERDIODE_PREPARETIMER:
			case FRAME_PICID_LASERDIODE_FLOWERROR:
				LaserDiodePrepare_Process(pic_id);
				UpdateLaserStatus();
				break;
			case FRAME_PICID_LASERDIODE_READY:
			case FRAME_PICID_LASERDIODE_START:
			case FRAME_PICID_LASERDIODE_STARTED:
				LaserDiodeWork_Process(pic_id);
			case FRAME_PICID_LASERDIODE_PHOTOTYPE:
				UpdateLaserStatus();
				break;
			
			// WiFi features
			case FRAME_PICID_SERVICE_WIFISCANNING:			break; // Blank picture, for future release
			case FRAME_PICID_SERVICE_WIFIAUTHENTICATION:break; // Blank picture, for future release
			
			// App idle user state
			case FRAME_PICID_MAINMENU:
			case FRAME_PICID_SERVICE:
			case FRAME_PICID_LANGMENU:
				// nothing to do
				break;
			
			case FRAME_PICID_SOLIDSTATE_INPUT:		
				SolidStateLaserInput_Process(pic_id);
				UpdateLaserStatus();
				break;
			case FRAME_PICID_SOLIDSTATE_SIMMERSTART:
			case FRAME_PICID_SOLIDSTATE_SIMMER:
			case FRAME_PICID_SOLIDSTATE_START:
			case FRAME_PICID_SOLIDSTATE_WORK:
				SolidStateLaserWork_Process(pic_id);
				UpdateLaserStatus();
				break;
			case FRAME_PICID_SOLIDSTATE_FLOWERROR:
			case FRAME_PICID_SOLIDSTATE_OVERHEATING:
			case FRAME_PICID_SOLIDSTATE_FAULT:
				break;
			
			case FRAME_PICID_REMOTECONTROL:
				break;
			
			default:
				SetPicId(FRAME_PICID_MAINMENU, g_wDGUSTimeout);
				break;
		}
		
		// Remote control
		if (RemoteControl)
			SetPicId(FRAME_PICID_REMOTECONTROL, g_wDGUSTimeout);
		else
			if (pic_id == FRAME_PICID_REMOTECONTROL)
			{
				SetPicId(FRAME_PICID_MAINMENU, g_wDGUSTimeout);
				//pic_id = FRAME_PICID_MAINMENU;
			}
		
		last_pic_id = pic_id;
		
    osThreadYield ();                                           // suspend thread
  }
}