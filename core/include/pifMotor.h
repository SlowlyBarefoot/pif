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
    MS_enStop		= 10    // 정지
} PIF_enMotorState;

typedef enum _PIF_enMotorMode
{
	MM_enDefault		= 0x00,

	// Direction : 방향
	MM_D_enMask			= 0x01,
	MM_D_enShift		= 0,
	MM_D_enCW			= 0x00,
	MM_D_enCCW			= 0x01,

	// Reduce Method : 감속 방법 지정
	MM_RM_enMask		= 0x06,
	MM_RM_enShift		= 1,
	MM_RM_enNone		= 0x00,
	MM_RM_enTime		= 0x02,		// 일정 시간후 중지
	MM_RM_enSensor		= 0x04,		// 센서 중지
	MM_RM_enPulse		= 0x06,		// 펄스 이동

	// Not Release : 정지시 잡고 있을지
	MM_NR_enMask		= 0x08,
	MM_NR_enShift		= 3,
	MM_NR_enNo			= 0x00,
	MM_NR_enYes			= 0x08,

	// Check Initial All Sensor : 초기 모든 센서 체크
	// Start 센서만 Dark이고 Reduce와 Stop 센서는 Light이어야 함
	MM_CIAS_enMask		= 0x10,
	MM_CIAS_enShift		= 4,
	MM_CIAS_enNo		= 0x00,
	MM_CIAS_enYes		= 0x10,

	// Check Finish Stop Sensor : 도달시 Stop 센서 체크
	MM_CFPS_enMask		= 0x20,
	MM_CFPS_enShift		= 5,
	MM_CFPS_enNo		= 0x00,
	MM_CFPS_enYes		= 0x20,
} PIF_enMotorMode;


#endif  // PIF_MOTOR_H
