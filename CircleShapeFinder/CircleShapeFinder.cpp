/*************************************************************************************/
/*
 * 파일명: CircleShapeFinder.cpp
 *
 * 개요:
 *   - MIL Geometric Model Finder(Mmod)에서 M_SHAPE_CIRCLE을 사용하여
 *     원형 모델을 정의하고 다양한 장면(간단/복잡/보정/소형 원)에서 서클을 탐색하는 예제.
 *
 * 핵심 요약:
 *   - 파이프라인: MmodAlloc(M_SHAPE_CIRCLE) → MmodDefine(M_CIRCLE, radius)
 *                 → MmodPreprocess → MmodFind → MmodGetResult → MmodDraw.
 *   - 간단 예제: 고정 반지름, 다중 발생 탐색.
 *   - 복잡 예제1: 스케일 범위 확대(M_SCALE_MIN_FACTOR), 엣지 추출 정밀도/스무딩 조정.
 *   - 복잡 예제2: 보정(McalAssociate) 적용, 수용도/최소 분리/극성 제약으로 오검출 억제.
 *   - 소형 원: M_RESOLUTION_COARSENESS_LEVEL 낮춰 작은 원 검출 민감도 향상.
 *   - 시각화: 그래픽 리스트를 디스플레이에 연결(M_ASSOCIATED_GRAPHIC_LIST_ID) 후 MmodDraw.
 *   - 성능: MappTimer(M_SYNCHRONOUS)로 탐색 시간(ms) 로깅.
 *
 * 저작권:
 *   © Matrox Electronic Systems Ltd., 1992-2025. All Rights Reserved
 */
#include <mil.h>

//***************************************************************************
// 예제 소개 출력
//***************************************************************************
void PrintHeader()
{
   MosPrintf(MIL_TEXT("[EXAMPLE NAME]\n"));
   MosPrintf(MIL_TEXT("CircleShapeFinder\n\n"));
   MosPrintf(MIL_TEXT("[SYNOPSIS]\n"));
   MosPrintf(MIL_TEXT("This example uses model finder to define circle models and search for circles\n"));
   MosPrintf(MIL_TEXT("in target images. A simple circle finder example is presented first (multiple\n"));
   MosPrintf(MIL_TEXT("occurrences and a small radius range with good search conditions), followed by\n"));
   MosPrintf(MIL_TEXT("more complex examples (multiple occurrences and a large radius range in a\n"));
   MosPrintf(MIL_TEXT("complex scene with bad search conditions) and an example of how to use\n"));
   MosPrintf(MIL_TEXT("M_RESOLUTION_COARSENESS_LEVEL to find very small circles.\n\n"));

   MosPrintf(MIL_TEXT("[MODULES USED]\n"));
   MosPrintf(MIL_TEXT("Modules used: application, system, display,\n")); 
   MosPrintf(MIL_TEXT("calibration, geometric model finder.\n\n"));

   MosPrintf(MIL_TEXT("Press any key to continue.\n\n"));
   MosGetch();
}

/* 예제 함수 선언 */
void SimpleCircleSearchExample(MIL_ID MilSystem, MIL_ID MilDisplay);
void ComplexCircleSearchExample1(MIL_ID MilSystem, MIL_ID MilDisplay);
void ComplexCircleSearchExample2(MIL_ID MilSystem, MIL_ID MilDisplay);
void SmallCircleSearchExample(MIL_ID MilSystem, MIL_ID MilDisplay);

/*****************************************************************************/
/* 메인: 시스템/디스플레이 할당 → 예제 4개 실행 → 해제 */
/*****************************************************************************/
int MosMain(void)
{
   MIL_ID MilApplication; /* 애플리케이션 ID */
   MIL_ID MilSystem;      /* 시스템 ID */
   MIL_ID MilDisplay;     /* 디스플레이 ID */

   /* 기본 오브젝트 할당(호스트 시스템 + 윈도우 디스플레이) */
   MappAlloc(M_NULL, M_DEFAULT, &MilApplication);
   MsysAlloc(MilApplication, M_SYSTEM_HOST, M_DEFAULT, M_DEFAULT, &MilSystem);
   MdispAlloc(MilSystem, M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay);

   /* 소개 출력 */
   //PrintHeader();

   /* 예제 실행 */
   //SimpleCircleSearchExample(MilSystem, MilDisplay);
   ComplexCircleSearchExample1(MilSystem, MilDisplay);
   //ComplexCircleSearchExample2(MilSystem, MilDisplay);
   //SmallCircleSearchExample(MilSystem, MilDisplay);
   
   /* 해제 */
   MdispFree(MilDisplay);
   MappFreeDefault(MilApplication, MilSystem, M_NULL, M_NULL, M_NULL);
   return 0;
}

