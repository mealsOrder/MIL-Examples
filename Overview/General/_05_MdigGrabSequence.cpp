/********************************************************************************/
/*
 * 파일명: MDigGrabSequence.cpp
 *
 * 개요:
 *   - 카메라로부터 취득한 프레임을 시퀀스로 기록하고, 동일 프레임레이트로 재생하는 예제.
 *   - 기록 대상은 메모리(무압축) 또는 AVI 파일(무압축/손실 JPEG/손실 JPEG2000) 선택 가능.
 *   - 매 프레임 도착 시 Hook(RecordFunction)을 호출하여 디스플레이/기록/압축을 수행.
 *
 * 핵심 요약:
 *   - MdigProcess + RecordFunction: 매 프레임을 Hook으로 받아 디스플레이 및 기록(메모리/파일) 처리.
 *   - 포맷 선택: 1) 메모리 무압축, 2) 파일 무압축, 3) JPEG(손실), 4) JPEG2000(손실) (라이선스 필요).
 *   - 멀티버퍼 그랩: NB_GRAB_IMAGE_MAX(기본 20)개 버퍼로 프레임 누락 최소화.
 *   - 프레임 주석: FRAME_NUMBER_ANNOTATION == M_YES 시, 프레임 번호 오버레이.
 *   - 재생(Playback): 기록된 프레임을 원 프레임레이트로 표시(파일은 ImportSequence, 메모리는 버퍼 복사).
 *   - 성능 주의: 디스크/시스템이 느리면 파일 기록 시 프레임 미스 발생 → 메모리 기록 권장, 주석/표시 최소화로 CPU 절감.
 *
 * 저작권:
 *   © Matrox Electronic Systems Ltd., 1992-2025. All Rights Reserved
 */
#include <mil.h>

/* 시퀀스 파일 이름(AVI) */
#define SEQUENCE_FILE M_TEMP_DIR MIL_TEXT("MilSequence.avi")

/* 압축 품질(Q-factor): 1~99 (클수록 낮은 품질/높은 압축) */
#define COMPRESSION_Q_FACTOR 50

/* 프레임 번호 주석 표시 여부 */
#define FRAME_NUMBER_ANNOTATION M_YES

/* 멀티버퍼 그랩 최대 이미지 수 */
#define NB_GRAB_IMAGE_MAX 20

/* 사용자 레코드 훅 함수 프로토타입(프레임마다 호출) */
MIL_INT MFTYPE RecordFunction(MIL_INT HookType, MIL_ID HookId, void* HookDataPtr);

/* 훅에 전달할 사용자 데이터 구조체 */
typedef struct
{
   MIL_ID  MilSystem;
   MIL_ID  MilDisplay;
   MIL_ID  MilImageDisp;        /* 디스플레이용 이미지 */
   MIL_ID  MilCompressedImage;  /* 압축 버퍼(선택) */
   MIL_INT NbGrabbedFrames;     /* 취득된 프레임 수 */
   MIL_INT SaveSequenceToDisk;  /* 파일 기록 여부 (M_YES/M_NO) */
} HookDataStruct;

