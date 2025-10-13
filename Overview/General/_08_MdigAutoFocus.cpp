/*****************************************************************************/
/*
 * 파일명: MdigAutoFocus.cpp
 *
 * 개요:
 *   - MdigFocus()를 사용해 자동 초점(autofocus)을 수행하는 예제.
 *   - 실제 렌즈 모터 제어/카메라 그랩 대신, 스무딩(Convolve) 강도를 바꿔
 *     초점 흐림/선명도를 시뮬레이션하여 포커스를 찾는다.
 *
 * 핵심 요약:
 *   - MdigFocus + 훅: MdigFocus()가 포커스 탐색을 진행하며 매 스텝마다
 *     MoveLensHookFunction()을 호출 → 렌즈 위치 변경과 프레임 취득(여기선 시뮬).
 *   - 시뮬 취득: SimulateGrabFromCamera()가 MimConvolve로 스무딩 횟수를 조절해
 *     초점 상태를 흉내냄(중앙 위치에서 가장 선명). MIL-Lite에선 Convolve 미지원.
 *   - 탐색 파라미터: 시작/최소/최대 위치, 변동 허용치, 모드/민감도를 조절해
 *     속도와 안정성을 균형 있게 설정(M_SMART_SCAN + 1).
 *   - 시각화: DrawCursor()가 오버레이로 현재 포커스 위치를 표시.
 *   - 결과: FocusPos(최적 위치), UserData.Iteration(탐색 스텝 수) 출력.
 *
 * 참고:
 *   - MIL-Lite 환경에선 렌즈 시뮬레이션이 지원되지 않으므로 실제 카메라 그랩으로 대체 필요.
 *
 * 저작권:
 *   © Matrox Electronic Systems Ltd., 1992-2025. All Rights Reserved
 */
#include <mil.h>

/* 소스 이미지 파일 */
#define IMAGE_FILE                     M_IMAGE_PATH MIL_TEXT("BaboonMono.mim")

/* 렌즈(메카니컬) 포커스 위치 범위/초기값 */
#define FOCUS_MAX_NB_POSITIONS         100
#define FOCUS_MIN_POSITION             0
#define FOCUS_MAX_POSITION             (FOCUS_MAX_NB_POSITIONS - 1)
#define FOCUS_START_POSITION           10

/* 오토포커스 탐색 속성(변동 허용치/모드/민감도) */
#define FOCUS_MAX_POSITION_VARIATION   M_DEFAULT
#define FOCUS_MODE                     M_SMART_SCAN
#define FOCUS_SENSITIVITY              1

/* 훅에 전달할 사용자 데이터 */
typedef struct
{
   MIL_ID  SourceImage;   /* 원본(선명) 이미지 */
   MIL_ID  FocusImage;    /* 현재 포커스 상태(디스플레이 대상) */
   MIL_ID  Display;       /* 디스플레이 ID(오버레이 표시에 사용) */
   long    Iteration;     /* 포커스 탐색 스텝 수(훅 호출 시 증가) */
} DigHookUserData;

/* 렌즈 이동 훅(콜백) – 포커스 위치가 바뀔 때마다 호출됨 */
MIL_INT MFTYPE MoveLensHookFunction(MIL_INT HookType,
                                    MIL_INT Position,
                                    void*   UserDataHookPtr);

/* 카메라 그랩 시뮬 – 렌즈 위치에 따라 스무딩 횟수를 달리해 초점 상태를 흉내냄 */
void SimulateGrabFromCamera(MIL_ID SourceImage,
                            MIL_ID FocusImage,
                            MIL_INT Iteration,
                            MIL_ID AnnotationDisplay);

/* 현재 포커스 위치 오버레이(커서) 그리기 */
void DrawCursor(MIL_ID AnnotationDisplay, MIL_INT Position);

