/*****************************************************************************/
/* 
 * 파일명: MappBenchmark.cpp
 *
 * 개요:
 *  - MappTimer를 활용하여 MIL(또는 사용자 정의) 처리 함수의 성능을 정밀 벤치마크하는 템플릿.
 *  - DLL 로드 지연, 매우 짧은 실행 구간에서의 OS 타이머 부정확성을 보정하는 절차(워밍업/반복수 추정) 포함.
 *  - 단일 코어 vs 멀티코어, 하이퍼스레딩(코어 공유) on/off, 하이브리드 CPU의 성능 레벨별 결과를 비교.
 *
 * 핵심 요약:
 *  - MappTimer + 워밍업: 1회 워밍업으로 DLL 로드 지연을 상쇄하고, 최소 실행시간을 바탕으로
 *    최소 2초 이상 측정되도록 반복 횟수를 자동 산정(평균 프레임당 ms, FPS 산출).
 *  - MP 비교: MappControlMp로 멀티프로세싱 on/off 및 코어 공유(하이퍼스레딩) on/off를 제어하여
 *    1코어 대비 멀티코어, HT on/off 성능을 직접 비교.
 *  - 하이브리드 CPU: M_MP_USE_PERFORMANCE_LEVEL 설정으로 성능 레벨 범위를 조절하며 레벨별 성능 측정.
 *  - 치환 용이: ProcessingExecute()의 MimRotate를 임의의 MIL/사용자 처리로 교체하여 동일한 절차로 벤치마크 가능.
 *  - 출력 지표: 평균 프레임당 시간(ms), FPS, 멀티프로세싱 가속 배수(몇 배 빨라졌는지) 제공.
 *
 * 저작권:
 *  © Matrox Electronic Systems Ltd., 1992-2025. All Rights Reserved
 */
#include <mil.h>

/* 대상 MIL 이미지 파일 및 회전 각도 */
#define IMAGE_FILE   M_IMAGE_PATH MIL_TEXT("LargeWafer.mim")
#define ROTATE_ANGLE -15

/* 타이밍 루프 파라미터 (최소 측정 시간과 반복 횟수 추정용) */
#define MINIMUM_BENCHMARK_TIME 2.0   /* 최소 측정 시간(초) – 1.0 이상 권장 */
#define ESTIMATION_NB_LOOP      10   /* 반복 횟수 추정을 위한 사전 측정 횟수 */
#define DEFAULT_NB_LOOP        100   /* 기본 반복 횟수(초기값) */

/* 처리 함수 파라미터 구조체: 입력/출력 버퍼 ID */
typedef struct 
{
   MIL_ID MilSourceImage;        /* 입력 이미지 버퍼 ID */
   MIL_ID MilDestinationImage;   /* 출력 이미지 버퍼 ID */
} PROC_PARAM;

/* 벤치마크 함수: 평균 프레임 시간(ms)과 FPS 산출 */
void Benchmark(PROC_PARAM& ProcParamPtr, MIL_DOUBLE& Time, MIL_DOUBLE& FramesPerSecond);

/* 처리 파이프라인(초기화/실행/해제) – 원하는 연산으로 치환 가능 */
void ProcessingInit(MIL_ID MilSystem, PROC_PARAM& ProcParamPtr);
void ProcessingExecute(PROC_PARAM& ProcParamPtr);
void ProcessingFree(PROC_PARAM& ProcParamPtr);

