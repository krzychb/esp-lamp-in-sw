/* main.c -- MQTT client example
*
* Copyright (c) 2014-2015, Tuan PM <tuanpm at live dot com>
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
* * Neither the name of Redis nor the names of its contributors may be used
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
#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "mqtt.h"
#include "wifi.h"
#include "config.h"
#include "debug.h"
#include "gpio.h"
#include "user_interface.h"
#include "mem.h"
#include "lightsw.h"


// ====================
// MQTT topics - inputs
// ====================
#define OPENHAB_HEART_BEAT "control/oh/00068601"
//
#define compile_075
//
// 003e050f	switch position - input - Study Ceiling Lamp - 062
// 003e0d05	light intensity - input - Study Ceiling Lamp - 062
// 003e8502	status - input - Study Ceiling Lamp - 062
// 003e8309	free memory - input - Study Ceiling Lamp - 062
// 003e8404	command - output - Study Ceiling Lamp - 062
// 003e0c05	light intensity - set point - Study Ceiling Lamp - 062
// 003e8604	heart beat - output - Study Ceiling Lamp - 062
//
#ifdef compile_062
#define LIGHTSW_NODE_MANE "Study Ceiling Lamp"
#define LIGHTSW_POSITION_STATUS "sensor/esp/003e050f"
#define LIGHTSW_INTENSITY_VALUE "sensor/esp/003e0d05"
#define LIGHTSW_OPERATION_STATUS "sensor/esp/003e8502"
#define LIGHTSW_FREE_HEAP_SIZE "sensor/esp/003e8309"
#define LIGHTSW_CONTROL_COMMAND "control/esp/003e8404"
#define LIGHTSW_INTENSITY_SETPOINT "control/esp/003e0c05"
#endif
//
// 0041050f	switch position - input - Living Room Ceiling Lamp S - 065
// 00410d05	light intensity - input - Living Room Ceiling Lamp S - 065
// 00418502	status - input - Living Room Ceiling Lamp S - 065
// 00418309	free memory - input - Living Room Ceiling Lamp S - 065
// 00418404	command - output - Living Room Ceiling Lamp S - 065
// 00410c05	light intensity - set point - Living Room Ceiling Lamp S - 065
//
#ifdef compile_065
#define LIGHTSW_NODE_MANE "Living Room Ceiling Lamp South"
#define LIGHTSW_POSITION_STATUS "sensor/esp/0041050f"
#define LIGHTSW_INTENSITY_VALUE "sensor/esp/00410d05"
#define LIGHTSW_OPERATION_STATUS "sensor/esp/00418502"
#define LIGHTSW_FREE_HEAP_SIZE "sensor/esp/00418309"
#define LIGHTSW_CONTROL_COMMAND "control/esp/00418404"
#define LIGHTSW_INTENSITY_SETPOINT "control/esp/00410c05"
#endif
//
// 004b050f	switch position - input - Living Room Ceiling Lamp N - 075
// 004b0d05	light intensity - input - Living Room Ceiling Lamp N - 075
// 004b8502	status - input - Living Room Ceiling Lamp N - 075
// 004b8309	free memory - input - Living Room Ceiling Lamp N - 075
// 004b8404	command - output - Living Room Ceiling Lamp N - 075
// 004b0c05	light intensity - set point - Living Room Ceiling Lamp N - 075
//
#ifdef compile_075
#define LIGHTSW_NODE_MANE "Living Room Ceiling Lamp North"
#define LIGHTSW_POSITION_STATUS "sensor/esp/004b050f"
#define LIGHTSW_INTENSITY_VALUE "sensor/esp/004b0d05"
#define LIGHTSW_OPERATION_STATUS "sensor/esp/004b8502"
#define LIGHTSW_FREE_HEAP_SIZE "sensor/esp/004b8309"
#define LIGHTSW_CONTROL_COMMAND "control/esp/004b8404"
#define LIGHTSW_INTENSITY_SETPOINT "control/esp/004b0c05"
#endif



MQTT_Client mqttClient;
LightSW lampToControl;


void ICACHE_FLASH_ATTR wifiConnectCb(uint8_t status)
{
	if(status == STATION_GOT_IP){
		INFO("Calling MQTT_Connect\r\n");
		MQTT_Connect(&mqttClient);
	} else {
		INFO("Calling MQTT_Disconnect\r\n");
		MQTT_Disconnect(&mqttClient);
	}
}


#define OPENHAB_HEART_BEAT_TIMEOUT 15000000 /* milliseconds */
static long heartBeatTimer = 0;