/******************************************************************************/
/* [간단 예제] 고정 반지름 원 다중 검출                                       */
/******************************************************************************/
/* 타깃 기본 이미지 */
#define SIMPLE_CIRCLE_SEARCH_TARGET_IMAGE   M_IMAGE_PATH MIL_TEXT("/CircleShapeFinder/SimpleCircleSearchTarget.mim")

/* 타깃 테스트 이미지 */
#define TEST_IMG   M_IMAGE_PATH MIL_TEXT("/CircleShapeFinder/ttt.bmp")

/* 모델 개수/반지름/최대 발생수 */
#define NUMBER_OF_MODELS            30L //18L
#define MODEL_RADIUS                100.0 
#define MODEL_MAX_OCCURRENCES       50L // 30L

void SimpleCircleSearchExample(MIL_ID MilSystem, MIL_ID MilDisplay)
{
   /* 버퍼 ID / 그래픽 리스트 ID */
   MIL_ID MilImage, GraphicList;
   /* 컨텍스트 ID/ 결과 ID */
   MIL_ID MilSearchContext, MilResult;

   /* 표시 색상 및 결과 저장 변수 */
   MIL_DOUBLE PositionDrawColor = M_COLOR_RED;
   MIL_DOUBLE ModelDrawColor    = M_COLOR_GREEN;
   MIL_DOUBLE BoxDrawColor      = M_COLOR_BLUE;
   MIL_INT    NumResults = 0L;
   MIL_DOUBLE Score[MODEL_MAX_OCCURRENCES], XPosition[MODEL_MAX_OCCURRENCES],
              YPosition[MODEL_MAX_OCCURRENCES], Radius[MODEL_MAX_OCCURRENCES],
              Time = 0.0;
   int i;

   /* 타깃 이미지 로드 & 표시 */
   MbufRestore(TEST_IMG, MilSystem, &MilImage);
   MdispSelect(MilDisplay, MilImage);

   /* 그래픽 리스트 생성 & 디스플레이에 연결(오버레이용) */
   MgraAllocList(MilSystem, M_DEFAULT, &GraphicList);
   MdispControl(MilDisplay, M_ASSOCIATED_GRAPHIC_LIST_ID, GraphicList);

   /* 서클 파인더 컨텍스트/결과 할당 */
   MmodAlloc(MilSystem, M_SHAPE_CIRCLE, M_DEFAULT, &MilSearchContext);
   MmodAllocResult(MilSystem, M_SHAPE_CIRCLE, &MilResult);

   /* 원 모델 정의(반지름 고정) + 발생 개수 설정 */
   MmodDefine(MilSearchContext, M_CIRCLE, M_DEFAULT, MODEL_RADIUS, M_DEFAULT, M_DEFAULT, M_DEFAULT);
   MmodControl(MilSearchContext, 0, M_NUMBER, NUMBER_OF_MODELS);

   /* 사전처리 */
   MmodPreprocess(MilSearchContext, M_DEFAULT);

   /* 타이머 리셋 후 탐색 */
   MappTimer(M_DEFAULT, M_TIMER_RESET + M_SYNCHRONOUS, M_NULL);
   MmodFind(MilSearchContext, MilImage, MilResult);
   MappTimer(M_DEFAULT, M_TIMER_READ + M_SYNCHRONOUS, &Time);

   /* 결과 수 */
   MmodGetResult(MilResult, M_DEFAULT, M_NUMBER + M_TYPE_MIL_INT, &NumResults);

   /* 콘솔에 텍스트 출력 */
   MosPrintf(MIL_TEXT("\nUsing model finder M_SHAPE_CIRCLE in a simple situation:\n"));
   MosPrintf(MIL_TEXT("--------------------------------------------------------\n\n"));
   MosPrintf(MIL_TEXT("A circle model was defined with a nominal radius of %-3.1f%.\n\n"), MODEL_RADIUS);

   if ((NumResults >= 1) && (NumResults <= MODEL_MAX_OCCURRENCES))
   {
      /* 결과 획득 */
      MmodGetResult(MilResult, M_DEFAULT, M_POSITION_X, XPosition);
      MmodGetResult(MilResult, M_DEFAULT, M_POSITION_Y, YPosition);
      MmodGetResult(MilResult, M_DEFAULT, M_RADIUS,     Radius);
      MmodGetResult(MilResult, M_DEFAULT, M_SCORE,      Score);

      MosPrintf(MIL_TEXT("Result   X-Position   Y-Position   Radius   Score\n\n"));
      for (i = 0; i < NumResults; i++)
      {
         MosPrintf(MIL_TEXT("%-9d%-13.2f%-13.2f%-8.2f%-5.2f%%\n"),i, XPosition[i], YPosition[i], Radius[i], Score[i]);
      }
      for (i = 0; i < NumResults; i++)
      {
         MosPrintf(MIL_TEXT("%f\n"), Score[i]);
      }
      
      MosPrintf(MIL_TEXT("\nThe search time was %.1f ms.\n\n"), Time * 1000.0);
   
      /* 오버레이 그리기 */
      
      //중심 위치 
      MgraControl(M_DEFAULT, M_COLOR, PositionDrawColor);
      MmodDraw(M_DEFAULT, MilResult, GraphicList, M_DRAW_POSITION, M_DEFAULT, M_DEFAULT);

      // 박스 
      MgraControl(M_DEFAULT, M_COLOR, BoxDrawColor);
      MmodDraw(M_DEFAULT, MilResult, GraphicList, M_DRAW_BOX, M_DEFAULT, M_DEFAULT);

      // 엣지
      MgraControl(M_DEFAULT, M_COLOR, ModelDrawColor);
      MmodDraw(M_DEFAULT, MilResult, GraphicList, M_DRAW_EDGES, M_DEFAULT, M_DEFAULT);
   }
   else
   {
      MosPrintf(MIL_TEXT("The model was not found or too many occurrences!\n\n"));
   }

   MosPrintf(MIL_TEXT("Press any key to continue.\n\n"));
   MosGetch();

   /* 해제 */
   MgraFree(GraphicList);
   MbufFree(MilImage);
   MmodFree(MilSearchContext);
   MmodFree(MilResult);
}

