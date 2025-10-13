/***************************************************************************************/
/*
 * 파일명: MdigProcess.cpp
 *
 * 개요:
 *   - MdigProcess()와 멀티버퍼링(다중 그랩 버퍼)을 이용해 견고한 실시간 처리 파이프라인을 구현하는 예제.
 *   - 새 프레임이 준비될 때마다 콜백(ProcessingFunction)이 호출되어 사용자 처리 코드를 실행.
 *
 * 핵심 요약:
 *   - 멀티버퍼링: BUFFERING_SIZE_MAX만큼 그랩 버퍼를 준비해 파이프라인 처리 → 실시간성 향상, 프레임 드롭 완화.
 *   - 콜백 처리: ProcessingFunction에서 매 프레임 사용자 처리(예: MimArith NOT) 및 디스플레이 갱신.
 *   - 처리시간 주의: 평균 처리시간 < 프레임 간격이어야 누락 프레임 방지. 콘솔 출력/오버레이 제거 시 CPU 사용률 ↓.
 *   - 통계 조회: MdigInquire로 처리 프레임 수/프레임레이트(ms/frame) 확인.
 *   - 전형적 흐름: 리소스 할당 → 프리뷰 → 정지 → 멀티버퍼 할당 → MdigProcess 시작 → 키로 정지 → 통계/정리.
 *
 * 저작권:
 *   © Matrox Electronic Systems Ltd., 1992-2025. All Rights Reserved
 */
#include <mil.h>

/* 멀티버퍼 큐 크기(클수록 실시간성 ↑, 메모리 사용 ↑) */
#define BUFFERING_SIZE_MAX 20

/* 사용자 처리 콜백 프로토타입 */
MIL_INT MFTYPE ProcessingFunction(MIL_INT HookType, MIL_ID HookId, void* HookDataPtr);

/* 콜백에서 사용할 사용자 데이터 구조체 */
typedef struct
{
   MIL_ID  MilImageDisp;          /* 디스플레이용 이미지 버퍼 */
   MIL_INT ProcessedImageCount;   /* 처리된 프레임 수 */
} HookDataStruct;

