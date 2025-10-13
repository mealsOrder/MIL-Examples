/********************************************************************************/
/* 
 * 파일명: MDigGrab.cpp 
 *
 * 개요:
 *   - 카메라로부터 영상을 연속(continuous) 및 단발(monoshot) 모드로 취득하는 예제.
 *   - 콘솔 입력으로 취득 시작/정지/단발 순서를 제어하고, 디스플레이에 결과를 표시.
 *
 * 핵심 요약:
 *   - MappAllocDefault: 애플리케이션/시스템/디스플레이/디지타이저/이미지를 한 번에 할당.
 *   - MdigGrabContinuous: 연속 취득 시작 → MilImage로 프레임이 계속 들어옴.
 *   - MdigHalt: 연속 취득 중단.
 *   - MdigGrab: 단발 취득(한 프레임) 수행.
 *   - MosGetch & 콘솔 메시지: 사용자 입력으로 흐름 제어, 상태 안내.
 *   - MappFreeDefault: 모든 리소스 해제(정리).
 *
 * 저작권:
 *   © Matrox Electronic Systems Ltd., 1992-2025. All Rights Reserved
 */
#include <mil.h> 

int MosMain(void)
{ 
   /* MIL 리소스 식별자 */
   MIL_ID MilApplication;  /* 애플리케이션 ID */
   MIL_ID MilSystem;       /* 시스템 ID */
   MIL_ID MilDisplay;      /* 디스플레이 ID */
   MIL_ID MilDigitizer;    /* 디지타이저(카메라) ID */ 
   MIL_ID MilImage;        /* 이미지 버퍼 ID */

   /* 1) 기본 리소스 할당
      - 애플리케이션, 시스템, 디스플레이, 디지타이저, 이미지 버퍼를 한 번에 준비 */
   MappAllocDefault(M_DEFAULT, &MilApplication, &MilSystem,
                             &MilDisplay, &MilDigitizer, &MilImage);

   /* 2) 연속 취득 시작
      - 카메라 프레임이 MilImage로 계속 들어오며, 디스플레이에 표시됨 */
   MdigGrabContinuous(MilDigitizer, MilImage);

   /* 3) 사용자 입력 대기: 연속 취득 진행 안내 */
   MosPrintf(MIL_TEXT("\nDIGITIZER ACQUISITION:\n"));
   MosPrintf(MIL_TEXT("----------------------\n\n"));
   MosPrintf(MIL_TEXT("Continuous image grab in progress.\n"));
   MosPrintf(MIL_TEXT("Press any key to stop.\n\n"));
   MosGetch();

   /* 4) 연속 취득 중단 */
   MdigHalt(MilDigitizer);

   /* 5) 상태 안내 및 단발 취득 준비 */
   MosPrintf(MIL_TEXT("Continuous grab stopped.\n\n"));
   MosPrintf(MIL_TEXT("Press any key to do a single image grab.\n\n"));
   MosGetch();

   /* 6) 단발 취득(한 프레임 캡처) */
   MdigGrab(MilDigitizer, MilImage);

   /* 7) 결과 확인 후 종료 대기 */
   MosPrintf(MIL_TEXT("Displaying the grabbed image.\n"));
   MosPrintf(MIL_TEXT("Press any key to end.\n\n"));
   MosGetch();

   /* 8) 리소스 해제(정리) */
   MappFreeDefault(MilApplication, MilSystem, MilDisplay, MilDigitizer, MilImage);

   return 0;
}
