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

	// Stop Method : 정지 방법 지정
	MM_SM_enMask		= 0x06,
	MM_SM_enShift		= 1,
	MM_SM_enNone		= 0x00,
	MM_SM_enPulse		= 0x02,		// 펄스 이송
	MM_SM_enSensor		= 0x04,		// 센서 중지

	// Not Release : 정지시 잡고 있을지
	MM_NR_enMask		= 0x08,
	MM_NR_enShift		= 3,
	MM_NR_enNo			= 0x00,
	MM_NR_enYes			= 0x08,

	// Check Initial Position : 초기 위치 체크
	MM_CIP_enMask		= 0x10,
	MM_CIP_enShift		= 4,
	MM_CIP_enNo			= 0x00,
	MM_CIP_enYes		= 0x10,

	// Check Both Dark : 모든 센서 암체크 유무
	MM_CBD_enMask		= 0x20,
	MM_CBD_enShift		= 5,
	MM_CBD_enNo			= 0x00,
	MM_CBD_enYes		= 0x20,

	// Stop Sensor State : 모터 정지시 도달 위치 센서 상태
	MM_SSS_enMask		= 0x40,
	MM_SSS_enShift		= 6,
	MM_SSS_enDark		= 0x00,
	MM_SSS_enLight		= 0x40,

	// Check Stop Position : 기동시 도달 위치 체크
	MM_CSP_enMask		= 0x80,
	MM_CSP_enShift		= 7,
	MM_CSP_enNo			= 0x00,
	MM_CSP_enYes		= 0x80
} PIF_enMotorMode;


#endif  // PIF_MOTOR_H