/******************************************************************************/
/* [복잡 예제 1] 스케일 변화 큼, 저대비/노이즈 등 복잡 장면                   */
/******************************************************************************/

#define COMPLEX_CIRCLE_SEARCH_TARGET_IMAGE_1   M_IMAGE_PATH MIL_TEXT("/CircleShapeFinder/ComplexCircleSearchTarget1.mim")
/* 타깃 테스트 이미지 */
#define TEST_IMG   M_IMAGE_PATH MIL_TEXT("/CircleShapeFinder/ttt.bmp")
#define NUMBER_OF_MODELS_1        10L
#define MODEL_RADIUS_1            300.0
#define SMOOTHNESS_VALUE_1        75.0
#define MIN_SCALE_FACTOR_VALUE_1  0.1

void ComplexCircleSearchExample1(MIL_ID MilSystem, MIL_ID MilDisplay)
{
   // 전체 결과용 디스플레이1
   MIL_ID MilImage, GraphicList;
   MIL_ID MilSearchContext, MilResult;

   // score 값 90 넘는 애들을 위한 디스플레이 2, 그래픽 리스트2
   MIL_ID MilDisplay2;
   MIL_ID GraphicList2;

   MIL_DOUBLE PositionDrawColor = M_COLOR_RED;
   MIL_DOUBLE ModelDrawColor    = M_COLOR_GREEN;
   MIL_DOUBLE BoxDrawColor = M_COLOR_BLUE;
   MIL_INT    NumResults = 0L;
   MIL_DOUBLE Score[MODEL_MAX_OCCURRENCES], XPosition[MODEL_MAX_OCCURRENCES],
              YPosition[MODEL_MAX_OCCURRENCES], Radius[MODEL_MAX_OCCURRENCES],
              Time = 0.0;
   int i;

   /* 타깃 표시 */
   MbufRestore(TEST_IMG, MilSystem, &MilImage); // TEST_IMG 내가 사용하려고 하는 테스트 이미지
   MdispSelect(MilDisplay, MilImage);

   MdispAlloc(MilSystem, M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDisplay2);
   MdispSelect(MilDisplay2, MilImage);
   MdispControl(MilDisplay2, M_TITLE, MIL_TEXT("Display 2 - Score >= 90% only"));

   /* 그래픽 리스트 연결 */
   MgraAllocList(MilSystem, M_DEFAULT, &GraphicList);
   MdispControl(MilDisplay, M_ASSOCIATED_GRAPHIC_LIST_ID, GraphicList);

   /* 그래픽 리스트 연결2 */
   MgraAllocList(MilSystem, M_DEFAULT, &GraphicList2);
   MdispControl(MilDisplay2, M_ASSOCIATED_GRAPHIC_LIST_ID,GraphicList2);

   /* 컨텍스트/결과 할당 */
   MmodAlloc(MilSystem, M_SHAPE_CIRCLE, M_DEFAULT, &MilSearchContext);
   MmodAllocResult(MilSystem, M_SHAPE_CIRCLE, &MilResult);

   /* 모델 정의 */
   MmodDefine(MilSearchContext, M_CIRCLE, M_DEFAULT, MODEL_RADIUS_1, M_DEFAULT, M_DEFAULT, M_DEFAULT);

   /* 엣지 추출 튜닝 + 스케일 최소 인자 ↓ (큰 스케일 범위 허용) */
   MmodControl(MilSearchContext, M_CONTEXT, M_DETAIL_LEVEL, M_VERY_HIGH);
   MmodControl(MilSearchContext, M_CONTEXT, M_SMOOTHNESS,  SMOOTHNESS_VALUE_1);
   MmodControl(MilSearchContext, 0,         M_SCALE_MIN_FACTOR, MIN_SCALE_FACTOR_VALUE_1);

   /* 발생 개수 설정 */
   MmodControl(MilSearchContext, M_DEFAULT, M_NUMBER, NUMBER_OF_MODELS_1);

   /* 사전처리 & 탐색 */
   MmodPreprocess(MilSearchContext, M_DEFAULT);
   MappTimer(M_DEFAULT, M_TIMER_RESET + M_SYNCHRONOUS, M_NULL);
   MmodFind(MilSearchContext, MilImage, MilResult);
   MappTimer(M_DEFAULT, M_TIMER_READ + M_SYNCHRONOUS, &Time);

   /* 결과 */
   MmodGetResult(MilResult, M_DEFAULT, M_NUMBER + M_TYPE_MIL_INT, &NumResults);

   MosPrintf(MIL_TEXT("\nUsing model finder M_SHAPE_CIRCLE in a complex situation:\n"));
   MosPrintf(MIL_TEXT("---------------------------------------------------------\n\n"));
   MosPrintf(MIL_TEXT("A circle model was defined with a nominal radius of %-3.1f%.\n\n"), MODEL_RADIUS_1);

   if ((NumResults >= 1) && (NumResults <= MODEL_MAX_OCCURRENCES))
   {
      MmodGetResult(MilResult, M_DEFAULT, M_POSITION_X, XPosition);
      MmodGetResult(MilResult, M_DEFAULT, M_POSITION_Y, YPosition);
      MmodGetResult(MilResult, M_DEFAULT, M_RADIUS,     Radius);
      MmodGetResult(MilResult, M_DEFAULT, M_SCORE,      Score);

      MosPrintf(MIL_TEXT("The circles were found despite: High scale range / Low contrast / Noisy edges\n\n"));
      
      /* ========================
         디스플레이 1: 전체 결과
      =========================== */

      MosPrintf(MIL_TEXT("[Display 1] All results\n"));
      MosPrintf(MIL_TEXT("Result   X-Position   Y-Position   Radius   Score\n\n"));

      MgraClear(M_DEFAULT, GraphicList);

      // 위치
      MgraControl(M_DEFAULT, M_COLOR, PositionDrawColor);
      MmodDraw(M_DEFAULT, MilResult, GraphicList, M_DRAW_POSITION, M_DEFAULT, M_DEFAULT);

      // 박스 
      MgraControl(M_DEFAULT, M_COLOR, BoxDrawColor);
      MmodDraw(M_DEFAULT, MilResult, GraphicList, M_DRAW_BOX, M_DEFAULT, M_DEFAULT);

      // 엣지
      MgraControl(M_DEFAULT, M_COLOR, ModelDrawColor);
      MmodDraw(M_DEFAULT, MilResult, GraphicList, M_DRAW_EDGES, M_DEFAULT, M_DEFAULT);


      for (i = 0; i < NumResults; i++)
      {
         MosPrintf(MIL_TEXT("%-9d%-13.2f%-13.2f%-8.2f%-5.2f%%\n"), i, XPosition[i], YPosition[i], Radius[i], Score[i]);
      }
      MosPrintf(MIL_TEXT("\n"));


      /* =========================================
         디스플레이 2: Score ≥ 90.0 만 출력/표시
      ============================================ */
      MosPrintf(MIL_TEXT("[Display 2] Filtered results\n"));
      MosPrintf(MIL_TEXT("Result   X-Position   Y-Position   Radius   Score\n\n"));

      MgraClear(M_DEFAULT, GraphicList2);


      // 내가 수정하려 하는 코드
      for (i = 0; i < NumResults; i++)
      {
         if (Score[i] > 90.0L) {
            MosPrintf(MIL_TEXT("%-9d%-13.2f%-13.2f%-8.2f%-5.2f%%\n"), i, XPosition[i], YPosition[i], Radius[i], Score[i]);

            // 위치
            MgraControl(M_DEFAULT, M_COLOR, PositionDrawColor);
            MmodDraw(M_DEFAULT, MilResult, GraphicList2, M_DRAW_POSITION, i, M_DEFAULT);

            // 박스 
            MgraControl(M_DEFAULT, M_COLOR, BoxDrawColor);
            MmodDraw(M_DEFAULT, MilResult, GraphicList2, M_DRAW_BOX, i, M_DEFAULT);

            // 엣지
            MgraControl(M_DEFAULT, M_COLOR, ModelDrawColor);
            MmodDraw(M_DEFAULT, MilResult, GraphicList2, M_DRAW_EDGES, i, M_DEFAULT);
         }
         
      }
   }
   else
   {
      MosPrintf(MIL_TEXT("The circles were not found or too many occurrences!\n\n"));
   }

   MosPrintf(MIL_TEXT("Press any key to continue.\n\n"));
   MosGetch();

   /* 해제 */
   
   /*
   MgraFree(GraphicList);
   MbufFree(MilImage);
   MmodFree(MilSearchContext);
   MmodFree(MilResult);
   */


   // 1) 디스플레이에서 이미지 선택 해제
   MdispSelect(MilDisplay, M_NULL);
   if (MilDisplay2) MdispSelect(MilDisplay2, M_NULL);

   // 2) 디스플레이에 붙인 그래픽 리스트 분리
   MdispControl(MilDisplay, M_ASSOCIATED_GRAPHIC_LIST_ID, M_NULL);
   if (MilDisplay2) MdispControl(MilDisplay2, M_ASSOCIATED_GRAPHIC_LIST_ID, M_NULL);

   // 3) 그래픽 리스트 free
   if (GraphicList)  MgraFree(GraphicList);
   if (GraphicList2) MgraFree(GraphicList2);

   // 4) 디스플레이 free
   //  - MilDisplay: 호출자에서 만들었으면 여기서 free 하지 말고 분리만 합니다.
   //    (지금 함수 인자로 받아왔으므로, 보통은 호출자가 free)
   //  - MilDisplay2: 이 함수에서 alloc 했으니 여기서 free
   if (MilDisplay2) MdispFree(MilDisplay2);

   // 5) 결과/컨텍스트 free
   if (MilResult)         MmodFree(MilResult);
   if (MilSearchContext)  MmodFree(MilSearchContext);

   // 6) 버퍼 free (디스플레이에서 이미 deselect 된 상태여야 함)
   if (MilImage) MbufFree(MilImage);
}