/* ------------------------- 메인 엔트리 ------------------------- */
int MosMain(void)
{
   /* 기본 리소스 */
   MIL_ID  MilApplication;   /* 애플리케이션 */
   MIL_ID  MilSystem;        /* 시스템 */
   MIL_ID  MilDisplay;       /* 디스플레이 */
   MIL_ID  MilSource;        /* 원본 이미지(선명) */
   MIL_ID  MilCameraFocus;   /* 포커스 상태 이미지(표시용) */
   MIL_INT FocusPos;         /* 최적 포커스 위치 결과 */
   DigHookUserData UserData; /* 훅에 전달할 사용자 데이터 */

   /* 1) 기본 자원 할당 */
   MappAllocDefault(M_DEFAULT, &MilApplication, &MilSystem, &MilDisplay, M_NULL, M_NULL);

   /* 2) 소스/포커스 버퍼 로드 및 초기화 */
   MbufRestore(IMAGE_FILE, MilSystem, &MilSource);
   MbufRestore(IMAGE_FILE, MilSystem, &MilCameraFocus);
   MbufClear(MilCameraFocus, 0);

   /* 디스플레이에 포커스 버퍼 선택 */
   MdispSelect(MilDisplay, MilCameraFocus);

   /* 3) 시작 포커스 위치에서 1프레임 시뮬 취득 */
   SimulateGrabFromCamera(MilSource, MilCameraFocus, FOCUS_START_POSITION, MilDisplay);

   /* 4) 훅 데이터 준비 */
   UserData.SourceImage = MilSource;
   UserData.FocusImage  = MilCameraFocus;
   UserData.Iteration   = 0L;
   UserData.Display     = MilDisplay;

   /* 안내 메시지 */
   MosPrintf(MIL_TEXT("\nAUTOFOCUS:\n"));
   MosPrintf(MIL_TEXT("----------\n\n"));
   MosPrintf(MIL_TEXT("Automatic focusing operation will be done on this image.\n"));
   MosPrintf(MIL_TEXT("Press any key to continue.\n\n"));
   MosGetch();
   MosPrintf(MIL_TEXT("Autofocusing...\n\n"));

   /* 5) 오토포커스 실행
         - 실제 환경에선 MoveLensHookFunction()에서 모터를 구동하고, 카메라로 그랩해야 함.
         - 여기서는 훅에서 스무딩을 적용한 시뮬 취득으로 대체. */
   MdigFocus(M_NULL,                 /* 디지타이저 없음(시뮬) */
             MilCameraFocus,         /* 포커스 측정/표시 대상 버퍼 */
             M_DEFAULT,              /* 기본 측정 파라미터 */
             MoveLensHookFunction,   /* 렌즈 이동 훅 – 위치 변경 시 호출 */
             &UserData,              /* 훅 사용자 데이터 */
             FOCUS_MIN_POSITION,     /* 탐색 최소 위치 */
             FOCUS_START_POSITION,   /* 시작 위치 */
             FOCUS_MAX_POSITION,     /* 탐색 최대 위치 */
             FOCUS_MAX_POSITION_VARIATION,          /* 위치 변동 허용치 */
             FOCUS_MODE + FOCUS_SENSITIVITY,        /* 모드/민감도 */
             &FocusPos);             /* 결과: 최적 포커스 위치 */

   /* 6) 결과 출력 */
   MosPrintf(MIL_TEXT("The best focus position is %d.\n"), (int)FocusPos);
   MosPrintf(MIL_TEXT("The best focus position found in %d iterations.\n\n"),
             (int)UserData.Iteration);
   MosPrintf(MIL_TEXT("Press any key to end.\n"));
   MosGetch();

   /* 7) 자원 해제 */
   MbufFree(MilSource);
   MbufFree(MilCameraFocus);
   MappFreeDefault(MilApplication, MilSystem, MilDisplay, M_NULL, M_NULL);

   return 0;
}

/* ------------------------------------------------------------------ */
/* 렌즈 이동 훅: 포커스 위치가 바뀌거나(on focus) 변경될 때 호출됨     */
/*  - 실제 장비에선 'Position'으로 모터를 구동하고, 새 프레임을 취득.   */
/*  - 본 예제는 SimulateGrabFromCamera()로 대체(스무딩 정도 변경).     */
/* ------------------------------------------------------------------ */
MIL_INT MFTYPE MoveLensHookFunction(MIL_INT HookType,
                                    MIL_INT Position,
                                    void*   UserDataHookPtr)
{
   DigHookUserData* UserData = (DigHookUserData*)UserDataHookPtr;

   /* 위치 변경(M_CHANGE) 또는 포커스 측정 시점(M_ON_FOCUS)에 반응 */
   if (HookType == M_CHANGE || HookType == M_ON_FOCUS)
   {
      /* (시뮬) 렌즈 위치 변경 + 프레임 취득 */
      SimulateGrabFromCamera(UserData->SourceImage,
                             UserData->FocusImage,
                             Position,
                             UserData->Display);
      /* 탐색 스텝 카운트 증가 */
      UserData->Iteration++;
   }
   return 0;
}

/* ------------------------------------------------------------------ */
/* 카메라 그랩 시뮬레이션: 스무딩 반복 횟수로 초점 정도를 흉내냄       */
/*  - 중앙 위치(FOCUS_BEST_POSITION)가 가장 선명(스무딩 0).            */
/*  - MIL-Lite에서는 MimConvolve 미지원 → 실제 그랩으로 대체 필요.     */
/* ------------------------------------------------------------------ */

/* 중앙(베스트) 포커스 위치 */
#define FOCUS_BEST_POSITION   (FOCUS_MAX_NB_POSITIONS/2)

