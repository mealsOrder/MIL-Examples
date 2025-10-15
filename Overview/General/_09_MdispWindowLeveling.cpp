/*
 * 파일명: MdispWindowLeveling.cpp
 *
 * 개요:
 *   - 10비트 모노 의료 영상을 디스플레이하고, LUT(룩업 테이블)로 윈도우/레벨을
 *     인터랙티브하게 조정하는 예제.
 *
 * 핵심 요약:
 *   - 최대 픽셀값을 처리로 계산(MimFindExtreme) → MbufControl(M_MAX)로 LUT 초기범위 설정.
 *   - 디스플레이 비트심도에 맞춰 1D LUT 버퍼(8/16bit) 생성 후 초기 램프 적용.
 *   - 키 조작:
 *       ←/→: 윈도우 좌/우 이동,  ↓/↑: 윈도우 좁힘/넓힘,
 *       L/U: 인플렉션(출력 레벨) 내림/올림,  R: 전체 초기화.
 *   - MgenLutRamp 3회로 [0~Start], [Start~End], [End~Max] 구간 램프/포화 LUT 생성 → MdispLut 즉시 반영.
 *   - DrawLutShape로 LUT 모양을 영상 위에 그려 시각화(성능 비용 높음 → 필요 시 비활성).
 *
 * 저작권:
 *   © Matrox Electronic Systems Ltd., 1992-2025. All Rights Reserved
 */
#include <mil.h>
#include <stdlib.h>

/* 로드할 이미지(10-bit 모노) */
#define IMAGE_NAME      MIL_TEXT("ArmsMono10bit.mim")
#define IMAGE_FILE      M_IMAGE_PATH IMAGE_NAME

/* LUT 모양 그리기(성능↑) — 필요 시 M_NO */
#define DRAW_LUT_SHAPE  M_YES

/* 유틸 함수 및 매크로 */
void DrawLutShape(MIL_ID MilDisplay,
                  MIL_ID MilOriginalImage,
                  MIL_ID MilImage,
                  MIL_INT Start,
                  MIL_INT End,
                  MIL_INT InflexionIntensity,
                  MIL_INT ImageMaxValue,
                  MIL_INT DisplayMaxValue);
#define MosMin(a, b) (((a) < (b)) ? (a) : (b))
#define MosMax(a, b) (((a) > (b)) ? (a) : (b))