/******************************************************************************/
/* [복잡 예제 2] 보정 적용 + 제약(수용도/최소 분리/극성)으로 견고성 향상      */
/******************************************************************************/
#define COMPLEX_CIRCLE_SEARCH_TARGET_IMAGE_2   M_IMAGE_PATH MIL_TEXT("/CircleShapeFinder/ComplexCircleSearchTarget2.mim")
#define COMPLEX_CIRCLE_SEARCH_CALIBRATION_2    M_IMAGE_PATH MIL_TEXT("/CircleShapeFinder/ComplexCircleSearchCalibration2.mca")
#define NUMBER_OF_MODELS_2            23L
#define MODEL_RADIUS_2                1.0
#define SMOOTHNESS_VALUE_2            65.0
#define ACCEPTANCE_VALUE_2            50.0
#define MIN_SEPARATION_SCALE_VALUE_2  1.5
#define MIN_SEPARATION_XY_VALUE_2     30.0

void ComplexCircleSearchExample2(MIL_ID MilSystem, MIL_ID MilDisplay)
{
   MIL_ID MilImage, MilCalibration, GraphicList;
   MIL_ID MilSearchContext, MilResult;

   MIL_DOUBLE PositionDrawColor = M_COLOR_RED;
   MIL_DOUBLE ModelDrawColor    = M_COLOR_GREEN;
   MIL_INT    NumResults = 0L;
   MIL_DOUBLE Score[MODEL_MAX_OCCURRENCES], XPosition[MODEL_MAX_OCCURRENCES],
              YPosition[MODEL_MAX_OCCURRENCES], Radius[MODEL_MAX_OCCURRENCES],
              Time = 0.0;
   int i;

   /* 타깃/보정 로드 및 연계 → 표시 */
   MbufRestore(COMPLEX_CIRCLE_SEARCH_TARGET_IMAGE_2, MilSystem, &MilImage);
   McalRestore(COMPLEX_CIRCLE_SEARCH_CALIBRATION_2, MilSystem, M_DEFAULT, &MilCalibration);
   McalAssociate(MilCalibration, MilImage, M_DEFAULT);
   MdispSelect(MilDisplay, MilImage);

   /* 그래픽 리스트 연결 */
   MgraAllocList(MilSystem, M_DEFAULT, &GraphicList);
   MdispControl(MilDisplay, M_ASSOCIATED_GRAPHIC_LIST_ID, GraphicList);

   /* 컨텍스트/결과 할당 및 모델 정의 */
   MmodAlloc(MilSystem, M_SHAPE_CIRCLE, M_DEFAULT, &MilSearchContext);
   MmodAllocResult(MilSystem, M_SHAPE_CIRCLE, &MilResult);
   MmodDefine(MilSearchContext, M_CIRCLE, M_DEFAULT, MODEL_RADIUS_2, M_DEFAULT, M_DEFAULT, M_DEFAULT);

   /* 엣지 추출/스무딩, 수용도, 최소 분리, 극성(반전) 설정 */
   MmodControl(MilSearchContext, M_CONTEXT, M_DETAIL_LEVEL, M_VERY_HIGH);
   MmodControl(MilSearchContext, M_CONTEXT, M_SMOOTHNESS,  SMOOTHNESS_VALUE_2);
   MmodControl(MilSearchContext, M_DEFAULT, M_ACCEPTANCE,  ACCEPTANCE_VALUE_2);
   MmodControl(MilSearchContext, 0,         M_MIN_SEPARATION_SCALE, MIN_SEPARATION_SCALE_VALUE_2);
   MmodControl(MilSearchContext, 0,         M_MIN_SEPARATION_X,     MIN_SEPARATION_XY_VALUE_2);
   MmodControl(MilSearchContext, 0,         M_MIN_SEPARATION_Y,     MIN_SEPARATION_XY_VALUE_2);
   MmodControl(MilSearchContext, 0,         M_POLARITY,             M_REVERSE);

   /* 발생 개수 설정 */
   MmodControl(MilSearchContext, M_DEFAULT, M_NUMBER, NUMBER_OF_MODELS_2);

   /* 사전처리/탐색/시간 */
   MmodPreprocess(MilSearchContext, M_DEFAULT);
   MappTimer(M_DEFAULT, M_TIMER_RESET + M_SYNCHRONOUS, M_NULL);
   MmodFind(MilSearchContext, MilImage, MilResult);
   MappTimer(M_DEFAULT, M_TIMER_READ + M_SYNCHRONOUS, &Time);

   /* 결과 수 */
   MmodGetResult(MilResult, M_DEFAULT, M_NUMBER + M_TYPE_MIL_INT, &NumResults);

   MosPrintf(MIL_TEXT("\nUsing model finder M_SHAPE_CIRCLE with a calibrated target:\n"));
   MosPrintf(MIL_TEXT("-----------------------------------------------------------\n\n"));
   MosPrintf(MIL_TEXT("A circle model was defined with a nominal radius of %-3.1f%.\n\n"), MODEL_RADIUS_2);

   if ((NumResults >= 1) && (NumResults <= MODEL_MAX_OCCURRENCES))
   {
      MmodGetResult(MilResult, M_DEFAULT, M_POSITION_X, XPosition);
      MmodGetResult(MilResult, M_DEFAULT, M_POSITION_Y, YPosition);
      MmodGetResult(MilResult, M_DEFAULT, M_RADIUS,     Radius);
      MmodGetResult(MilResult, M_DEFAULT, M_SCORE,      Score);

      MosPrintf(MIL_TEXT("Found despite: Occlusion / Low contrast / Noisy edges\n\n"));
      MosPrintf(MIL_TEXT("Result   X-Position   Y-Position   Radius   Score\n\n"));
      for (i = 0; i < NumResults; i++)
      {
         MosPrintf(MIL_TEXT("%-9d%-13.2f%-13.2f%-8.2f%-5.2f%%\n"),
                   i, XPosition[i], YPosition[i], Radius[i], Score[i]);
      }
      MosPrintf(MIL_TEXT("\nThe search time was %.1f ms.\n\n"), Time * 1000.0);

      /* 오버레이 */
      MgraControl(M_DEFAULT, M_COLOR, PositionDrawColor);
      MmodDraw(M_DEFAULT, MilResult, GraphicList, M_DRAW_POSITION, M_DEFAULT, M_DEFAULT);
      MgraControl(M_DEFAULT, M_COLOR, ModelDrawColor);
      MmodDraw(M_DEFAULT, MilResult, GraphicList, M_DRAW_EDGES, M_DEFAULT, M_DEFAULT);
   }
   else
   {
      MosPrintf(MIL_TEXT("The circles were not found or too many occurrences!\n\n"));
   }

   MosPrintf(MIL_TEXT("Press any key to continue.\n\n"));
   MosGetch();

   /* 해제 */
   McalFree(MilCalibration);
   MgraFree(GraphicList);
   MbufFree(MilImage);
   MmodFree(MilSearchContext);
   MmodFree(MilResult);
}

