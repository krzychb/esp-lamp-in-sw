/* lightsw.c
*  *
* Copyright (c) 2014-2015, Krzysztof Budzynski <krzychb at gazeta dot pl>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* * The name of Krzysztof Budzynski or krzychb may not be used
* to endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/

#include "user_interface.h"
#include "osapi.h"
#include "espconn.h"
#include "os_type.h"
#include "mem.h"
#include "debug.h"
#include "user_config.h"
#include "gpio.h"
#include "config.h"
#include "lightsw.h"


void ICACHE_FLASH_ATTR LightSW_Initialize(LightSW *lamp, int period_ms)
{
	lamp->stateName = SEQUENCE_IDLE;

	// pin to control relay is hard coded
	// change it below if required
	lamp->_switchPin = 5;
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);

	// pin 2 is used to output a cycle time for a scope measurement
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);

	os_timer_disarm(&lamp->_stateMachineTimer);
	os_timer_setfn(&lamp->_stateMachineTimer, (os_timer_func_t *)LightSW_RunSequence, lamp);
	os_timer_arm(&lamp->_stateMachineTimer, period_ms, 1);
}


void ICACHE_FLASH_ATTR LightSW_RunSequence(uint32_t *args)
{
	// cycle measurement using scope
	GPIO_OUTPUT_SET(2, TRUE);

	LightSW* lamp = (LightSW*) args;

	if (lamp->stateName == SEQUENCE_IDLE) return;

	// report current status or result
	lamp->operationStatus = lamp->stateName;

	// read current light intensity
	lamp->currentIntensity = LightSW_ReadIntensityCounts();

	// ====================
	// Toggle Switch
	// ====================
	if (lamp->stateName == TOGGLE_ON)
	{
		lamp->_sequenceTimer = system_get_time();
		INFO("\tIdle > ");
		GPIO_OUTPUT_SET(lamp->_switchPin, TRUE);
		lamp->_stateTimer = system_get_time();
		lamp->stateName = TOGGLE_ON__FINALIZE;
		INFO("Toggle On > ");
	}
	else if (lamp->stateName == TOGGLE_ON__FINALIZE)
	{
		if (system_get_time() > lamp->_stateTimer + TOGGLE_ON_DURATION)
		{
			GPIO_OUTPUT_SET(lamp->_switchPin, FALSE);
			lamp->stateName = SEQUENCE_IDLE;
			lamp->operationStatus = OPERATION_STATUS__SUCCESFULL;
			INFO("Idle\r\n");
		}
	}
	// ====================
	// Power On
	// ====================
	else if (lamp->stateName == POWER_ON)
	{
		lamp->_sequenceTimer = system_get_time();
		INFO("\tIdle > ");
		if (lamp->currentIntensity <= sysCfg.minIntensity/2)
		{
			GPIO_OUTPUT_SET(lamp->_switchPin, TRUE);
			lamp->_stateTimer = system_get_time();
			lamp->stateName = POWER_ON__TOGGLE_ON;
			INFO("Toggle On > ");
		}
		else
		{
			lamp->stateName = SEQUENCE_IDLE;
			lamp->operationStatus = OPERATION_STATUS__ALREADY_ON;
			INFO("Already On! > Idle\r\n");
		}
	}
	else if (lamp->stateName == POWER_ON__TOGGLE_ON)
	{
		if (system_get_time() > lamp->_stateTimer + TOGGLE_ON_DURATION)
		{
			GPIO_OUTPUT_SET(lamp->_switchPin, FALSE);
			lamp->_stateTimer = system_get_time();
			lamp->stateName = POWER_ON__IS_ON;
			INFO("Is On? > ");
		}
	}
	else if (lamp->stateName == POWER_ON__IS_ON)
	{
		if (system_get_time() > lamp->_stateTimer + POWER_ON_DURATION)
		{
			if (lamp->currentIntensity <= sysCfg.minIntensity/2)
			{
				lamp->operationStatus = OPERATION_STATUS__NOT_ON;
				INFO("Not On! > ");
			}
			else
			{
				lamp->operationStatus = OPERATION_STATUS__SUCCESFULL;
			}
			lamp->stateName = SEQUENCE_IDLE;
			INFO("Idle\r\n");
		}
	}
	// ====================
	// Power Off
	// ====================
	else if (lamp->stateName == POWER_OFF)
	{
		lamp->_sequenceTimer = system_get_time();
		INFO("\tIdle > ");
		if (lamp->currentIntensity > sysCfg.minIntensity/2)
		{
			GPIO_OUTPUT_SET(lamp->_switchPin, TRUE);
			lamp->_stateTimer = system_get_time();
			lamp->stateName = POWER_OFF__TOGGLE_ON;
			INFO("Toggle On > ");
		}
		else
		{
			lamp->stateName = SEQUENCE_IDLE;
			lamp->operationStatus = OPERATION_STATUS__ALREADY_OFF;
			INFO("Already Off! > Idle\r\n");
		}
	}
	else if (lamp->stateName == POWER_OFF__TOGGLE_ON)
	{
		if (system_get_time() > lamp->_stateTimer + TOGGLE_ON_DURATION)
		{
			GPIO_OUTPUT_SET(lamp->_switchPin, FALSE);
			lamp->_stateTimer = system_get_time();
			lamp-> stateName = POWER_OFF__IS_OFF;
			INFO("Is Off? > ");
		}
	}
	else if (lamp->stateName == POWER_OFF__IS_OFF)
	{
		if (system_get_time() > lamp->_stateTimer + POWER_OFF_DURATION)
		{
			if (lamp->currentIntensity > sysCfg.minIntensity/2)
			{
				lamp->operationStatus = OPERATION_STATUS__NOT_OFF;
				INFO(" Not Off! > ");
			}
			else
			{
				lamp->operationStatus = OPERATION_STATUS__SUCCESFULL;
			}
			lamp->stateName = SEQUENCE_IDLE;
			INFO("Idle\r\n");
		}
	}
	// ====================
	// Calibrate
	// ====================
	else if (lamp->stateName == CALIBRATE)
	{
		lamp->_sequenceTimer = system_get_time();
		lamp->_intensityChangeTimer = lamp->_sequenceTimer;
		lamp->_lastIntensity = lamp->currentIntensity;
		INFO("\tIdle > ");
		GPIO_OUTPUT_SET(lamp->_switchPin, TRUE);
		lamp->_stateTimer = system_get_time();
		lamp->stateName = CALIBRATE__ACCELERATE;
		INFO("Accelerate > ");
	}
	else if (lamp->stateName == CALIBRATE__ACCELERATE)
	{
		if (system_get_time() > lamp->_stateTimer + ACCELERATE_DURATION)
		{
			lamp->stateName = CALIBRATE__TO_LIMIT;
			INFO("To Limit > ");
		}
	}
	else if (lamp->stateName == CALIBRATE__TO_LIMIT)
	{
		// move to next step when time limit is reached or intensity does not change
		if (system_get_time() > lamp->_stateTimer + TRAVEL_MAX_DURATION || abs(lamp->_intensityChange) <= DB_INTENSITY_CHANGE)
		{
			INFO("\r\n");
			INFO("\tGot to %d  > ", lamp->currentIntensity);
			sysCfg.minIntensity = lamp->currentIntensity;
			GPIO_OUTPUT_SET(lamp->_switchPin, FALSE);
			lamp->_stateTimer = system_get_time();
			lamp->stateName = CALIBRATE__TOGGLE_OFF;
			INFO("Toggle Off > ");
		}
	}
	else if (lamp->stateName == CALIBRATE__TOGGLE_OFF)
	{
		if (system_get_time() > lamp->_stateTimer + TOGGLE_OFF_DURATION)
		{
			lamp->_sequenceTimer = system_get_time();
			GPIO_OUTPUT_SET(lamp->_switchPin, TRUE);
			lamp->_stateTimer = system_get_time();
			lamp->stateName = CALIBRATE__ACCELERATE_OTHER;
			INFO("Accelerate > ");
		}
	}
	else if (lamp->stateName == CALIBRATE__ACCELERATE_OTHER)
	{
		if (system_get_time() > lamp->_stateTimer + ACCELERATE_DURATION)
		{
			lamp->stateName = CALIBRATE__TO_OTHER_LIMIT;
			INFO("To Other Limit > ");
		}
	}
	else if (lamp->stateName == CALIBRATE__TO_OTHER_LIMIT)
	{
		// finish when time limit is reached or intensity does not change
		if (system_get_time() > lamp->_stateTimer + TRAVEL_MAX_DURATION || abs(lamp->_intensityChange) <= DB_INTENSITY_CHANGE)
		{
			GPIO_OUTPUT_SET(lamp->_switchPin, FALSE);
			INFO("\r\n");
			INFO("\tGot to %d  > ", lamp->currentIntensity);
			sysCfg.maxIntensity = lamp->currentIntensity;
			lamp->stateName = SEQUENCE_IDLE;
			INFO("Idle\r\n");
			// swap identified values if required
			if(sysCfg.maxIntensity < sysCfg.minIntensity)
			{
				int tmpVal = sysCfg.maxIntensity;
				sysCfg.maxIntensity = sysCfg.minIntensity;
				sysCfg.minIntensity = tmpVal;
			}
			CFG_Save();
			INFO("Calibration %d / %d\r\n", sysCfg.minIntensity, sysCfg.maxIntensity);
			lamp->operationStatus = OPERATION_STATUS__SUCCESFULL;
		}
	}
	// ====================
	// Set Intensity
	// ====================
	else if (lamp->stateName == SET_INTENSITY)
	{
		lamp->_sequenceTimer = system_get_time();
		lamp->_intensityChangeTimer = lamp->_sequenceTimer;
		lamp->_lastIntensity = lamp->currentIntensity;
		INFO("\tIdle > ");
		if (abs(lamp->_targetIntensity - lamp->currentIntensity) < (sysCfg.maxIntensity - sysCfg.minIntensity) / DB_INTENSITY_RATIO)
		{
			INFO("On Target! > ");
			lamp->stateName = SEQUENCE_IDLE;
			lamp->operationStatus = OPERATION_STATUS__ALREADY_ON_TARGET;
			INFO("Idle\r\n");
		}
	    else
	    {
			GPIO_OUTPUT_SET(lamp->_switchPin, TRUE);
			lamp->_stateTimer = system_get_time();
			if (lamp->currentIntensity > sysCfg.minIntensity/2)
			{
				lamp->stateName = SET_INTENSITY__ACCELERATE;
			}
			else
			{
				lamp->stateName = SET_INTENSITY__GET_ON;
				INFO("Get On > ");
			}
			INFO("Moving from %d to %d > ", lamp->currentIntensity, lamp->_targetIntensity);
	    }
	}
	else if (lamp->stateName == SET_INTENSITY__GET_ON)
	{
		  if (system_get_time() > lamp->_stateTimer + POWER_ON_DURATION - ACCELERATE_DURATION)
		  {
				lamp->_stateTimer = system_get_time();
				lamp->stateName = SET_INTENSITY__ACCELERATE;
		  }
	}
	else if (lamp->stateName == SET_INTENSITY__ACCELERATE)
	{
		if (system_get_time() > lamp->_stateTimer + ACCELERATE_DURATION)
		{
			bool goingUp = (lamp->_intensityChange > 0) ? true : false;
			lamp->_targetAbove = (lamp->currentIntensity < lamp->_targetIntensity) ? true : false;
			INFO("\r\n");
			INFO("\tMoved by %d to %d, ", lamp->_intensityChange, lamp->currentIntensity);
			if ((lamp->_targetAbove && goingUp) || (!lamp->_targetAbove && !goingUp))
			{
				lamp->stateName = SET_INTENSITY__GET_TARGET;
				INFO("Get Target > ");
			}
			else
			{
				GPIO_OUTPUT_SET(lamp->_switchPin, FALSE);
				lamp->_stateTimer = system_get_time();
				lamp->stateName = SET_INTENSITY__TOGGLE_OFF;
				INFO("Toggle Off > ");
			}
		}
	}
	else if (lamp->stateName == SET_INTENSITY__TOGGLE_OFF)
	{
		if (system_get_time() > lamp->_stateTimer + TOGGLE_OFF_DURATION)
		{
			lamp->_stateTimer = system_get_time();
			lamp->stateName = SET_INTENSITY;
			INFO("Return > \r\n");
		}
	}
	else if (lamp->stateName == SET_INTENSITY__GET_TARGET)
	{
		bool valueAbove = (lamp->currentIntensity > lamp->_targetIntensity) ? true : false;
		// finalise when target is reached or intensity does not change
		if ((lamp->_targetAbove && valueAbove) || (!lamp->_targetAbove && !valueAbove) || abs(lamp->_intensityChange) <= DB_INTENSITY_CHANGE)
		{
			lamp->stateName = SET_INTENSITY__FINALIZE;
		}
	}
	else if (lamp->stateName == SET_INTENSITY__FINALIZE)
	{
		GPIO_OUTPUT_SET(lamp->_switchPin, FALSE);
		lamp->stateName = SEQUENCE_IDLE;
		INFO("\r\n");
		INFO("\tFinalized at %d > ", lamp->currentIntensity);
		if (abs(lamp->_targetIntensity - lamp->currentIntensity) >= (sysCfg.maxIntensity - sysCfg.minIntensity) / DB_INTENSITY_RATIO)
		{
			lamp->operationStatus = OPERATION_STATUS__OFF_TARGET;
			INFO("Off Target! > ");
		}
		else
		{
			lamp->operationStatus = OPERATION_STATUS__ON_TARGET;
		}
		INFO("Idle\r\n");
	}
	// ========================================
	// Common Tasks & Check for Timeout
	// ========================================
	if (lamp->stateName != SEQUENCE_IDLE)
	{
		if (system_get_time() > lamp->_sequenceTimer + SEQUENCE_TIMEOUT)
		{
			lamp->stateName = SEQUENCE_IDLE;
			lamp->operationStatus = OPERATION_STATUS__TIMEOUT;
			INFO("Timeout! > Idle\r\n");
		}
		// Periodically sense light intensity change as required by certain states
		if ((lamp->stateName & SET_INTENSITY) == SET_INTENSITY || (lamp->stateName & CALIBRATE) == CALIBRATE)
		{
			if (system_get_time() > lamp->_intensityChangeTimer + SENSE_CHANGE_DURATION)
			{
				lamp->_intensityChange = lamp->currentIntensity - lamp->_lastIntensity;
				lamp->_lastIntensity = lamp->currentIntensity;
				lamp->_intensityChangeTimer = system_get_time();
			}
		}
	}

	// cycle measurement using scope
	GPIO_OUTPUT_SET(2, FALSE);
}


int ICACHE_FLASH_ATTR LightSW_ReadIntensityCounts(void)
{
	int i, valueAcc = 0;
	for (i = 0; i < MEASURE_SAMPLES; i++)
		valueAcc += system_adc_read();
	return (int) valueAcc / MEASURE_SAMPLES;
}


int ICACHE_FLASH_ATTR LightSW_ReadIntensityPercent(void)
{
	return (int) (100.0 * (LightSW_ReadIntensityCounts() - sysCfg.minIntensity) / (sysCfg.maxIntensity - sysCfg.minIntensity));
}


void ICACHE_FLASH_ATTR LightSW_ToggleSwitch(LightSW *lamp)
{
	lamp->stateName = TOGGLE_ON;
}


void ICACHE_FLASH_ATTR LightSW_Power(LightSW *lamp, bool state)
{
	if (state)
		lamp->stateName = POWER_ON;
	else
		lamp->stateName = POWER_OFF;
}


void ICACHE_FLASH_ATTR LightSW_Calibrate(LightSW *lamp)
{
	lamp->stateName = CALIBRATE;
}


void ICACHE_FLASH_ATTR LightSW_SetIntensity(LightSW *lamp, int targetIntensityPercent)
{
	lamp->_targetIntensity = (int) (sysCfg.minIntensity + (sysCfg.maxIntensity - sysCfg.minIntensity) * (float) targetIntensityPercent / 100);
	lamp->stateName = SET_INTENSITY;
}


void ICACHE_FLASH_ATTR LightSW_Quit(LightSW *lamp)
{
	GPIO_OUTPUT_SET(lamp->_switchPin, FALSE);
	lamp->stateName = SEQUENCE_IDLE;
	lamp->operationStatus = OPERATION_STATUS__QUIT;
	INFO("Quit! > Idle\r\n");
}


char ICACHE_FLASH_ATTR LightSW_CheckStatus(LightSW *lamp)
{
	return lamp->operationStatus;
}
