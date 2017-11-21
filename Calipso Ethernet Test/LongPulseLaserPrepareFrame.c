#include <string.h>
#include "DGUS.h"
#include "Driver_USART.h"
#include "stm32f4xx_hal.h"
#include "SolidStateLaser.h"
#include "GlobalVariables.h"

#include <math.h>
#include "arm_math.h"

extern void SetDACValue(float32_t value);

void LongPulseLaserPrepare_Process(uint16_t pic_id)
{
	bool update = false;
	uint16_t new_pic_id = pic_id;
	
	DGUS_SOLIDSTATELASER* value;
	ReadVariable(FRAMEDATA_SOLIDSTATELASER_BASE, (void**)&value, sizeof(frameData_SolidStateLaser));
	if ((osSignalWait(DGUS_EVENT_SEND_COMPLETED, g_wDGUSTimeout).status != osEventTimeout) && (osSignalWait(DGUS_EVENT_RECEIVE_COMPLETED, g_wDGUSTimeout).status != osEventTimeout))
			convert_laserdata_ss(&frameData_SolidStateLaser, value);
	else 
		return;
	
	switch (pic_id)
	{
		case FRAME_PICID_LONGPULSE_FLOWERROR:
			frameData_LaserDiode.flow = (uint16_t)flow1 * 10.0f;
			if (flow1 > flow_normal)
			{
				frameData_LaserDiode.state = 3; // Set ready state
				new_pic_id = FRAME_PICID_LONGPULSE_SIMMERSTART;
			}
			update = true;
			break;
		case FRAME_PICID_LONGPULSE_OVERHEATING:
			if (temperature < temperature_normal)
			{
				frameData_LaserDiode.state = 3; // Set ready state
				new_pic_id = FRAME_PICID_LONGPULSE_SIMMERSTART;
			}
			update = true;
			break;
		case FRAME_PICID_LONGPULSE_FAULT:
			break;
	}
	
	if (update)
	{
		WriteSolidStateLaserDataConvert16(FRAMEDATA_SOLIDSTATELASER_BASE, &frameData_SolidStateLaser);
		osSignalWait(DGUS_EVENT_SEND_COMPLETED, g_wDGUSTimeout);
	}
	
	if (pic_id != new_pic_id && update)
		SetPicId(new_pic_id, g_wDGUSTimeout);
}

void _LongPulseLaserOff()
{
	// Solid State Laser Off
	footswitch_en = false;
	SolidStateLaser_en = false;
	LampControlPulseStop();
	osDelay(100);
	__SOLIDSTATELASER_SIMMEROFF();
	osDelay(100);
	__SOLIDSTATELASER_HVOFF();
	osDelay(100);
	__SOLIDSTATELASER_DISCHARGEON();
}

void LongPulseErrorCheck_Process(uint16_t pic_id)
{
	// Check for errors of solid state laser long pulse
	if ((pic_id >= 51) && (pic_id <= 58))
	{
		MenuID = MENU_ID_LONGPULSE;
		
		// Check temperature
		if (temperature > temperature_overheat_solidstate)
		{
			_LongPulseLaserOff();
			SetPicId(FRAME_PICID_LONGPULSE_OVERHEATING, g_wDGUSTimeout);
		}
		
#ifdef FLOW_CHECK
		// Check flow
		if (flow1 < flow_low)
		{
			_LongPulseLaserOff();
			SetPicId(FRAME_PICID_LONGPULSE_FLOWERROR, g_wDGUSTimeout);
		}
#endif
		
		// Fault check
		if (__MISC_GETCHARGEMODULEFAULTSTATE())
		{
			_LongPulseLaserOff();
			SetPicId(FRAME_PICID_LONGPULSE_FAULT, g_wDGUSTimeout);
		}
	}
}

void LongPulseStopIfWork(uint16_t pic_id)
{
	if (((pic_id >= FRAME_PICID_LONGPULSE_INPUT) && (pic_id <= FRAME_PICID_LONGPULSE_FAULT)))
	{
		frameData_SolidStateLaser.buttons.onInputBtn = 0x00;
		frameData_SolidStateLaser.buttons.onSimmerBtn = 0x00;
		frameData_SolidStateLaser.buttons.onStartBtn = 0x00;
		frameData_SolidStateLaser.buttons.onStopBtn = 0x00;
		WriteSolidStateLaserDataConvert16(FRAMEDATA_SOLIDSTATELASER_BASE, &frameData_SolidStateLaser);
		osSignalWait(DGUS_EVENT_SEND_COMPLETED, g_wDGUSTimeout);
		
		_LongPulseLaserOff();
	}
}