/******************************************************************************/
/* [소형 원 예제] 해상도 Coarseness 레벨 조정으로 작은 원 검출 성능 향상      */
/******************************************************************************/
#define SMALL_CIRCLE_IMAGE   M_IMAGE_PATH MIL_TEXT("/CircleShapeFinder/ManySmallCircles.mim")
#define MODEL_RADIUS_3       5.0

void SmallCircleSearchExample(MIL_ID MilSystem, MIL_ID MilDisplay)
{
   MIL_ID MilImage, GraphicList, MilSearchContext, MilResult;
   MIL_INT NumResults = 0L;
   MIL_DOUBLE Score[MODEL_MAX_OCCURRENCES], XPosition[MODEL_MAX_OCCURRENCES],
              YPosition[MODEL_MAX_OCCURRENCES], Radius[MODEL_MAX_OCCURRENCES],
              Time = 0.0;

   MosPrintf(MIL_TEXT("\nUsing model finder M_SHAPE_CIRCLE with M_RESOLUTION_COARSENESS_LEVEL control\n"));
   MosPrintf(MIL_TEXT("----------------------------------------------------------------------------\n\n"));

   /* 타깃 로드/표시 */
   MbufRestore(SMALL_CIRCLE_IMAGE, MilSystem, &MilImage);
   MdispControl(MilDisplay, M_TITLE, MIL_TEXT("Target image"));
   MdispSelect(MilDisplay, MilImage);

   /* 그래픽 리스트 연결 */
   MgraAllocList(MilSystem, M_DEFAULT, &GraphicList);
   MdispControl(MilDisplay, M_ASSOCIATED_GRAPHIC_LIST_ID, GraphicList);

   /* 컨텍스트/결과 할당, 모델 정의 */
   MmodAlloc(MilSystem, M_SHAPE_CIRCLE, M_DEFAULT, &MilSearchContext);
   MmodAllocResult(MilSystem, M_SHAPE_CIRCLE, &MilResult);
   MmodDefine(MilSearchContext, M_DEFAULT, M_DEFAULT, MODEL_RADIUS_3, M_DEFAULT, M_DEFAULT, M_DEFAULT);

   /* 모든 발생 찾기 */
   MmodControl(MilSearchContext, 0, M_NUMBER, M_ALL);

   /* 사전처리 */
   MmodPreprocess(MilSearchContext, M_DEFAULT);

   /* 설명 */
   MosPrintf(MIL_TEXT("A circle model was defined with a nominal radius of %-3.1f%.\n\n"), MODEL_RADIUS_3);
   MosPrintf(MIL_TEXT("a) M_RESOLUTION_COARSENESS_LEVEL = 50 (default)\n"));
   MosPrintf(MIL_TEXT("Press any key to continue.\n"));
   MosGetch();

   /* 람다: 탐색/결과 출력/오버레이 공통 처리 */
   auto FindAndDisplayResults = [&]()
   {
      MappTimer(M_DEFAULT, M_TIMER_RESET + M_SYNCHRONOUS, M_NULL);
      MmodFind(MilSearchContext, MilImage, MilResult);
      MappTimer(M_DEFAULT, M_TIMER_READ + M_SYNCHRONOUS, &Time);

      MmodGetResult(MilResult, M_DEFAULT, M_NUMBER + M_TYPE_MIL_INT, &NumResults);

      if ((NumResults >= 1) && (NumResults <= MODEL_MAX_OCCURRENCES))
      {
         MmodGetResult(MilResult, M_DEFAULT, M_POSITION_X, XPosition);
         MmodGetResult(MilResult, M_DEFAULT, M_POSITION_Y, YPosition);
         MmodGetResult(MilResult, M_DEFAULT, M_RADIUS,     Radius);
         MmodGetResult(MilResult, M_DEFAULT, M_SCORE,      Score);

         MosPrintf(MIL_TEXT("Result   X-Position   Y-Position   Radius   Score\n\n"));
         for (int i = 0; i < NumResults; i++)
         {
            MosPrintf(MIL_TEXT("%-9d%-13.2f%-13.2f%-8.2f%-5.2f%%\n"),
                      i, XPosition[i], YPosition[i], Radius[i], Score[i]);
         }
         MosPrintf(MIL_TEXT("\nThe search time was %.1f ms.\n\n"), Time * 1000.0);

         /* 각 결과에 엣지/박스/포지션 오버레이 */
         for (int i = 0; i < NumResults; i++)
         {
            MgraControl(M_DEFAULT, M_COLOR, M_COLOR_RED);
            MmodDraw(M_DEFAULT, MilResult, GraphicList,
                     M_DRAW_EDGES + M_DRAW_BOX + M_DRAW_POSITION, i, M_DEFAULT);
         }
      }
      else
      {
         MosPrintf(MIL_TEXT("The circles were not found or too many occurrences!\n\n"));
      }
   };

   /* 기본 coarseness 50으로 탐색 */
   FindAndDisplayResults();

   MosPrintf(MIL_TEXT("Some occurrences are missed. Decreasing M_RESOLUTION_COARSENESS_LEVEL helps.\n\n"));
   MosPrintf(MIL_TEXT("b) M_RESOLUTION_COARSENESS_LEVEL = 40\n"));
   MosPrintf(MIL_TEXT("Press any key to continue.\n"));
   MosGetch();

   /* 주석 제거 후 coarseness 낮추고 재탐색 */
   MgraClear(M_DEFAULT, GraphicList);
   MmodControl(MilSearchContext, M_CONTEXT, M_RESOLUTION_COARSENESS_LEVEL, 40);
   FindAndDisplayResults();

   MosPrintf(MIL_TEXT("Now, all occurrences are found with higher scores.\n\n"));
   MosPrintf(MIL_TEXT("Press any key to end.\n"));
   MosGetch();

   /* 해제 */
   MgraFree(GraphicList);
   MbufFree(MilImage);
   MmodFree(MilSearchContext);
   MmodFree(MilResult);
}
