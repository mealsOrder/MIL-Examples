/***************************************************************************************/
/*
 * 파일명: MdigDoubleBuffering.cpp
 *
 * 개요:
 *   - 2개의 타깃 버퍼를 교대로 사용하여(더블 버퍼링) 한쪽은 처리, 한쪽은 취득을 수행.
 *   - 프레임 시작 훅(M_GRAB_START)을 이용해 현재 취득 중 프레임 인덱스를 출력.
 *
 * 핵심 요약:
 *   - 더블 버퍼링: MilImage[0]/[1]을 번갈아 사용하여 "취득 ↔ 처리"를 오버랩(동시에 진행) 해 지연을 완화.
 *   - 비동기 그랩: MdigControl(..., M_GRAB_MODE, M_ASYNCHRONOUS)로 취득을 비동기화하여 처리와 겹침 허용.
 *   - 프레임 시작 훅: MdigHookFunction(..., M_GRAB_START, ...)으로 프레임 시작 시점 콜백(인덱스 출력).
 *   - 동기 타이머: MappTimer(M_SYNCHRONOUS)로 그랩 동기화된 시간 측정 → FPS/프레임당 ms 계산.
 *   - 주의: 실시간 견고성에는 MdigProcess() + 멀티버퍼링 권장(본 예제는 데모/경량 처리에 적합).
 *
 * 저작권:
 *   © Matrox Electronic Systems Ltd., 1992-2025. All Rights Reserved
 */
#include <mil.h>
#include <stdlib.h>

/* 프레임 시작 훅 콜백 및 사용자 데이터 구조체 */
MIL_INT MFTYPE GrabStart(MIL_INT, MIL_ID, void*);
typedef struct
{
   MIL_INT NbGrabStart; /* 시작된 프레임 수 누적 */
}  UserDataStruct;

#define STRING_LENGTH_MAX  20

