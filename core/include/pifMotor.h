#ifndef PIF_MOTOR_H
#define PIF_MOTOR_H


#include "pif.h"


typedef enum EnPifMotorState
{
    MS_IDLE			= 0,
    MS_GAINED		= 1,    // 가속 구간
    MS_STABLE		= 2,    // 정속 상태 확인
    MS_CONST		= 3,    // 정속 구간
    MS_REDUCE		= 4,    // 감속 구간
    MS_LOW_CONST	= 5,    // 감속후 정위치까지 도달하기 위한 구간
    MS_OVER_RUN		= 6,    // 정지 센서 감지후 일정 거리 이동
    MS_BREAK		= 7,    // 브레이크 잡을 준비하다.
    MS_BREAKING		= 8,    // 브레이크를 잡다
    MS_STOPPING		= 9,    // 정지중
    MS_STOP			= 10,   // 정지
    MS_COUNT		= 11
} PifMotorState;

typedef enum EnPifMotorMode
{
	// Direction : 방향
	MM_D_MASK			= 0x01,
	MM_D_SHIFT			= 0,
	MM_D_CW				= 0x00,
	MM_D_CCW			= 0x01,

	// Speed Control : 속도 제어 여부
	MM_SC_MASK			= 0x02,
	MM_SC_SHIFT			= 1,
	MM_SC_NO			= 0x00,
	MM_SC_YES			= 0x02,

	// Position Control : 위치 제어 여부
	MM_PC_MASK			= 0x04,
	MM_PC_SHIFT			= 2,
	MM_PC_NO			= 0x00,
	MM_PC_YES			= 0x04,

	// Reduce Time : 감속 시점 지정
	MM_RT_MASK			= 0x18,
	MM_RT_SHIFT			= 3,
	MM_RT_NONE			= 0x00,
	MM_RT_TIME			= 0x08,		// 일정 시간후 중지

	// Not Release : 정지시 잡고 있을지
	MM_NR_MASK			= 0x20,
	MM_NR_SHIFT			= 5,
	MM_NR_NO			= 0x00,
	MM_NR_YES			= 0x20,

	// Check Initial All Sensor : 초기 모든 센서 체크
	// Start 센서만 Dark이고 Reduce와 Stop 센서는 Light이어야 함
	MM_CIAS_MASK		= 0x40,
	MM_CIAS_SHIFT		= 6,
	MM_CIAS_NO			= 0x00,
	MM_CIAS_YES			= 0x40,

	// Check Finish Stop Sensor : 도달시 Stop 센서 체크
	MM_CFPS_MASK		= 0x80,
	MM_CFPS_SHIFT		= 7,
	MM_CFPS_NO			= 0x00,
	MM_CFPS_YES			= 0x80,
} PifMotorMode;


extern const char* kMotorState[MS_COUNT];


#endif  // PIF_MOTOR_H