int MosMain(void)
{
   /* 기본 MIL 자원 */
   MIL_ID MilApplication;        /* 애플리케이션 ID */
   MIL_ID MilSystem;             /* 시스템 ID */
   MIL_ID MilDisplay;            /* 디스플레이 ID */
   MIL_ID MilImage;              /* 표시/처리용 이미지 버퍼 */
   MIL_ID MilOriginalImage = 0;  /* LUT 모양 오버레이를 위한 원본 이미지 (지연 로드) */
   MIL_ID MilLut;                /* LUT 버퍼 */

   /* 영상/디스플레이 파라미터 */
   MIL_INT ImageSizeX, ImageSizeY, ImageMaxValue;
   MIL_INT DisplaySizeBit, DisplayMaxValue;

   /* 윈도우/레벨 파라미터 */
   MIL_INT Start, End, Step, InflectionLevel;
   MIL_INT Ch;

   /* 1) 애플리케이션/시스템/디스플레이 할당 */
   MappAllocDefault(M_DEFAULT, &MilApplication, &MilSystem, &MilDisplay, M_NULL, M_NULL);

   /* 2) 대상 이미지 로드 */
   MbufRestore(IMAGE_FILE, MilSystem, &MilImage);

   /* 3) 영상의 최대 픽셀값 계산 → LUT 초기화 참고값 설정 */
   MIL_ID MilExtremeResult = M_NULL;
   MimAllocResult((MIL_ID)MbufInquire(MilImage, M_OWNER_SYSTEM, M_NULL), 1L, M_EXTREME_LIST, &MilExtremeResult);
   MimFindExtreme(MilImage, MilExtremeResult, M_MAX_VALUE);
   MimGetResult(MilExtremeResult, M_VALUE, &ImageMaxValue);
   MimFree(MilExtremeResult);

   /* 디스플레이가 LUT 초기 범위를 알 수 있도록 영상의 최대값 등록 */
   MbufControl(MilImage, M_MAX, (MIL_DOUBLE)ImageMaxValue);

   /* 4) 디스플레이에 선택(별도 윈도우 쓰려면 MdispSelectWindow 사용) */
   MdispSelect(MilDisplay, MilImage);

   /* 5) 디스플레이 출력 비트 수 확인 → 최대 출력값 계산 */
   MdispInquire(MilDisplay, M_SIZE_BIT, &DisplaySizeBit);
   DisplayMaxValue = (1 << DisplaySizeBit) - 1;

   /* 안내 메시지 */
   MosPrintf(MIL_TEXT("\nINTERACTIVE WINDOW LEVELING:\n"));
   MosPrintf(MIL_TEXT("----------------------------\n\n"));
   MosPrintf(MIL_TEXT("Image name : %s\n"), IMAGE_NAME);
   MosPrintf(MIL_TEXT("Image size : %d x %d\n"),
             (int)MbufInquire(MilImage, M_SIZE_X, &ImageSizeX),
             (int)MbufInquire(MilImage, M_SIZE_Y, &ImageSizeY));
   MosPrintf(MIL_TEXT("Image max  : %4d\n"),   (int)ImageMaxValue);
   MosPrintf(MIL_TEXT("Display max: %4d\n\n"), (int)DisplayMaxValue);

   /* 6) LUT 버퍼 할당(길이: 이미지 최대값+1, 타입: 디스플레이 비트수 기준 8/16bit) */
   MbufAlloc1d(MilSystem, ImageMaxValue + 1,
               ((DisplaySizeBit > 8) ? 16 : 8) + M_UNSIGNED, M_LUT, &MilLut);

   /* 7) 초기 LUT: 전체 범위 램프(0→DisplayMax) + LUT 최대값 등록 */
   MgenLutRamp(MilLut, 0, 0, ImageMaxValue, (MIL_DOUBLE)DisplayMaxValue);
   MbufControl(MilLut, M_MAX, (MIL_DOUBLE)DisplayMaxValue);

   /* 8) 디스플레이에 LUT 적용 */
   MdispLut(MilDisplay, MilLut);

   /* 9) 조작 안내 */
   MosPrintf(MIL_TEXT("Keys assignment:\n\n"));
   MosPrintf(MIL_TEXT("Arrow keys :    Left=move Left, Right=move Right, Down=Narrower, Up=Wider.\n"));
   MosPrintf(MIL_TEXT("Intensity keys: L=Lower,  U=Upper,  R=Reset.\n"));
   MosPrintf(MIL_TEXT("Press any key to end.\n\n"));

   /* 10) 인터랙티브 윈도우/레벨 조정 루프 */
   Ch = 0;
   Start = 0;
   End   = ImageMaxValue;
   InflectionLevel = DisplayMaxValue;

   /* Step: 영상 다이내믹 레인지에 비례하게 설정(최소 4) */
   Step = (ImageMaxValue + 1) / 128;
   Step = MosMax(Step, 4);

   while (Ch != '\r')  /* Enter로 종료 */
   {
      switch (Ch)
      {
      /* ←: 윈도우 좌로 이동 */
      case 0x4B: { Start -= Step; End -= Step; break; }
      /* →: 윈도우 우로 이동 */
      case 0x4D: { Start += Step; End += Step; break; }
      /* ↓: 윈도우 좁힘 */
      case 0x50: { Start += Step; End -= Step; break; }
      /* ↑: 윈도우 넓힘 */
      case 0x48: { Start -= Step; End += Step; break; }
      /* L: 인플렉션 레벨 낮춤 */
      case 'L':
      case 'l': { InflectionLevel--; break; }
      /* U: 인플렉션 레벨 올림 */
      case 'U':
      case 'u': { InflectionLevel++; break; }
      /* R: 초기화 */
      case 'R':
      case 'r': { Start = 0; End = ImageMaxValue; InflectionLevel = DisplayMaxValue; break; }
      }

      /* 10-1) 범위 포화(Clamp) — 인덱스/레벨 모두 유효 범위 유지 */
      End   = MosMin(End, ImageMaxValue);
      Start = MosMin(Start, End);
      End   = MosMax(End, Start);
      Start = MosMax(Start, 0);
      End   = MosMax(End, 0);
      InflectionLevel = MosMax(InflectionLevel, 0);
      InflectionLevel = MosMin(InflectionLevel, DisplayMaxValue);

      MosPrintf(MIL_TEXT("Inflection points: Low=(%d,0), High=(%d,%d).   \r"),
                (int)Start, (int)End, InflectionLevel);

      /* 10-2) 3구간 LUT 생성: [0~Start], [Start~End], [End~Max] */
      MgenLutRamp(MilLut, 0, 0, Start, 0);  /* 좌측 포화 */
      MgenLutRamp(MilLut, Start, 0, End, (MIL_DOUBLE)InflectionLevel); /* 윈도우 구간 */
      MgenLutRamp(MilLut, End, (MIL_DOUBLE)InflectionLevel, ImageMaxValue, (MIL_DOUBLE)DisplayMaxValue); /* 우측 포화 */

      /* 10-3) LUT 적용 */
      MdispLut(MilDisplay, MilLut);

      /* 10-4) (옵션) LUT 모양을 영상 위에 그려 시각화 — 성능 비용 큼 */
      if (DRAW_LUT_SHAPE)
      {
         if (!MilOriginalImage)
            MbufRestore(IMAGE_FILE, MilSystem, &MilOriginalImage);

         DrawLutShape(MilDisplay, MilOriginalImage, MilImage,
                      Start, End, InflectionLevel, ImageMaxValue, DisplayMaxValue);
      }

      /* 10-5) 특수키(화살표) 처리: 0xE0 접두어 다음 코드 읽기 */
      if ((Ch = MosGetch()) == 0xE0)
         Ch = MosGetch();
   }
   MosPrintf(MIL_TEXT("\n\n"));

   /* 11) 자원 해제 */
   MbufFree(MilLut);
   MbufFree(MilImage);
   if (MilOriginalImage)
      MbufFree(MilOriginalImage);
   MappFreeDefault(MilApplication, MilSystem, MilDisplay, M_NULL, M_NULL);

   return 0;
}

