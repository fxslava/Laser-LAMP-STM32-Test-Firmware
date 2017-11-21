#include <string.h>
#include "DGUS.h"
#include "Driver_USART.h"
#include "stm32f4xx_hal.h"
#include "SolidStateLaser.h"
#include "GlobalVariables.h"

#include <math.h>
#include "arm_math.h"

extern void SetDACValue(float32_t value);

void LaserDiodePrepare_Process(uint16_t pic_id)
{
	bool update = false;
	uint16_t new_pic_id = pic_id;
	
	uint16_t timer_minutes = frameData_LaserDiode.timer.timer_minutes;
	uint16_t timer_seconds = frameData_LaserDiode.timer.timer_seconds;
	
	DGUS_LASERDIODE* value;
	ReadVariable(FRAMEDATA_LASERDIODE_BASE, (void**)&value, sizeof(frameData_LaserDiode));
	if ((osSignalWait(DGUS_EVENT_SEND_COMPLETED, g_wDGUSTimeout).status != osEventTimeout) && (osSignalWait(DGUS_EVENT_RECEIVE_COMPLETED, g_wDGUSTimeout).status != osEventTimeout))
		convert_laserdata(&frameData_LaserDiode, value);
	else 
		return;
	
	switch (pic_id)
	{
		case FRAME_PICID_LASERDIODE_FLOWERROR:
			frameData_LaserDiode.timer.timer_minutes = (uint16_t)flow2;
			frameData_LaserDiode.timer.timer_seconds = (uint16_t)(flow2 * 10.0f) % 10;
			if (flow2 > flow_normal)
			{
				if (!prepare)
				{
					frameData_LaserDiode.state = 1;
					new_pic_id = FRAME_PICID_LASERDIODE_INPUT;
				}
				else
					new_pic_id = FRAME_PICID_LASERDIODE_PREPARETIMER;
				update = true;
			}
			break;
		case FRAME_PICID_LASERDIODE_TEMPERATUREOUT:
			frameData_LaserDiode.timer.timer_minutes = (uint16_t)temperature;
			frameData_LaserDiode.timer.timer_seconds = (uint16_t)(temperature * 10.0f) % 10;
			//frameData_LaserDiode.coolIcon = 1;
			if (temperature < temperature_normal)
			{
				if (!prepare)
				{
					frameData_LaserDiode.state = 1;
					new_pic_id = FRAME_PICID_LASERDIODE_INPUT;
				}
				else
					new_pic_id = FRAME_PICID_LASERDIODE_PREPARETIMER;
				update = true;
			}
			break;
		case FRAME_PICID_LASERDIODE_PREPARETIMER:
			CoolSet((frameData_LaserDiode.cooling + 1) * 17);
		
			frameData_LaserDiode.timer.timer_minutes = m_wMinutes;
			frameData_LaserDiode.timer.timer_seconds = m_wSeconds;
			if (!prepare)
			{
				frameData_LaserDiode.state = 1;
				new_pic_id = FRAME_PICID_LASERDIODE_READY;
				update = true;
			}
			break;
	}
	
	if (timer_minutes != frameData_LaserDiode.timer.timer_minutes ||
			timer_seconds != frameData_LaserDiode.timer.timer_seconds)
		update = true;
	
	if (frameData_LaserDiode.buttons.onCancelBtn != 0)
	{
		// On Stop Pressed
		frameData_LaserDiode.buttons.onCancelBtn = 0;
		new_pic_id = FRAME_PICID_LASERDIODE_INPUT;
		DiodeLaser_en = false;
		DiodeControlPulseStop();
		__MISC_LASERDIODE_OFF();
		SetDACValue(0.0f);
		update = true;
		
		//StoreGlobalVariables();
	}
	
	if (update)
	{
		WriteLaserDiodeDataConvert16(FRAMEDATA_LASERDIODE_BASE, &frameData_LaserDiode);
		osSignalWait(DGUS_EVENT_SEND_COMPLETED, g_wDGUSTimeout);
	}
	
	if (/*pic_id != new_pic_id &&*/ update)
		SetPicId(new_pic_id, g_wDGUSTimeout);
}

void _DiodeLaserOff()
{
	// Diode Laser Off
	footswitch_en = false;
	DiodeLaser_en = false;
	DiodeControlPulseStop();
	osDelay(100);
	__MISC_LASERDIODE_OFF();
	osDelay(100);
	CoolOff();
}

void LaserDiodeErrorCheck_Process(uint16_t pic_id)
{
	// Check for error state of diode laser
	if ((pic_id >= FRAME_PICID_LASERDIODE_INPUT) && (pic_id <= FRAME_PICID_LASERDIODE_PHOTOTYPE))
	{
		MenuID = MENU_ID_DIODELASER;
		
		// Check temperature
		if (temperature > temperature_overheat)
		{
			_DiodeLaserOff();
			SetPicId(FRAME_PICID_LASERDIODE_TEMPERATUREOUT, g_wDGUSTimeout);
		}
		
#ifdef FLOW_CHECK
		// Check flow
		if (flow2 < flow_low)
		{
			_DiodeLaserOff();
			SetPicId(FRAME_PICID_LASERDIODE_FLOWERROR, g_wDGUSTimeout);
		}
#endif
		
		// Check is working
		if ((pic_id == FRAME_PICID_LASERDIODE_INPUT) || 
			  (pic_id == FRAME_PICID_LASERDIODE_PHOTOTYPE))
		{
			// Peltier off
			CoolOff();
		}
	}
}

void LaserDiodeStopIfWork(uint16_t pic_id)
{
	// If laser diode running
	if (((pic_id >= FRAME_PICID_LASERDIODE_INPUT) && (pic_id <= FRAME_PICID_LASERDIODE_PHOTOTYPE)) || (pic_id == FRAME_PICID_SERVICE_LASERDIODE))
	{
		frameData_LaserDiode.buttons.onCancelBtn = 0x00;
		frameData_LaserDiode.buttons.onIgnitionBtn = 0x00;
		frameData_LaserDiode.buttons.onInputBtn = 0x00;
		frameData_LaserDiode.buttons.onReadyBtn = 0x00;
		frameData_LaserDiode.buttons.onRestartBtn = 0x00;
		frameData_LaserDiode.buttons.onStartBtn = 0x00;
		frameData_LaserDiode.buttons.onStopBtn = 0x00;
		WriteLaserDiodeDataConvert16(FRAMEDATA_LASERDIODE_BASE, &frameData_LaserDiode);
		osSignalWait(DGUS_EVENT_SEND_COMPLETED, g_wDGUSTimeout);
		
		CoolOff();
		_DiodeLaserOff();
	}
}