#define FORCE_UPDATE_PUBLISH_PERIOD 60 /* seconds */
#define DB_INTENSITY_CHANGE_PERCENT 1

static uint32 freeHeapSize, lastFreeHeapSize = 0;
static int lightIntensityPercent, lastLightIntensityPercent = 0;
static char operationStatus, lastOperationStatus = OPERATION_STATUS__UNDEFINED;
static unsigned char forcePublishCounter = 0;

void ICACHE_FLASH_ATTR publish_update(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;

	char tplBuff[128];

	// check for heart beat timeout
	if (system_get_time() > heartBeatTimer + OPENHAB_HEART_BEAT_TIMEOUT)
	{
		INFO("OpenHab Heart Beat timeout!\r\n");
		wifi_station_disconnect();
		wifi_station_connect();
		heartBeatTimer = system_get_time();
	}

	// heap updates
	freeHeapSize = system_get_free_heap_size();
	if(freeHeapSize != lastFreeHeapSize || forcePublishCounter % FORCE_UPDATE_PUBLISH_PERIOD == 0)
	{
		os_sprintf(tplBuff, "%ld", freeHeapSize);
		MQTT_Publish(client, LIGHTSW_FREE_HEAP_SIZE, tplBuff, os_strlen(tplBuff), 0, 0);
		// INFO("Heap %ld / %ld\r\n", freeHeapSize, freeHeapSize - lastFreeHeapSize);
		lastFreeHeapSize = freeHeapSize;
	}

	//light switch updates
	lightIntensityPercent = LightSW_ReadIntensityPercent();
	if(abs(lightIntensityPercent - lastLightIntensityPercent) > DB_INTENSITY_CHANGE_PERCENT || forcePublishCounter % FORCE_UPDATE_PUBLISH_PERIOD == 0)
	{
		// Light intensity
		os_sprintf(tplBuff, "%d", lightIntensityPercent);
		MQTT_Publish(client, LIGHTSW_INTENSITY_VALUE, tplBuff, os_strlen(tplBuff), 0, 0);
		// INFO("Light intensity %d / %d\r\n", lightIntensityPercent, lightIntensityPercent - lastLightIntensityPercent);
		lastLightIntensityPercent = lightIntensityPercent;

		// Lamp status
		if(LightSW_ReadIntensityCounts() <= sysCfg.minIntensity/2)
			os_strcpy(tplBuff, "OFF");
		else
			os_strcpy(tplBuff, "ON");
		MQTT_Publish(client, LIGHTSW_POSITION_STATUS, tplBuff, os_strlen(tplBuff), 0, 0);
		// INFO("Light status %s\r\n", tplBuff);
	}

	// operation status
	operationStatus = LightSW_CheckStatus(&lampToControl);
	if(operationStatus != lastOperationStatus || forcePublishCounter % FORCE_UPDATE_PUBLISH_PERIOD == 0)
	{
		os_sprintf(tplBuff, "%d", operationStatus);
		MQTT_Publish(client, LIGHTSW_OPERATION_STATUS, tplBuff, os_strlen(tplBuff), 0, 0);
		// INFO("Operation status 0x%x\r\n", operationStatus);
		lastOperationStatus = operationStatus;
	}

	forcePublishCounter++;
}


#define UPDATE_PUBLISH_PERIOD 1000 /* milliseconds */
LOCAL os_timer_t update_publish_timer;
void ICACHE_FLASH_ATTR mqttConnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Connected\r\n");

	MQTT_Subscribe(client, OPENHAB_HEART_BEAT, 0);
	MQTT_Subscribe(client, LIGHTSW_CONTROL_COMMAND, 0);
	MQTT_Subscribe(client, LIGHTSW_INTENSITY_SETPOINT, 0);

	// Set up a timer to publish updates
	os_timer_disarm(&update_publish_timer);
	os_timer_setfn(&update_publish_timer, (os_timer_func_t *)publish_update, &mqttClient);
	os_timer_arm(&update_publish_timer, UPDATE_PUBLISH_PERIOD, 1);

}


void ICACHE_FLASH_ATTR mqttDisconnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Disconnected\r\n");

	// decide if to stop publishing updates to queue when there is no connection

}