/* 메인 함수 */
int MosMain(void)
{
   /* 기본 리소스 ID */
   MIL_ID MilApplication;
   MIL_ID MilSystem;
   MIL_ID MilDigitizer;
   MIL_ID MilDisplay;
   MIL_ID MilImage[2];   /* 더블 버퍼링용 그랩/처리 버퍼 2개 */
   MIL_ID MilImageDisp;  /* 디스플레이 버퍼 */

   /* 루프/통계 */
   long        NbProc = 0;  /* 처리한 프레임 수 */
   long        n = 0;       /* 현재 처리/토글 인덱스 (0 또는 1) */
   MIL_DOUBLE  Time = 0.0;  /* 총 경과 시간(초) */
   MIL_TEXT_CHAR Text[STRING_LENGTH_MAX] = MIL_TEXT("0");
   UserDataStruct UserStruct;

   /* 1) 기본 리소스 할당 (App/System/Display/Digitizer) */
   MappAllocDefault(M_DEFAULT, &MilApplication, &MilSystem, &MilDisplay,
                                      &MilDigitizer, M_NULL);

   /* 2) 단일 채널(8bit) 디스플레이 버퍼 할당 및 초기화 */
   MbufAlloc2d(MilSystem,
               MdigInquire(MilDigitizer, M_SIZE_X, M_NULL),
               MdigInquire(MilDigitizer, M_SIZE_Y, M_NULL),
               8 + M_UNSIGNED,
               M_IMAGE + M_PROC + M_DISP,
               &MilImageDisp);
   MbufClear(MilImageDisp, M_COLOR_BLACK);

   /* 디스플레이 선택 */
   MdispSelect(MilDisplay, MilImageDisp);

   /* 3) 더블 버퍼링용 그랩 버퍼 2개 할당 (GRAB+PROC) */
   for (n = 0; n < 2; n++)
   {
       MbufAlloc2d(MilSystem,
                   MdigInquire(MilDigitizer, M_SIZE_X, M_NULL),
                   MdigInquire(MilDigitizer, M_SIZE_Y, M_NULL),
                   8L + M_UNSIGNED,
                   M_IMAGE + M_GRAB + M_PROC,
                   &MilImage[n]);
   }

   /* 4) 프레임 시작 훅 등록: 각 프레임 시작 시 인덱스/카운트 출력 */
   UserStruct.NbGrabStart = 0;
   MdigHookFunction(MilDigitizer, M_GRAB_START, GrabStart, (void*)(&UserStruct));

   /* 안내 메시지 */
   MosPrintf(MIL_TEXT("\nDOUBLE BUFFERING ACQUISITION AND PROCESSING:\n"));
   MosPrintf(MIL_TEXT("--------------------------------------------\n\n"));
   MosPrintf(MIL_TEXT("Press any key to stop.\n\n"));

   /* 5) 비동기 그랩 모드: 취득과 처리를 오버랩하기 위해 필수 */
   MdigControl(MilDigitizer, M_GRAB_MODE, M_ASYNCHRONOUS);

   /* 6) 첫 프레임 취득 시작 (다음 루프에서 반대 버퍼를 그랩하며 현재 버퍼 처리) */
   MdigGrab(MilDigitizer, MilImage[0]);

   /* 7) 처리/취득 교대 루프 */
   n = 0;
   do
   {
      /* (A) 다음 버퍼로 취득 시작: 처리와 취득이 오버랩됨 */
      MdigGrab(MilDigitizer, MilImage[1 - n]);

      /* (B) 첫 사이클에서 동기 타이머 리셋(그랩과 동기화된 경과시간 측정) */
      if (NbProc == 0)
         MappTimer(M_DEFAULT, M_TIMER_RESET + M_SYNCHRONOUS, M_NULL);

      /* (C) (옵션) 프레임 번호 오버레이 – 성능이 중요하면 제거 권장 */
      MosSprintf(Text, STRING_LENGTH_MAX, MIL_TEXT("%ld"), NbProc + 1);
      MgraText(M_DEFAULT, MilImage[n], 32, 32, Text);

      /* (D) 사용자 처리 예시: 반전(NOT) → 디스플레이로 전송
            - 실제 프로젝트에서는 원하는 처리(필터/측정/검사 등)로 교체 */
      MimArith(MilImage[n], M_NULL, MilImageDisp, M_NOT);

      /* (E) 처리 프레임 수 증가 및 버퍼 토글 */
      NbProc++;
      n = 1 - n;
   }
   while (!MosKbhit());  /* 키 입력 시 종료 */

   /* 8) 마지막 그랩 완료 대기 및 동기 타이머 읽기 */
   MdigGrabWait(MilDigitizer, M_GRAB_END);
   MappTimer(M_DEFAULT, M_TIMER_READ + M_SYNCHRONOUS, &Time);
   MosGetch();

   /* 9) 통계 출력: 총 프레임, FPS, 프레임당 ms */
   MosPrintf(MIL_TEXT("%ld frames processed, at a frame rate of %.2f frames/sec ")
             MIL_TEXT("(%.2f ms/frame).\n"),
             NbProc, NbProc / Time, 1000.0 * Time / NbProc);
   MosPrintf(MIL_TEXT("Press any key to end.\n\n"));
   MosGetch();

   /* 10) 프레임 시작 훅 해제 */
   MdigHookFunction(MilDigitizer, M_GRAB_START + M_UNHOOK, GrabStart, (void*)(&UserStruct));

   /* 11) 버퍼/리소스 해제 (2버퍼 → 디스플레이 → 기본 리소스) */
   for (n = 0; n < 2; n++)
       MbufFree(MilImage[n]);
   MbufFree(MilImageDisp);
   MappFreeDefault(MilApplication, MilSystem, MilDisplay, MilDigitizer, M_NULL);

   return 0;
}

/* ---------------------------------------------------------------------------------
 * GrabStart 훅 함수: 각 프레임이 "시작"될 때 호출됨
 *  - 목적: 프레임 시작 이벤트 카운트/인덱스를 실시간으로 확인(진단/로그)
 * --------------------------------------------------------------------------------- */
MIL_INT MFTYPE GrabStart(MIL_INT HookType, MIL_ID EventId, void* UserStructPtr)
{
   UserDataStruct* UserPtr = (UserDataStruct*)UserStructPtr;

   /* 프레임 시작 카운트 증가 및 진행상황 출력 */
   UserPtr->NbGrabStart++;
   MosPrintf(MIL_TEXT("#%d\r"), (int)UserPtr->NbGrabStart);

   return 0;
}
