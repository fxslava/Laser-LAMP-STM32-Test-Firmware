#include <string.h>
#include "DGUS.h"
#include "Driver_USART.h"
#include "stm32f4xx_hal.h"
#include "SolidStateLaser.h"
#include "GlobalVariables.h"
#include "LaserMisc.h"

#include <math.h>
#include "arm_math.h"

extern void SetDACValue(float32_t value);
extern TIM_HandleTypeDef hTIM11;
extern bool eneable_temp_sensor;

void FractLaserWork_Process(uint16_t pic_id)
{
	bool update = false;
	uint16_t new_pic_id = pic_id;
	static int timeout_cnt = 0;
	
	DGUS_SOLIDSTATELASER* value;
	ReadVariable(FRAMEDATA_SOLIDSTATELASER_BASE, (void**)&value, sizeof(frameData_FractLaser));
	if ((osSignalWait(DGUS_EVENT_SEND_COMPLETED, g_wDGUSTimeout).status != osEventTimeout) && (osSignalWait(DGUS_EVENT_RECEIVE_COMPLETED, g_wDGUSTimeout).status != osEventTimeout))
	{
		convert_laserdata_ss(&frameData_FractLaser, value);
		timeout_cnt = 0;
	}
	else 
	{
		timeout_cnt++;
		if (timeout_cnt > 2)
		{
			timeout_cnt = 0;
			
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
			
			new_pic_id = FRAME_PICID_FRACTLASER_INPUT;
			
			SoundOn();
			__HAL_TIM_SET_AUTORELOAD(&hTIM11, 42000);
			HAL_TIM_Base_Start_IT(&hTIM11);
			
			update = true;
		}
		return;
	}
	
	uint16_t state = frameData_FractLaser.state;
	
	osDelay(50);
	
	__SOLIDSTATELASER_DISCHARGEOFF();
	
	if (pic_id > FRAME_PICID_FRACTLASER_INPUT && pic_id <= FRAME_PICID_FRACTLASER_WORK)
		__SOLIDSTATELASER_HVON();
	else
		__SOLIDSTATELASER_HVOFF();
	
	if (pic_id >= FRAME_PICID_FRACTLASER_SIMMER && pic_id <= FRAME_PICID_FRACTLASER_WORK)
		__SOLIDSTATELASER_SIMMERON();
	else
		__SOLIDSTATELASER_SIMMEROFF();
	
	if (pic_id == FRAME_PICID_FRACTLASER_WORK)
		SolidStateLaser_en = true;
	else
		SolidStateLaser_en = false;
	
	//frameData_SolidStateLaser.state = 0;
	
	// Input pressed
	if (frameData_FractLaser.buttons.onSimmerBtn != 0)
	{		
		new_pic_id = FRAME_PICID_FRACTLASER_SIMMER;
		
		frameData_FractLaser.state = 2;
		
		// On Input Pressed
		frameData_FractLaser.buttons.onSimmerBtn = 0;
		update = true;
	}
	
	// Simmer wait
	if (pic_id == FRAME_PICID_FRACTLASER_SIMMER)
	{
		frameData_FractLaser.state = 2;
		
#ifdef DEBUG_SOLID_STATE_LASER		
		if (__MISC_GETSIMMERSENSOR()) 
#endif
		{
			frameData_FractLaser.state = 3;
			new_pic_id = FRAME_PICID_FRACTLASER_START;
			update = true;
		}
	}
	
	// Start pressed
	if (frameData_FractLaser.buttons.onStartBtn != 0)
	{
		// On Start Pressed
		frameData_FractLaser.buttons.onStartBtn = 0;
		
		//LampControlPulseStart();
		__MISC_LASERLED_ON();
		
		new_pic_id = FRAME_PICID_FRACTLASER_WORK;
		
		update = true;
	}
	
	if (frameData_FractLaser.buttons.onStopBtn != 0)
	{
		// On Input Pressed
		frameData_FractLaser.buttons.onStopBtn = 0;
		
		__MISC_LASERLED_OFF();
		
		SolidStateLaser_en = false;
		LampControlPulseStop();
		__SOLIDSTATELASER_HVOFF();
		__SOLIDSTATELASER_SIMMEROFF();
		SetDACValue(0.0f);
		frameData_FractLaser.state = 0;
		new_pic_id = FRAME_PICID_FRACTLASER_INPUT;
		update = true;
		StoreGlobalVariables();
	}
	
	if (frameData_FractLaser.buttons.onCancelBtn != 0)
	{
		// On Input Pressed
		frameData_FractLaser.buttons.onCancelBtn = 0;
		
		SolidStateLaser_en = false;
		LampControlPulseStop();
		__SOLIDSTATELASER_HVOFF();
		__SOLIDSTATELASER_SIMMEROFF();
		SetDACValue(0.0f);
		frameData_FractLaser.state = 0;
		new_pic_id = FRAME_PICID_FRACTLASER_INPUT;
		update = true;
		StoreGlobalVariables();
	}
	
	if (state != frameData_FractLaser.state)
		update = true;
	
	if (frameData_FractLaser.PulseCounter != GetSolidStateGlobalPulse(LaserID))
	{
		frameData_FractLaser.PulseCounter = GetSolidStateGlobalPulse(LaserID);
		frameData_FractLaser.SessionPulseCounter = GetSolidStateSessionPulse(LaserID);;
		update = true;
	}
	
	if (update)
	{
		WriteSolidStateLaserDataConvert16(FRAMEDATA_SOLIDSTATELASER_BASE, &frameData_FractLaser);
		osSignalWait(DGUS_EVENT_SEND_COMPLETED, g_wDGUSTimeout);
	}
	
	if (pic_id != new_pic_id && update)
		SetPicId(new_pic_id, g_wDGUSTimeout);
}