void ICACHE_FLASH_ATTR mqttPublishedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	// INFO("MQTT: Published\r\n");
}


bool ICACHE_FLASH_ATTR IsDigitsOnly(char *str, int len)
{
	int i;
	for(i=0; i<len; i++)
		if (str[i] < '0' || str[i] > '9') return false;
	return true;
}


void ICACHE_FLASH_ATTR mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
	char	*topicBuf = (char*)os_zalloc(topic_len+1),
			*dataBuf = (char*)os_zalloc(data_len+1);

	MQTT_Client* client = (MQTT_Client*)args;

	os_memcpy(topicBuf, topic, topic_len);
	topicBuf[topic_len] = 0;

	os_memcpy(dataBuf, data, data_len);
	dataBuf[data_len] = 0;

	// INFO("MQTT Rx: %s, data: %s \r\n", topicBuf, dataBuf);

	// ignore data like ON, OFF, INCREASE, DECREASE, etc.
	if(IsDigitsOnly(dataBuf, os_strlen(dataBuf)))
	{
		int lampIntensityPercent;
		if (os_strcmp(topicBuf, OPENHAB_HEART_BEAT)==0)
		{
			// INFO("OpenHab Heart Beat received %s\r\n", dataBuf);
			heartBeatTimer = system_get_time();
		}
		else if (os_strcmp(topicBuf, LIGHTSW_INTENSITY_SETPOINT)==0)
		{
			lampIntensityPercent = atoi(dataBuf);
			INFO("Set Intensity from %d%% to %d%%\r\n", LightSW_ReadIntensityPercent(), lampIntensityPercent);
			LightSW_SetIntensity(&lampToControl, lampIntensityPercent);
		}
		else if (os_strcmp(topicBuf, LIGHTSW_CONTROL_COMMAND)==0)
		{
			switch ((char) atoi(dataBuf))
			{
			case REPORT_STATUS:
				INFO("Current intensity %d%% / %d\r\n", LightSW_ReadIntensityPercent(), LightSW_ReadIntensityCounts());
				INFO("Calibration %d / %d\r\n", sysCfg.minIntensity, sysCfg.maxIntensity);
				INFO("Operation status 0x%x\r\n", LightSW_CheckStatus(&lampToControl));
				break;
			case TOGGLE_ON:
				INFO("Toggle switch On\r\n");
				LightSW_ToggleSwitch(&lampToControl);
				break;
			case POWER_ON:
				INFO("Power On\r\n");
				LightSW_Power(&lampToControl, TRUE);
				break;
			case POWER_OFF:
				INFO("Power Off\r\n");
				LightSW_Power(&lampToControl, FALSE);
				break;
			case SET_INTENSITY:
				// provide pseudo random number from 0 to 100
				lampIntensityPercent = (int) (system_get_time() % 101);
				INFO("Set Intensity from %d%% to %d%%\r\n", LightSW_ReadIntensityPercent(), lampIntensityPercent);
				LightSW_SetIntensity(&lampToControl, lampIntensityPercent);
				break;
			case CALIBRATE:
				INFO("Calibrate\r\n");
				LightSW_Calibrate(&lampToControl);
				break;
			case SEQUENCE_IDLE:
				LightSW_Quit(&lampToControl);
				break;
			default:
				INFO("case?\r\n");
				break;
			}
		}
	}

	// free memory at the very end
	os_free(topicBuf);
	os_free(dataBuf);
}


void user_rf_pre_init(void)
{
}


void user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	os_delay_us(1000000);

	CFG_Load();

	MQTT_InitConnection(&mqttClient, sysCfg.mqtt_host, sysCfg.mqtt_port, sysCfg.security);
	MQTT_InitClient(&mqttClient, sysCfg.device_id, sysCfg.mqtt_user, sysCfg.mqtt_pass, sysCfg.mqtt_keepalive, 1);

	MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);
	MQTT_OnData(&mqttClient, mqttDataCb);

	// connect to WiFi for the first time
	// once done then connection will try to recover if lost
	WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd, wifiConnectCb);

	LightSW_Initialize(&lampToControl, 10);

	INFO("\r\n");
	INFO("LightSwitch with MQTT\r\n");
	INFO("Node name: %s\r\n", LIGHTSW_NODE_MANE);
	INFO("System started ...\r\n");
}