int MosMain(void)
{
   /* 기본 MIL 자원 ID */
   MIL_ID MilApplication, MilSystem, MilDisplay, MilDisplayImage;
   MIL_ID MilSystemOwnerApplication; /* 시스템 소유 애플리케이션 ID (MP 제어에 필요) */
   MIL_ID MilSystemCurrentThreadId;  /* 현재 스레드 ID (코어 수 조회에 필요) */

   /* 측정 결과 및 보조 변수 */
   PROC_PARAM ProcessingParam;
   MIL_DOUBLE TimeAllCores, TimeAllCoresNoCS, TimeOneCore; /* 평균 프레임 시간(ms) */
   MIL_DOUBLE FPSAllCores, FPSAllCoresNoCS, FPSOneCore;    /* FPS */
   MIL_INT    NbCoresUsed, NbCoresUsedNoCS;                /* 사용 코어 수 */
   MIL_INT    NbPerformanceLevel;                          /* 성능 레벨 수(하이브리드 CPU) */
   MIL_INT    CurrentMaxPerfLevel;

   /* 1) 기본 자원 할당: 애플리케이션/시스템/디스플레이 */
   MappAllocDefault(M_DEFAULT, &MilApplication, &MilSystem,
                                 &MilDisplay, M_NULL, M_NULL);

   /* 2) MP 제어/조회에 필요한 소유 앱/스레드 정보 획득 */
   MsysInquire(MilSystem, M_OWNER_APPLICATION, &MilSystemOwnerApplication);
   MsysInquire(MilSystem, M_CURRENT_THREAD_ID, &MilSystemCurrentThreadId);

   /* 3) 디스플레이용 이미지 복원 및 표시 */
   MbufRestore(IMAGE_FILE, MilSystem, &MilDisplayImage);
   MdispSelect(MilDisplay, MilDisplayImage);

   /* 4) 처리 버퍼 준비(입/출력 할당 및 입력 이미지 로드) */
   ProcessingInit(MilSystem, ProcessingParam);

   /* 안내 및 시작 대기 */
   MosPrintf(MIL_TEXT("\nPROCESSING FUNCTION BENCHMARKING:\n"));
   MosPrintf(MIL_TEXT("---------------------------------\n\n"));
   MosPrintf(MIL_TEXT("This program times a processing function under different conditions.\n"));
   MosPrintf(MIL_TEXT("Press any key to start.\n\n"));
   MosGetch();
   MosPrintf(MIL_TEXT("PROCESSING TIME FOR %lldx%lld:\n" ),
             (long long)MbufInquire(ProcessingParam.MilDestinationImage, M_SIZE_X, M_NULL),
             (long long)MbufInquire(ProcessingParam.MilDestinationImage, M_SIZE_Y, M_NULL));
   MosPrintf(MIL_TEXT("------------------------------\n\n"));

   /* 5) [단일 코어] 멀티프로세싱 비활성화 후 벤치마크 */
   MappControlMp(MilSystemOwnerApplication, M_MP_USE, M_DEFAULT, M_DISABLE, M_NULL);
   Benchmark(ProcessingParam, TimeOneCore, FPSOneCore);

   /* 결과 반영 및 출력 */
   MbufCopy(ProcessingParam.MilDestinationImage, MilDisplayImage);
   MosPrintf(MIL_TEXT("Without multi-processing (  1 CPU core ): %5.3f ms (%6.1f fps)\n\n"),
             TimeOneCore, FPSOneCore);
   MappControlMp(MilSystemOwnerApplication, M_MP_USE, M_DEFAULT, M_DEFAULT, M_NULL);

   /* 6) [멀티 코어] 멀티프로세싱 활성 + 성능 레벨 전체 사용 허용 */
   MappControlMp(MilSystemOwnerApplication, M_MP_USE, M_DEFAULT, M_ENABLE,  M_NULL);
   MappControlMp(MilSystemOwnerApplication, M_MP_USE_PERFORMANCE_LEVEL, M_ALL, M_ENABLE, M_NULL);

   /* 사용 가능한 성능 레벨 수(하이브리드 아닌 경우 1) */
   MappInquireMp(MilSystemOwnerApplication, M_MP_NB_PERFORMANCE_LEVEL,
                 M_DEFAULT, M_DEFAULT, &NbPerformanceLevel);

   /* 7) 최대 성능 레벨에서 1까지 낮춰가며 측정 반복 */
   for (CurrentMaxPerfLevel = NbPerformanceLevel; CurrentMaxPerfLevel > 0; CurrentMaxPerfLevel--)
   {
      if (CurrentMaxPerfLevel > 1)
         MosPrintf(MIL_TEXT("Benchmark result with core performance level 1 to %d.\n\n"),
                   CurrentMaxPerfLevel);
      else
         MosPrintf(MIL_TEXT("Benchmark result with core performance level 1.\n\n"),
                   CurrentMaxPerfLevel);

      /* (a) 하이퍼스레딩 허용: 코어 공유 ENABLE */
      MappControlMp(MilSystemOwnerApplication, M_CORE_SHARING, M_DEFAULT, M_ENABLE, M_NULL);
      MthrInquireMp(MilSystemCurrentThreadId, M_CORE_NUM_EFFECTIVE, M_DEFAULT, M_DEFAULT, &NbCoresUsed);
      if (NbCoresUsed > 1)
      {
         Benchmark(ProcessingParam, TimeAllCores, FPSAllCores);
         MbufCopy(ProcessingParam.MilDestinationImage, MilDisplayImage);
         MosPrintf(MIL_TEXT("Using multi-processing   (%3d CPU cores): %5.3f ms (%6.1f fps)\n"),
                   (int)NbCoresUsed, TimeAllCores, FPSAllCores);
      }
      /* MP 설정 원복 */
      MappControlMp(MilSystemOwnerApplication, M_MP_USE, M_DEFAULT, M_DEFAULT, M_NULL);
      MappControlMp(MilSystemOwnerApplication, M_CORE_SHARING, M_DEFAULT, M_DEFAULT, M_NULL);

      /* (b) 하이퍼스레딩 비활성: 코어 공유 DISABLE */
      MappControlMp(MilSystemOwnerApplication, M_MP_USE, M_DEFAULT, M_ENABLE,  M_NULL);
      MappControlMp(MilSystemOwnerApplication, M_CORE_SHARING, M_DEFAULT, M_DISABLE, M_NULL);
      MthrInquireMp(MilSystemCurrentThreadId, M_CORE_NUM_EFFECTIVE, M_DEFAULT, M_DEFAULT, &NbCoresUsedNoCS);
      if (NbCoresUsedNoCS != NbCoresUsed)
      {
         Benchmark(ProcessingParam, TimeAllCoresNoCS, FPSAllCoresNoCS);
         MbufCopy(ProcessingParam.MilDestinationImage, MilDisplayImage);
         MosPrintf(MIL_TEXT("Using multi-processing   (%3d CPU cores): %5.3f ms (%6.1f fps), no Hyper-Thread.\n"),
                   (int)NbCoresUsedNoCS, TimeAllCoresNoCS, FPSAllCoresNoCS);
      }
      /* MP 설정 원복 */
      MappControlMp(MilSystemOwnerApplication, M_MP_USE, M_DEFAULT, M_DEFAULT, M_NULL);
      MappControlMp(MilSystemOwnerApplication, M_CORE_SHARING, M_DEFAULT, M_DEFAULT, M_NULL);

      /* (c) 현재 최대 성능 레벨을 하나 비활성(상한을 낮춰 다음 루프) */
      MappControlMp(MilSystemOwnerApplication, M_MP_USE_PERFORMANCE_LEVEL,
                    CurrentMaxPerfLevel, M_DISABLE, M_NULL);

      /* (d) 요약: 1코어 대비 가속 배수 출력 */
      if (NbCoresUsed > 1)
      {
         MosPrintf(MIL_TEXT("Benchmark is %.1f times faster with multi-processing.\n"),
                   TimeOneCore / TimeAllCores);
      }
      if (NbCoresUsedNoCS != NbCoresUsed)
      {
         MosPrintf(MIL_TEXT("Benchmark is %.1f times faster with multi-processing and no Hyper-Thread.\n\n"),
                   TimeOneCore / TimeAllCoresNoCS);
      }
   }

   /* 종료 대기 */
   MosPrintf(MIL_TEXT("Press any key to end.\n"));
   MosGetch();

   /* 자원 해제 */
   ProcessingFree(ProcessingParam);
   MdispSelect(MilDisplay, M_NULL);
   MbufFree(MilDisplayImage);
   MappFreeDefault(MilApplication, MilSystem, MilDisplay, M_NULL, M_NULL);
   return 0;
}