/* 메인 함수 */
int MosMain(void)
{
   /* 기본 리소스 ID */
   MIL_ID  MilApplication, MilRemoteApplication, MilSystem, MilDigitizer, MilDisplay, MilImageDisp;
   MIL_ID  MilGrabImages[NB_GRAB_IMAGE_MAX];  /* 그랩 버퍼 배열 */
   MIL_ID  MilCompressedImage = M_NULL;       /* 압축 버퍼(옵션) */

   /* 제어/상태 변수 */
   MIL_INT  CompressAttribute = 0; /* 압축 속성(M_COMPRESS + M_JPEG_LOSSY 등) */
   MIL_INT  NbFrames = 0, Selection = 1, LicenseModules = 0, n = 0;
   MIL_INT  FrameCount = 0, FrameMissed = 0, NbFramesReplayed = 0, Exit = 0;
   MIL_DOUBLE FrameRate = 0.0, TimeWait = 0.0, TotalReplay = 0.0;
   MIL_INT  SaveSequenceToDisk = M_NO;
   HookDataStruct UserHookData;

   /* 1) 기본 리소스 할당 (App/System/Display/Digitizer) */
   MappAllocDefault(M_DEFAULT, &MilApplication, &MilSystem, &MilDisplay, &MilDigitizer, M_NULL);

   /* 2) 디스플레이용 이미지 버퍼 할당 및 선택 */
   MbufAllocColor(MilSystem,
                  MdigInquire(MilDigitizer, M_SIZE_BAND, M_NULL),
                  MdigInquire(MilDigitizer, M_SIZE_X,    M_NULL),
                  MdigInquire(MilDigitizer, M_SIZE_Y,    M_NULL),
                  8L + M_UNSIGNED,
                  M_IMAGE + M_GRAB + M_DISP,
                  &MilImageDisp);
   MbufClear(MilImageDisp, 0x0);
   MdispSelect(MilDisplay, MilImageDisp);

   /* (연습화면) 디스플레이에서 연속 취득 시작 */
   MdigGrabContinuous(MilDigitizer, MilImageDisp);

   /* 안내 메시지 */
   MosPrintf(MIL_TEXT("\nSEQUENCE ACQUISITION:\n"));
   MosPrintf(MIL_TEXT("--------------------\n\n"));

   /* 3) JPEG/J2K 라이선스 확인 */
   MsysInquire(MilSystem, M_OWNER_APPLICATION, &MilRemoteApplication);
   MappInquire(MilRemoteApplication, M_LICENSE_MODULES, &LicenseModules);

   /* 4) 기록 포맷 선택(키 입력) */
   MosPrintf(MIL_TEXT("Choose the sequence format:\n"));
   MosPrintf(MIL_TEXT("1) Uncompressed images to memory (up to %ld frames).\n"), NB_GRAB_IMAGE_MAX);
   MosPrintf(MIL_TEXT("2) Uncompressed images to an AVI file.\n"));
   if (LicenseModules & (M_LICENSE_JPEGSTD | M_LICENSE_JPEG2000))
   {
      if (LicenseModules & M_LICENSE_JPEGSTD)
         MosPrintf(MIL_TEXT("3) Compressed lossy JPEG images to an AVI file.\n"));
      if (LicenseModules & M_LICENSE_JPEG2000)
         MosPrintf(MIL_TEXT("4) Compressed lossy JPEG2000 images to an AVI file.\n"));
   }


   /* ---------------------------------------------------------------------------------
   * [시퀀스 기록 포맷 선택 루프]
   *  - 콘솔 키 입력(MosGetch)으로 포맷을 선택할 때까지 반복.
   *  - '1' 또는 Enter(\r)  : 메모리(무압축) 기록
   *  - '2'                : 파일(무압축) 기록
   *  - '3'                : 파일(JPEG 손실 압축) 기록  ※ JPEG 라이선스 필요
   *  - '4'                : 파일(JPEG2000 손실 압축) 기록 ※ JPEG2000 라이선스 필요
   *  - 그 외 입력          : 잘못된 선택 → 다시 입력 요구
   * 
   * [요약]
   *  - ValidSelection 플래그는 "올바른 선택이 들어왔는지"를 제어.
   *  - MosGetch()로 단일 키 입력을 받아 switch로 분기.
   *  - CompressAttribute: 이후 압축 버퍼 할당/기록에 사용(JPEG/J2K 설정).
   *  - SaveSequenceToDisk: M_YES(파일 기록) / M_NO(메모리 기록) 흐름 결정.
   *  - Enter 키(\r)를 '1'과 동등하게 취급하여 빠른 기본 선택(메모리 무압축).
   * ========================================================================= */

   bool ValidSelection = false;
   while (!ValidSelection)
   {
      /* 1) 키 입력 대기: 한 글자 입력을 즉시 읽는다(엔터 필요 없음) */
      Selection = MosGetch();
      ValidSelection = true;  // 일단 true로 두고, default에서 잘못된 입력이면 false로 되돌린다.

      /* 2) 입력값 스위치 처리 */
      switch (Selection)
      {
      /* 2-1) '1' 또는 Enter(\r) → 메모리(무압축)로 기록
         - CompressAttribute: 압축 없음(M_NULL)
         - SaveSequenceToDisk: 디스크에 저장하지 않음(M_NO)
         - 주의: 메모리에 NB_GRAB_IMAGE_MAX 만큼만 프레임이 저장되므로, 길게 녹화하려면 파일 기록을 사용 */
      case '1':
      case '\r':
         MosPrintf(MIL_TEXT("\nUncompressed images to memory selected.\n"));
         CompressAttribute   = M_NULL;
         SaveSequenceToDisk  = M_NO;
         break;

      /* 2-2) '2' → 파일(무압축) 기록
         - CompressAttribute: 압축 없음(M_NULL)
         - SaveSequenceToDisk: 디스크 저장(M_YES)
         - 주의: 디스크 쓰기 속도가 충분하지 않으면 프레임 미스가 날 수 있음 */
      case '2':
         MosPrintf(MIL_TEXT("\nUncompressed images to file selected.\n"));
         CompressAttribute   = M_NULL;
         SaveSequenceToDisk  = M_YES;
         break;

      /* 2-3) '3' → 파일(JPEG 손실 압축) 기록
         - CompressAttribute: M_COMPRESS + M_JPEG_LOSSY
         - SaveSequenceToDisk: 디스크 저장(M_YES)
         - 비트레이트/품질은 이후 M_Q_FACTOR(COMPRESSION_Q_FACTOR)로 제어 */
      case '3':
         MosPrintf(MIL_TEXT("\nJPEG images to file selected.\n"));
         CompressAttribute   = M_COMPRESS + M_JPEG_LOSSY;
         SaveSequenceToDisk  = M_YES;
         break;

      /* 2-4) '4' → 파일(JPEG2000 손실 압축) 기록
         - CompressAttribute: M_COMPRESS + M_JPEG2000_LOSSY
         - SaveSequenceToDisk: 디스크 저장(M_YES)
         - 코덱 라이선스가 없으면 메뉴에 표시되지 않거나 실패할 수 있음 */
      case '4':
         MosPrintf(MIL_TEXT("\nJPEG 2000 images to file selected.\n"));
         CompressAttribute   = M_COMPRESS + M_JPEG2000_LOSSY;
         SaveSequenceToDisk  = M_YES;
         break;

      /* 2-5) 이외 입력 → 잘못된 선택, 루프를 다시 진행하도록 플래그 false */
      default:
         MosPrintf(MIL_TEXT("\nInvalid selection !.\n"));
         ValidSelection = false;
         break;
      }
   }

   
   
   /* 5) 압축 버퍼 필요 시 할당 및 품질 설정 */
   if (CompressAttribute)
   {
      MbufAllocColor(MilSystem,
                     MdigInquire(MilDigitizer, M_SIZE_BAND, M_NULL),
                     MdigInquire(MilDigitizer, M_SIZE_X,    M_NULL),
                     MdigInquire(MilDigitizer, M_SIZE_Y,    M_NULL),
                     8L + M_UNSIGNED,
                     M_IMAGE + CompressAttribute,
                     &MilCompressedImage);
      MbufControl(MilCompressedImage, M_Q_FACTOR, COMPRESSION_Q_FACTOR);
   }

   /* 6) 시퀀스 저장용 그랩 버퍼 배열 할당 (멀티버퍼) */
   for (NbFrames = 0, n = 0; n < NB_GRAB_IMAGE_MAX; n++)
   {
      /* 최소 버퍼 수(2개) 이후부터는 에러 프린트 비활성로 계속 시도 */
      if (n == 2)
         MappControl(M_DEFAULT, M_ERROR, M_PRINT_DISABLE);

      MbufAllocColor(MilSystem,
                     MdigInquire(MilDigitizer, M_SIZE_BAND, M_NULL),
                     MdigInquire(MilDigitizer, M_SIZE_X,    M_NULL),
                     MdigInquire(MilDigitizer, M_SIZE_Y,    M_NULL),
                     8L + M_UNSIGNED,
                     M_IMAGE + M_GRAB,
                     &MilGrabImages[n]);

      if (MilGrabImages[n])
      {
         NbFrames++;
         MbufClear(MilGrabImages[n], 0xFF);
      }
      else
         break;
   }
   MappControl(M_DEFAULT, M_ERROR, M_PRINT_ENABLE);

   /* 연습화면 정지: 연속 취득 중단 */
   MdigHalt(MilDigitizer);

   /* 7) 파일 기록 모드라면 AVI 오픈 */
   if (SaveSequenceToDisk)
   {
      MosPrintf(MIL_TEXT("\nSaving the sequence to an AVI file...\n"));
      MbufExportSequence(SEQUENCE_FILE, M_DEFAULT, M_NULL, M_NULL, M_DEFAULT, M_OPEN);
   }
   else
   {
      MosPrintf(MIL_TEXT("\nSaving the sequence to memory...\n\n"));
   }

   /* 8) Hook 데이터 초기화 */
   UserHookData.MilSystem          = MilSystem;
   UserHookData.MilDisplay         = MilDisplay;
   UserHookData.MilImageDisp       = MilImageDisp;
   UserHookData.MilCompressedImage = MilCompressedImage;
   UserHookData.SaveSequenceToDisk = SaveSequenceToDisk;
   UserHookData.NbGrabbedFrames    = 0;

   /* 9) 시퀀스 취득 시작
         - 파일 기록: M_START(키로 정지)
         - 메모리 기록: M_SEQUENCE(NbFrames 채우면 자동 정지) */
   MdigProcess(MilDigitizer, MilGrabImages, NbFrames,
               SaveSequenceToDisk ? M_START : M_SEQUENCE,
               M_DEFAULT, RecordFunction, &UserHookData);

   /* 파일 기록 시: 키 입력 대기 후 정지 */
   if (SaveSequenceToDisk)
   {
      MosPrintf(MIL_TEXT("\nPress any key to stop recording.\n\n"));
      MosGetch();
   }

   /* 프레임레이트 유효값 확보를 위해 최소 2프레임까지 기다림 */
   do
   {
      MdigInquire(MilDigitizer, M_PROCESS_FRAME_COUNT, &FrameCount);
   }
   while (FrameCount < 2);

   /* 10) 시퀀스 취득 정지 */
   MdigProcess(MilDigitizer, MilGrabImages, NbFrames, M_STOP,
               M_DEFAULT, RecordFunction, &UserHookData);

   /* 11) 통계 출력 */
   MdigInquire(MilDigitizer, M_PROCESS_FRAME_COUNT,  &FrameCount);
   MdigInquire(MilDigitizer, M_PROCESS_FRAME_RATE,   &FrameRate);
   MdigInquire(MilDigitizer, M_PROCESS_FRAME_MISSED, &FrameMissed);
   MosPrintf(MIL_TEXT("\n\n%d frames recorded (%d missed), at %.1f frames/sec ")
             MIL_TEXT("(%.1f ms/frame).\n\n"),
             (int)UserHookData.NbGrabbedFrames, (int)FrameMissed, FrameRate, 1000.0/FrameRate);

   /* 파일 기록 시: AVI 클로즈(프레임레이트 기입) */
   if (SaveSequenceToDisk)
      MbufExportSequence(SEQUENCE_FILE, M_DEFAULT, M_NULL, M_NULL, FrameRate, M_CLOSE);

   /* 12) 재생 준비 */
   MosPrintf(MIL_TEXT("Press any key to start the sequence playback.\n"));
   MosGetch();

   /* 13) 재생 루프 (Enter로 종료, 다른 키면 재생 반복) */
   if (UserHookData.NbGrabbedFrames > 0)
   {
      MIL_INT KeyPressed = 0;
      do
      {
         /* 파일 기록 재생: 메타 조회 후 오픈 */
         if (SaveSequenceToDisk)
         {
            MosPrintf(MIL_TEXT("\nPlaying sequence from the AVI file...\n"));
            MosPrintf(MIL_TEXT("Press any key to end playback.\n\n"));
            MbufDiskInquire(SEQUENCE_FILE, M_NUMBER_OF_IMAGES, &FrameCount);
            MbufDiskInquire(SEQUENCE_FILE, M_FRAME_RATE,       &FrameRate);
            MbufDiskInquire(SEQUENCE_FILE, M_COMPRESSION_TYPE, &CompressAttribute);

            MbufImportSequence(SEQUENCE_FILE, M_DEFAULT, M_NULL,
                               M_NULL, M_NULL, M_NULL, M_NULL, M_OPEN);
         }
         else
         {
            MosPrintf(MIL_TEXT("\nPlaying sequence from memory...\n\n"));
            FrameCount = NbFrames; /* 메모리 기록은 버퍼 수만큼 재생 */
            /* 프레임레이트는 이전 측정값(FrameRate)을 그대로 사용 */
         }

         /* 파일/메모리 공통: 목표 프레임 간격으로 디스플레이 */
         TotalReplay = 0.0;
         NbFramesReplayed = 0;
         for (n = 0; n < FrameCount; n++)
         {
            /* 경과 타이머 리셋 */
            MappTimer(M_DEFAULT, M_TIMER_RESET, M_NULL);

            if (SaveSequenceToDisk)
            {
               /* 파일에서 프레임 불러와 디스플레이로 */
               MbufImportSequence(SEQUENCE_FILE, M_DEFAULT, M_LOAD,
                                  M_NULL, &MilImageDisp, n, 1, M_READ);
            }
            else
            {
               /* 메모리 버퍼 → 디스플레이 복사 */
               MbufCopy(MilGrabImages[n], MilImageDisp);
            }

            NbFramesReplayed++;
            MosPrintf(MIL_TEXT("Frame #%d             \r"), (int)NbFramesReplayed);

            /* 키 감지(버퍼 꽉 찬 뒤에만 반응) */
            if (MosKbhit() && (n >= (NB_GRAB_IMAGE_MAX - 1)))
            {
               MosGetch();
               break;
            }

            /* 목표 프레임레이트 맞추기 위해 대기 */
            MappTimer(M_DEFAULT, M_TIMER_READ, &TimeWait);
            TotalReplay += TimeWait;
            TimeWait = (1.0 / FrameRate) - TimeWait;
            MappTimer(M_DEFAULT, M_TIMER_WAIT, &TimeWait);
            TotalReplay += (TimeWait > 0) ? TimeWait : 0.0;
         }

         /* 파일 재생 종료 시 클로즈 */
         if (SaveSequenceToDisk)
            MbufImportSequence(SEQUENCE_FILE, M_DEFAULT, M_NULL,
                               M_NULL, M_NULL, M_NULL, M_NULL, M_CLOSE);

         /* 재생 통계 출력 */
         MosPrintf(MIL_TEXT("\n\n%d frames replayed, at a frame rate of %.1f frames/sec ")
                   MIL_TEXT("(%.1f ms/frame).\n\n"),
                   (int)NbFramesReplayed, n / TotalReplay, 1000.0 * TotalReplay / n);
         MosPrintf(MIL_TEXT("Press <Enter> to end (or any other key to playback again).\n"));
         KeyPressed = MosGetch();
      }
      while ((KeyPressed != '\r') && (KeyPressed != '\n'));
   }

   /* 14) 버퍼 해제 */
   MbufFree(MilImageDisp);
   for (n = 0; n < NbFrames; n++)
      MbufFree(MilGrabImages[n]);
   if (MilCompressedImage)
      MbufFree(MilCompressedImage);

   /* 15) 기본 리소스 해제 */
   MappFreeDefault(MilApplication, MilSystem, MilDisplay, MilDigitizer, M_NULL);

   return 0;
}