/* ------------------------------------------------------------------------------------
 * DrawLutShape: 현재 LUT의 형태를 영상에 그려 시각화
 *  - 단순 주석(Annotation) 방식: 매번 전체 이미지를 다시 칠해 CPU 비용 큼(성능 주의)
 * ------------------------------------------------------------------------------------ */
void DrawLutShape(MIL_ID MilDisplay,
                  MIL_ID MilOriginalImage,
                  MIL_ID MilImage,
                  MIL_INT Start,
                  MIL_INT End,
                  MIL_INT InflexionIntensity,
                  MIL_INT ImageMaxValue,
                  MIL_INT DisplayMaxValue)
{
   MIL_INT        ImageSizeX, ImageSizeY;
   MIL_DOUBLE     Xstart, Xend, Xstep, Ymin, Yinf, Ymax, Ystep;
   MIL_TEXT_CHAR  String[8];

   /* 크기 질의 */
   MbufInquire(MilImage, M_SIZE_X, &ImageSizeX);
   MbufInquire(MilImage, M_SIZE_Y, &ImageSizeY);

   /* 좌표 변환 파라미터 계산
      - X축: 입력(이미지) 값 범위를 화면 너비로 스케일
      - Y축: 출력(디스플레이) 값을 화면 높이의 하단 1/4에 스케일 (아래로 갈수록 값 큼) */
   Xstep  = (MIL_DOUBLE)ImageSizeX / (MIL_DOUBLE)ImageMaxValue;
   Xstart = Start * Xstep;
   Xend   = End   * Xstep;
   Ystep  = ((MIL_DOUBLE)ImageSizeY / 4.0) / (MIL_DOUBLE)DisplayMaxValue;
   Ymin   = ((MIL_DOUBLE)ImageSizeY - 2);                 /* 하단(0 출력) */
   Yinf   = Ymin - (InflexionIntensity * Ystep);          /* 인플렉션 출력 위치 */
   Ymax   = Ymin - (DisplayMaxValue   * Ystep);           /* 상단(최대 출력) */

   /* 성능 향상을 위해 모든 주석 완료까지 디스플레이 갱신 비활성 */
   MdispControl(MilDisplay, M_UPDATE, M_DISABLE);

   /* 원본 영상 복원(이전 라인 지우기) */
   MbufCopy(MilOriginalImage, MilImage);

   /* 축 레이블/눈금 텍스트 */
   MgraControl(M_DEFAULT, M_COLOR, (MIL_DOUBLE)ImageMaxValue);
   MgraText(M_DEFAULT, MilImage, 4, (MIL_INT)Ymin - 22, MIL_TEXT("0"));
   MosSprintf(String, 8, MIL_TEXT("%d"), (int)DisplayMaxValue);
   MgraText(M_DEFAULT, MilImage, 4, (MIL_INT)Ymax - 16, String);
   MosSprintf(String, 8, MIL_TEXT("%d"), (int)ImageMaxValue);
   MgraText(M_DEFAULT, MilImage, ImageSizeX - 38, (MIL_INT)Ymin - 22, String);

   /* LUT 모양(3구간) 그리기: X=입력(이미지 값), Y=출력(디스플레이 값) */
   MgraLine(M_DEFAULT, MilImage, 0, (MIL_INT)Ymin, (MIL_INT)Xstart, (MIL_INT)Ymin);
   MgraLine(M_DEFAULT, MilImage, (MIL_INT)Xstart, (MIL_INT)Ymin, (MIL_INT)Xend, (MIL_INT)Yinf);
   MgraLine(M_DEFAULT, MilImage, (MIL_INT)Xend, (MIL_INT)Yinf, ImageSizeX - 1, (MIL_INT)Ymax);

   /* 갱신 재개 */
   MdispControl(MilDisplay, M_UPDATE, M_ENABLE);
}