/* 메인 함수 */
int MosMain(void)
{
   /* 기본 리소스 ID들 */
   MIL_ID MilApplication;
   MIL_ID MilSystem;
   MIL_ID MilDigitizer;
   MIL_ID MilDisplay;
   MIL_ID MilImageDisp;

   /* 멀티버퍼 그랩 리스트 */
   MIL_ID MilGrabBufferList[BUFFERING_SIZE_MAX] = { 0 };
   MIL_INT MilGrabBufferListSize;

   /* 통계 변수 */
   MIL_INT    ProcessFrameCount   = 0;
   MIL_DOUBLE ProcessFrameRate    = 0.0;

   /* 콜백 데이터 */
   HookDataStruct UserHookData;

   /* 1) 기본 리소스 할당 (App/System/Display/Digitizer) */
   MappAllocDefault(M_DEFAULT, &MilApplication, &MilSystem, &MilDisplay,
                                        &MilDigitizer, M_NULL);

   /* 2) 단일 채널(8bit) 디스플레이 버퍼 할당 및 초기화 */
   MbufAlloc2d(MilSystem,
               MdigInquire(MilDigitizer, M_SIZE_X, M_NULL),
               MdigInquire(MilDigitizer, M_SIZE_Y, M_NULL),
               8 + M_UNSIGNED,
               M_IMAGE + M_GRAB + M_PROC + M_DISP,
               &MilImageDisp);
   MbufClear(MilImageDisp, M_COLOR_BLACK);

   /* 디스플레이 선택 */
   MdispSelect(MilDisplay, MilImageDisp);

   /* 안내 메시지 */
   MosPrintf(MIL_TEXT("\nMULTIPLE BUFFERED PROCESSING.\n"));
   MosPrintf(MIL_TEXT("-----------------------------\n\n"));
   MosPrintf(MIL_TEXT("Press any key to start processing.\n\n"));

   /* 3) 프리뷰: 디스플레이 버퍼로 연속 취득 후 키 입력 대기 */
   MdigGrabContinuous(MilDigitizer, MilImageDisp);
   MosGetch();

   /* 프리뷰 정지 */
   MdigHalt(MilDigitizer);

   /* 4) 멀티버퍼 그랩 버퍼 할당 (최대 BUFFERING_SIZE_MAX개) */
   for (MilGrabBufferListSize = 0;
        MilGrabBufferListSize < BUFFERING_SIZE_MAX;
        MilGrabBufferListSize++)
   {
      /* 최소 2개 이후부터는 에러 출력 억제(최대한 많이 할당 시도) */
      if (MilGrabBufferListSize == 2)
         MappControl(M_DEFAULT, M_ERROR, M_PRINT_DISABLE);

      MbufAlloc2d(MilSystem,
                  MdigInquire(MilDigitizer, M_SIZE_X, M_NULL),
                  MdigInquire(MilDigitizer, M_SIZE_Y, M_NULL),
                  8 + M_UNSIGNED,
                  M_IMAGE + M_GRAB + M_PROC,
                  &MilGrabBufferList[MilGrabBufferListSize]);

      if (MilGrabBufferList[MilGrabBufferListSize])
         MbufClear(MilGrabBufferList[MilGrabBufferListSize], 0xFF);
      else
         break;
   }
   MappControl(M_DEFAULT, M_ERROR, M_PRINT_ENABLE);

   /* 5) 콜백에 전달할 데이터 초기화 */
   UserHookData.MilImageDisp        = MilImageDisp;
   UserHookData.ProcessedImageCount = 0;

   /* 6) 실시간 처리 시작
         - 각 프레임이 도착할 때마다 ProcessingFunction 콜백 호출 */
   MdigProcess(MilDigitizer, MilGrabBufferList, MilGrabBufferListSize,
               M_START, M_DEFAULT, ProcessingFunction, &UserHookData);

   /* (메인 스레드는 여기서 다른 작업을 수행할 수도 있음) */

   /* 7) 키 입력으로 정지 */
   MosPrintf(MIL_TEXT("Press any key to stop.                    \n\n"));
   MosGetch();

   /* 실시간 처리 정지 */
   MdigProcess(MilDigitizer, MilGrabBufferList, MilGrabBufferListSize,
               M_STOP, M_DEFAULT, ProcessingFunction, &UserHookData);

   /* 8) 통계 출력: 처리된 프레임 수 및 프레임레이트 */
   MdigInquire(MilDigitizer, M_PROCESS_FRAME_COUNT,  &ProcessFrameCount);
   MdigInquire(MilDigitizer, M_PROCESS_FRAME_RATE,   &ProcessFrameRate);
   MosPrintf(MIL_TEXT("\n\n%d frames grabbed at %.1f frames/sec (%.1f ms/frame).\n"),
             (int)ProcessFrameCount, ProcessFrameRate, 1000.0/ProcessFrameRate);
   MosPrintf(MIL_TEXT("Press any key to end.\n\n"));
   MosGetch();

   /* 9) 자원 해제: 그랩 버퍼들 → 디스플레이 버퍼 → 기본 리소스 */
   while (MilGrabBufferListSize > 0)
      MbufFree(MilGrabBufferList[--MilGrabBufferListSize]);

   MbufFree(MilImageDisp);
   MappFreeDefault(MilApplication, MilSystem, MilDisplay, MilDigitizer, M_NULL);

   return 0;
}

/* -------------------------------------------------------------------- */
/* 사용자 처리 콜백: 그랩 버퍼가 준비될 때마다 호출됨                   */
/* -------------------------------------------------------------------- */

/* 오버레이 문자열 표시용 상수 */
#define STRING_LENGTH_MAX  20
#define STRING_POS_X       20
#define STRING_POS_Y       20

MIL_INT MFTYPE ProcessingFunction(MIL_INT HookType, MIL_ID HookId, void* HookDataPtr)
{
   HookDataStruct* UserHookDataPtr = (HookDataStruct*)HookDataPtr;
   MIL_ID ModifiedBufferId;
   MIL_TEXT_CHAR Text[STRING_LENGTH_MAX] = { MIL_TEXT('\0'), };

   /* 1) 방금 완료된 그랩 버퍼 ID 조회 */
   MdigGetHookInfo(HookId, M_MODIFIED_BUFFER + M_BUFFER_ID, &ModifiedBufferId);

   /* 2) 프레임 카운트 증가 */
   UserHookDataPtr->ProcessedImageCount++;

   /* 3) (옵션) 콘솔/오버레이 표시 — 성능 최적화 필요 시 제거 권장 */
   MosPrintf(MIL_TEXT("Processing frame #%d.\r"),
             (int)UserHookDataPtr->ProcessedImageCount);
   MosSprintf(Text, STRING_LENGTH_MAX, MIL_TEXT("%d"),
              (int)UserHookDataPtr->ProcessedImageCount);
   MgraText(M_DEFAULT, ModifiedBufferId, STRING_POS_X, STRING_POS_Y, Text);

   /* 4) 사용자 처리 예시: NOT 연산 후 디스플레이 업데이트
         - 실제 프로젝트에서는 원하는 처리(MimFilter/Blob 등)로 교체 */
   MimArith(ModifiedBufferId, M_NULL, UserHookDataPtr->MilImageDisp, M_NOT);

   return 0;
}