/* ------------------------------ */
/* 매 프레임 호출되는 레코드 훅   */
/* ------------------------------ */

/* 주석(Annotation) 관련 상수 */
#define STRING_LENGTH_MAX  20
#define STRING_POS_X       20
#define STRING_POS_Y       20

MIL_INT MFTYPE RecordFunction(MIL_INT HookType, MIL_ID HookId, void* HookDataPtr)
{
   HookDataStruct* UserHookDataPtr = (HookDataStruct*)HookDataPtr;
   MIL_ID ModifiedImage = 0;
   MIL_TEXT_CHAR Text[STRING_LENGTH_MAX] = { MIL_TEXT('\0'), };

   /* 1) 방금 취득된 버퍼 ID 얻기 */
   MdigGetHookInfo(HookId, M_MODIFIED_BUFFER + M_BUFFER_ID, &ModifiedImage);

   /* 2) 프레임 카운트 증가 및 진행 상황 표시 */
   UserHookDataPtr->NbGrabbedFrames++;
   MosPrintf(MIL_TEXT("Frame #%d               \r"), (int)UserHookDataPtr->NbGrabbedFrames);

   /* 3) 옵션: 영상에 프레임 번호 텍스트로 주석 */
   if (FRAME_NUMBER_ANNOTATION == M_YES)
   {
      MosSprintf(Text, STRING_LENGTH_MAX, MIL_TEXT(" %d "),
                 (int)UserHookDataPtr->NbGrabbedFrames);
      MgraText(M_DEFAULT, ModifiedImage, STRING_POS_X, STRING_POS_Y, Text);
   }

   /* 4) 새 프레임을 디스플레이 버퍼로 복사 */
   MbufCopy(ModifiedImage, UserHookDataPtr->MilImageDisp);

   /* 5) 필요 시 압축 버퍼로 복사(파일 기록 시 압축 프레임 사용) */
   if (UserHookDataPtr->MilCompressedImage)
      MbufCopy(ModifiedImage, UserHookDataPtr->MilCompressedImage);

   /* 6) 파일 기록 모드면 현재 프레임 기록 */
   if (UserHookDataPtr->SaveSequenceToDisk)
   {
      MbufExportSequence(SEQUENCE_FILE, M_DEFAULT,
                         UserHookDataPtr->MilCompressedImage
                           ? &UserHookDataPtr->MilCompressedImage
                           : &ModifiedImage,
                         1, M_DEFAULT, M_WRITE);
   }

   return 0;
}
