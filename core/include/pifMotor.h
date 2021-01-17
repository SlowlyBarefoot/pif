#ifndef PIF_MOTOR_H
#define PIF_MOTOR_H


#include "pif.h"


typedef enum _PIF_enMotorState
{
    MS_enIdle		= 0,
    MS_enGained		= 1,    // 가속 구간
    MS_enStable		= 2,    // 정속 상태 확인
    MS_enConst		= 3,    // 정속 구간
    MS_enReduce		= 4,    // 감속 구간
    MS_enLowConst	= 5,    // 감속후 정위치까지 도달하기 위한 구간
    MS_enOverRun	= 6,    // 정지 센서 감지후 일정 거리 이동
    MS_enBreak		= 7,    // 브레이크 잡을 준비하다.
    MS_enBreaking	= 8,    // 브레이크를 잡다
    MS_enStopping	= 9,    // 정지중
    MS_enStop		= 10,   // 정지
    MS_enCount		= 11
} PIF_enMotorState;

typedef enum _PIF_enMotorMode
{
	// Direction : 방향
	MM_D_enMask			= 0x01,
	MM_D_enShift		= 0,
	MM_D_enCW			= 0x00,
	MM_D_enCCW			= 0x01,

	// Speed Control : 속도 제어 여부
	MM_SC_enMask		= 0x02,
	MM_SC_enShift		= 1,
	MM_SC_enNo			= 0x00,
	MM_SC_enYes			= 0x02,

	// Position Control : 위치 제어 여부
	MM_PC_enMask		= 0x04,
	MM_PC_enShift		= 2,
	MM_PC_enNo			= 0x00,
	MM_PC_enYes			= 0x04,

	// Reduce Time : 감속 시점 지정
	MM_RT_enMask		= 0x18,
	MM_RT_enShift		= 3,
	MM_RT_enNone		= 0x00,
	MM_RT_enTime		= 0x08,		// 일정 시간후 중지

	// Not Release : 정지시 잡고 있을지
	MM_NR_enMask		= 0x20,
	MM_NR_enShift		= 5,
	MM_NR_enNo			= 0x00,
	MM_NR_enYes			= 0x20,

	// Check Initial All Sensor : 초기 모든 센서 체크
	// Start 센서만 Dark이고 Reduce와 Stop 센서는 Light이어야 함
	MM_CIAS_enMask		= 0x40,
	MM_CIAS_enShift		= 6,
	MM_CIAS_enNo		= 0x00,
	MM_CIAS_enYes		= 0x40,

	// Check Finish Stop Sensor : 도달시 Stop 센서 체크
	MM_CFPS_enMask		= 0x80,
	MM_CFPS_enShift		= 7,
	MM_CFPS_enNo		= 0x00,
	MM_CFPS_enYes		= 0x80,
} PIF_enMotorMode;


extern const char *c_cMotorState[MS_enCount];


#endif  // PIF_MOTOR_H
