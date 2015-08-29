#ifndef _LIGHTSW_H_
#define _LIGHTSW_H_

#define MEASURE_SAMPLES 5

// times in microseconds
#define TOGGLE_ON_DURATION 100000
#define TOGGLE_OFF_DURATION 100000
#define POWER_ON_DURATION 1300000
#define POWER_OFF_DURATION 2300000
#define ACCELERATE_DURATION 900000
#define SENSE_CHANGE_DURATION 90000
#define TRAVEL_MAX_DURATION 7000000
#define SEQUENCE_TIMEOUT 8000000

// values in AD counts
#define MIN_INTENSITY 300
#define MAX_INTENSITY 1010
#define DB_INTENSITY_RATIO 20
#define DB_INTENSITY_CHANGE 1


//
// commands and state machine states
//
#define SEQUENCE_IDLE 0x00
#define REPORT_STATUS 0x08

// Toggle ON
#define TOGGLE_ON 0x10
#define TOGGLE_ON__FINALIZE 0x11

// Power ON
#define POWER_ON 0x20
#define POWER_ON__TOGGLE_ON 0x21
#define POWER_ON__IS_ON 0x22

// Power OFF
#define POWER_OFF 0x30
#define POWER_OFF__TOGGLE_ON 0x31
#define POWER_OFF__IS_OFF 0x32

// Set Intensity
#define SET_INTENSITY 0x40
#define SET_INTENSITY__GET_ON 0x41
#define SET_INTENSITY__ACCELERATE 0x42
#define SET_INTENSITY__TOGGLE_OFF 0x43
#define SET_INTENSITY__ACCELERATE_OTHER 0x44
#define SET_INTENSITY__GET_TARGET 0x45
#define SET_INTENSITY__FINALIZE 0x46

// Calibrate
#define CALIBRATE 0x50
#define CALIBRATE__ACCELERATE 0x51
#define CALIBRATE__TO_LIMIT 0x52
#define CALIBRATE__TOGGLE_OFF 0x53
#define CALIBRATE__ACCELERATE_OTHER 0x54
#define CALIBRATE__TO_OTHER_LIMIT 0x55


//
// operation statuses
//
#define OPERATION_STATUS__UNDEFINED 0x00
#define OPERATION_STATUS__SUCCESFULL 0x80
#define OPERATION_STATUS__NOT_ON 0x85
#define OPERATION_STATUS__ALREADY_ON 0x90
#define OPERATION_STATUS__NOT_OFF 0x95
#define OPERATION_STATUS__ALREADY_OFF 0xA0
#define OPERATION_STATUS__ALREADY_ON_TARGET 0xA5
#define OPERATION_STATUS__ON_TARGET 0xB0
#define OPERATION_STATUS__OFF_TARGET 0xB5
#define OPERATION_STATUS__TIMEOUT 0xC0
#define OPERATION_STATUS__QUIT 0xC5



typedef struct
{
    char stateName;
    char operationStatus;
    int currentIntensity;

    char _switchPin;
    long _stateTimer;
    long _sequenceTimer;
    long _intensityChangeTimer;
    int _lastIntensity;
    int _intensityChange;
    int _targetIntensity;
    bool _targetAbove;
    os_timer_t _stateMachineTimer;
} LightSW;


void ICACHE_FLASH_ATTR LightSW_Initialize(LightSW *lamp, int period_ms);
void ICACHE_FLASH_ATTR LightSW_RunSequence(uint32_t *args);
int ICACHE_FLASH_ATTR LightSW_ReadIntensityCounts(void);
int ICACHE_FLASH_ATTR LightSW_ReadIntensityPercent(void);
void ICACHE_FLASH_ATTR LightSW_ToggleSwitch(LightSW *lamp);
void ICACHE_FLASH_ATTR LightSW_Power(LightSW *lamp, bool state);
void ICACHE_FLASH_ATTR LightSW_Calibrate(LightSW *lamp);
void ICACHE_FLASH_ATTR LightSW_SetIntensity(LightSW *lamp, int targetIntensity);
void ICACHE_FLASH_ATTR LightSW_Quit(LightSW *lamp);

#endif