/*****************************************************************************
 * 벤치마크 함수
 *  - DLL 로드/캐시 등 초기 지연 보정: 1회 워밍업
 *  - 최소 실행시간 추정 후, MINIMUM_BENCHMARK_TIME(기본 2초) 이상이 되도록 반복수 계산
 *  - 평균 프레임 시간(ms), FPS 산출
 *****************************************************************************/
void Benchmark(PROC_PARAM& ProcParamPtr, MIL_DOUBLE& Time, MIL_DOUBLE& FramesPerSecond)
{
   MIL_INT    EstimatedNbLoop = DEFAULT_NB_LOOP;
   MIL_DOUBLE StartTime, EndTime;
   MIL_DOUBLE MinTime = 0.0;
   MIL_INT    n;

   /* 스레드 내 모든 호출 완료 대기(간섭 제거) */
   MthrWait(M_DEFAULT, M_THREAD_WAIT, M_NULL);

   /* 1) 워밍업(1회) – DLL 로드 및 초기 오버헤드 제거 */
   MappTimer(M_DEFAULT, M_TIMER_READ, &StartTime);
   ProcessingExecute(ProcParamPtr);
   MthrWait(M_DEFAULT, M_THREAD_WAIT, M_NULL);
   MappTimer(M_DEFAULT, M_TIMER_READ, &EndTime);
   MinTime = EndTime - StartTime;

   /* 2) 사전 측정으로 최소 실행시간(MinTime) 갱신 */
   for (n = 0; n < ESTIMATION_NB_LOOP; n++)
   {
      MappTimer(M_DEFAULT, M_TIMER_READ, &StartTime);
      ProcessingExecute(ProcParamPtr);
      MthrWait(M_DEFAULT, M_THREAD_WAIT, M_NULL);
      MappTimer(M_DEFAULT, M_TIMER_READ, &EndTime);

      Time    = EndTime - StartTime;
      MinTime = (Time < MinTime) ? Time : MinTime;
   }

   /* 3) 최소 2초 이상이 되도록 반복수 산정 */
   if (MinTime > 0.0)
      EstimatedNbLoop = (MIL_INT)(MINIMUM_BENCHMARK_TIME / MinTime) + 1;

   /* 4) 본 벤치마크 실행 */
   MappTimer(M_DEFAULT, M_TIMER_READ, &StartTime);
   for (n = 0; n < EstimatedNbLoop; n++)
      ProcessingExecute(ProcParamPtr);
   MthrWait(M_DEFAULT, M_THREAD_WAIT, M_NULL);
   MappTimer(M_DEFAULT, M_TIMER_READ, &EndTime);

   /* 5) 평균 프레임 시간(ms) 및 FPS 계산 */
   Time = EndTime - StartTime;             /* 총 소요시간(초) */
   FramesPerSecond = EstimatedNbLoop / Time;
   Time = (Time * 1000.0) / EstimatedNbLoop; /* 프레임당 시간(ms) */
}