void SimulateGrabFromCamera(MIL_ID SourceImage,
                            MIL_ID FocusImage,
                            MIL_INT Iteration,
                            MIL_ID AnnotationDisplay)
{
   MIL_INT NbSmoothNeeded;   /* 필요한 스무딩 횟수(초점 흐림 정도) */
   MIL_INT BufType;          /* 버퍼 타입 */
   MIL_INT BufSizeX, BufSizeY;
   MIL_INT Smooth;           /* 반복 인덱스 */
   MIL_ID  TempBuffer;       /* 임시 버퍼 */
   MIL_ID  SourceOwnerSystem;

   /* MIL-Lite 환경에서는 시뮬 불가(Convolve 미지원) → 컴파일 에러 유도 */
#if (M_MIL_LITE)
# error "Replace the SimulateGrabFromCamera() function with a true image grab."
#endif

   /* 초점 중심으로부터의 거리만큼 스무딩 횟수 증가 → 초점이 멀수록 흐림 */
   NbSmoothNeeded = MosAbs(Iteration - FOCUS_BEST_POSITION);

   /* 버퍼 정보 조회 */
   BufType  = MbufInquire(FocusImage, M_TYPE,   M_NULL);
   BufSizeX = MbufInquire(FocusImage, M_SIZE_X, M_NULL);
   BufSizeY = MbufInquire(FocusImage, M_SIZE_Y, M_NULL);

   if (NbSmoothNeeded == 0)
   {
      /* 최적 위치: 원본을 그대로 복사(가장 선명) */
      MbufCopy(SourceImage, FocusImage);
   }
   else if (NbSmoothNeeded == 1)
   {
      /* 가벼운 흐림 1회 */
      MimConvolve(SourceImage, FocusImage, M_SMOOTH);
   }
   else
   {
      /* 스무딩 다회 적용: 임시 버퍼 사용 */
      SourceOwnerSystem = (MIL_ID)MbufInquire(SourceImage, M_OWNER_SYSTEM, M_NULL);

      /* 임시 버퍼 할당 */
      MbufAlloc2d(SourceOwnerSystem, BufSizeX, BufSizeY, BufType, M_IMAGE + M_PROC, &TempBuffer);

      /* 첫 스무딩: Source → Temp */
      MimConvolve(SourceImage, TempBuffer, M_SMOOTH);

      /* 중간 스무딩 반복: Temp → Temp */
      for (Smooth = 1; Smooth < NbSmoothNeeded - 1; Smooth++)
         MimConvolve(TempBuffer, TempBuffer, M_SMOOTH);

      /* 마지막 스무딩: Temp → FocusImage */
      MimConvolve(TempBuffer, FocusImage, M_SMOOTH);

      /* 임시 버퍼 해제 */
      MbufFree(TempBuffer);
   }

   /* 현재 포커스 위치를 오버레이로 표시 */
   DrawCursor(AnnotationDisplay, Iteration);
}

/* --------------------------------------------------------------- */
/* 포커스 위치 커서(오버레이) 그리기                               */
/*   - 화면 하단 7/8 높이에 수평선 + 현재 위치를 가리키는 화살표   */
/* --------------------------------------------------------------- */

/* 커서 스타일 */
#define CURSOR_POSITION   ((BufSizeY*7)/8)
#define CURSOR_SIZE       14
#define CURSOR_COLOR      M_COLOR_GREEN

void DrawCursor(MIL_ID AnnotationDisplay, MIL_INT Position)
{
   MIL_ID     AnnotationImage;
   MIL_INT    BufSizeX, BufSizeY, n;
   MIL_DOUBLE CursorColor;

   /* 오버레이 활성화 및 초기화 */
   MdispControl(AnnotationDisplay, M_OVERLAY, M_ENABLE);
   MdispControl(AnnotationDisplay, M_OVERLAY_CLEAR, M_DEFAULT);
   MdispInquire(AnnotationDisplay, M_OVERLAY_ID, &AnnotationImage);
   MbufInquire(AnnotationImage, M_SIZE_X, &BufSizeX);
   MbufInquire(AnnotationImage, M_SIZE_Y, &BufSizeY);

   /* 그리기 색상 설정 */
   CursorColor = CURSOR_COLOR;
   MgraControl(M_DEFAULT, M_COLOR, CursorColor);

   /* 위치 → 픽셀 좌표 스케일링 */
   n = (BufSizeX / FOCUS_MAX_NB_POSITIONS);

   /* 수평 기준선 */
   MgraLine(M_DEFAULT, AnnotationImage,
            0,                 CURSOR_POSITION + CURSOR_SIZE,
            BufSizeX - 1,     CURSOR_POSITION + CURSOR_SIZE);

   /* 화살표(좌측 사선) */
   MgraLine(M_DEFAULT, AnnotationImage,
            Position*n,                CURSOR_POSITION + CURSOR_SIZE,
            Position*n - CURSOR_SIZE,  CURSOR_POSITION);

   /* 화살표(우측 사선) */
   MgraLine(M_DEFAULT, AnnotationImage,
            Position*n,                CURSOR_POSITION + CURSOR_SIZE,
            Position*n + CURSOR_SIZE,  CURSOR_POSITION);

   /* 화살표 꼭대기 수평선 */
   MgraLine(M_DEFAULT, AnnotationImage,
            Position*n - CURSOR_SIZE,  CURSOR_POSITION,
            Position*n + CURSOR_SIZE,  CURSOR_POSITION);
}