/*****************************************************************************
 * 처리 초기화
 *  - 입력/출력 컬러 버퍼를 소스 이미지와 동일 스펙으로 할당
 *  - 입력 버퍼에 이미지 로드
 *****************************************************************************/
void ProcessingInit(MIL_ID MilSystem, PROC_PARAM& ProcParamPtr)
{
   /* 입력 이미지 버퍼 할당 */
   MbufAllocColor(MilSystem, 
      MbufDiskInquire(IMAGE_FILE, M_SIZE_BAND, M_NULL),
      MbufDiskInquire(IMAGE_FILE, M_SIZE_X,    M_NULL),
      MbufDiskInquire(IMAGE_FILE, M_SIZE_Y,    M_NULL),
      MbufDiskInquire(IMAGE_FILE, M_SIZE_BIT,  M_NULL) + M_UNSIGNED,
      M_IMAGE + M_PROC, &ProcParamPtr.MilSourceImage);

   /* 입력 이미지 로드 */
   MbufLoad(IMAGE_FILE, ProcParamPtr.MilSourceImage);

   /* 출력 이미지 버퍼 할당 */
   MbufAllocColor(MilSystem, 
      MbufDiskInquire(IMAGE_FILE, M_SIZE_BAND, M_NULL),
      MbufDiskInquire(IMAGE_FILE, M_SIZE_X,    M_NULL),
      MbufDiskInquire(IMAGE_FILE, M_SIZE_Y,    M_NULL),
      MbufDiskInquire(IMAGE_FILE, M_SIZE_BIT,  M_NULL) + M_UNSIGNED,
      M_IMAGE + M_PROC, &ProcParamPtr.MilDestinationImage);
}

/*****************************************************************************
 * 처리 실행 (치환 포인트)
 *  - 현재는 예시로 회전(MimRotate) 사용
 *  - 원하는 MIL/사용자 처리로 교체하여 동일 템플릿으로 벤치마크 가능
 *****************************************************************************/
void ProcessingExecute(PROC_PARAM& ProcParamPtr)
{
   MimRotate(ProcParamPtr.MilSourceImage, ProcParamPtr.MilDestinationImage, ROTATE_ANGLE,
             M_DEFAULT, M_DEFAULT, M_DEFAULT, M_DEFAULT,
             M_BILINEAR + M_OVERSCAN_CLEAR);
}

/*****************************************************************************
 * 처리 해제
 *  - 입력/출력 버퍼 해제
 *****************************************************************************/
void ProcessingFree(PROC_PARAM& ProcParamPtr)
{
   MbufFree(ProcParamPtr.MilSourceImage);
   MbufFree(ProcParamPtr.MilDestinationImage);
}
