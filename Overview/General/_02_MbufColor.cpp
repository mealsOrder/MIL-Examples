/**********************************************************************************/
/* 
 * 파일명: MBufColor.cpp
 *
 * 개요:
 *   - 컬러 버퍼(이미지) 조작을 시연하는 예제
 *   - 디스플레이 가능한 컬러 이미지 버퍼를 만들고, 좌측에는 원본 이미지를 로드
 *   - 각 밴드(R/G/B) 별로 텍스트 주석을 그려 넣은 뒤
 *   - HSL로 변환 → L(명도) 성분에 오프셋 추가 → 다시 RGB로 변환하여 우측에 결과 표시
 *   - Child buffer를 이용해 하나의 디스플레이에서 두 장의 이미지를 나눠 표시
 * 
 * 핵심 요약:
 *   - MbufAllocColor: 컬러 이미지를 담을 버퍼를 생성합니다. 여기서는 소스 이미지의 가로 2배 크기로 만들어 좌/우로 비교 표시가 가능하게 했습니다.
 *   - MbufChild2d: 큰 메인 버퍼를 영역 기반(2D) 으로 쪼개 서브 버퍼(Child)를 만듭니다. 복사 없이 같은 메모리를 참조하므로 빠르고 메모리 효율적입니다.
 *   - MbufChildColor: 컬러 이미지에서 특정 밴드(R, G, B 또는 H, S, L)만 참조하는 Child 버퍼를 만듭니다. 특정 성분만 직접 조작할 때 유용합니다.
 *   - MimConvert (RGB↔HSL): 색 공간 변환. 명도 조절은 보통 HSL의 L만 다루는 것이 자연스럽습니다.
 *   - MimArith + M_SATURATION: 산술 연산 시 포화(saturation) 옵션으로 오버/언더플로우를 안전하게 잘라줍니다.
 *   - MdispSelect: 디스플레이에 어떤 버퍼를 보여줄지 지정합니다. (여기서는 메인 버퍼 MilImage)
 * 
 * 저작권:
 *   © Matrox Electronic Systems Ltd., 1992-2025. All Rights Reserved
 */

#include <mil.h> 

/* 소스 MIL 이미지 파일 (예: 새 이미지) */
#define IMAGE_FILE              M_IMAGE_PATH MIL_TEXT("Bird.mim")

/* 명도(Luminance)에 더할 오프셋 값 */
#define IMAGE_LUMINANCE_OFFSET  40L

/* 메인 함수 */
int MosMain(void)
{ 
   /* MIL 자원 ID들 */
   MIL_ID  MilApplication,        // 애플리케이션 ID
           MilSystem,             // 시스템 ID
           MilDisplay,            // 디스플레이 ID
           MilImage,              // 컬러 디스플레이용 메인 이미지 버퍼 ID
           MilLeftSubImage,       // 좌측(원본 표시) 서브 이미지 버퍼 ID
           MilRightSubImage,      // 우측(처리 결과 표시) 서브 이미지 버퍼 ID
           MilLumSubImage = 0,    // 우측 이미지의 L(명도) 성분 서브 이미지 ID
           MilRedBandSubImage,    // 좌측 이미지의 R 성분 서브 이미지 ID
           MilGreenBandSubImage,  // 좌측 이미지의 G 성분 서브 이미지 ID
           MilBlueBandSubImage;   // 좌측 이미지의 B 성분 서브 이미지 ID

   MIL_INT SizeX, SizeY, SizeBand, Type;

   /* 1) 기본 자원 할당(애플리케이션/시스템/디스플레이) */
   MappAllocDefault(M_DEFAULT, &MilApplication, &MilSystem, &MilDisplay, M_NULL, M_NULL);

   /* 2) 소스 이미지의 메타데이터(밴드 수, 크기, 타입)를 조회하여
         그 가로폭을 2배로 한 디스플레이용 컬러 버퍼를 할당 */
   MbufAllocColor(MilSystem,
                  MbufDiskInquire(IMAGE_FILE, M_SIZE_BAND, &SizeBand), // 밴드 수(보통 3: RGB)
                  MbufDiskInquire(IMAGE_FILE, M_SIZE_X, &SizeX) * 2,   // 가로 2배(좌:원본/우:결과)
                  MbufDiskInquire(IMAGE_FILE, M_SIZE_Y, &SizeY),       // 세로
                  MbufDiskInquire(IMAGE_FILE, M_TYPE,   &Type),        // 픽셀 타입
                  M_IMAGE + M_DISP + M_PROC,                           // 이미지/표시/처리 가능 플래그
                  &MilImage);

   /* 디스플레이 버퍼 초기화(검정) 후 디스플레이에 선택 */
   MbufClear(MilImage, 0L);
   MdispSelect(MilDisplay, MilImage);

   /* 3) 디스플레이용 메인 버퍼(MilImage)를 좌/우로 나눈 Child 2D 버퍼 생성
         - 좌측: 원본 표시 영역
         - 우측: 처리 결과 표시 영역 */
   MbufChild2d(MilImage, 0L,     0L, SizeX, SizeY, &MilLeftSubImage);
   MbufChild2d(MilImage, SizeX,  0L, SizeX, SizeY, &MilRightSubImage);

   /* 4) 좌측 서브 이미지에 원본 컬러 이미지 로드 */
   MbufLoad(IMAGE_FILE, MilLeftSubImage);
      
   /* 5) 좌측(원본) 서브 이미지의 R/G/B 성분을 각 밴드 Child로 매핑 */
   MbufChildColor(MilLeftSubImage, M_RED,   &MilRedBandSubImage);
   MbufChildColor(MilLeftSubImage, M_GREEN, &MilGreenBandSubImage);
   MbufChildColor(MilLeftSubImage, M_BLUE,  &MilBlueBandSubImage);

   /* 6) 각 밴드에 색상값을 직접 지정해 텍스트를 그려 넣는 예시
         (실전에서는 M_RGB888(...)로 한 번에 지정하는 방식이 더 단순함) */
   MgraControl(M_DEFAULT, M_COLOR, 0xFF);  // R 밴드(값 0xFF): 빨강 성분 강조
   MgraText(M_DEFAULT, MilRedBandSubImage,   SizeX/16, SizeY/8, MIL_TEXT(" TOUCAN "));
   MgraControl(M_DEFAULT, M_COLOR, 0x90);  // G 밴드(값 0x90): 초록 성분 강조
   MgraText(M_DEFAULT, MilGreenBandSubImage, SizeX/16, SizeY/8, MIL_TEXT(" TOUCAN "));
   MgraControl(M_DEFAULT, M_COLOR, 0x00);  // B 밴드(값 0x00): 파랑 성분 0
   MgraText(M_DEFAULT, MilBlueBandSubImage,  SizeX/16, SizeY/8, MIL_TEXT(" TOUCAN "));
 
   /* 안내 메시지 */
   MosPrintf(MIL_TEXT("\nCOLOR OPERATIONS:\n"));
   MosPrintf(MIL_TEXT("-----------------\n\n"));
   MosPrintf(MIL_TEXT("좌측에 컬러 원본을 로드하고, 각 R/G/B 밴드에 텍스트 주석을 그렸습니다.\n"));
   MosPrintf(MIL_TEXT("계속하려면 키를 누르세요.\n\n"));
   MosGetch();

   /* 7) 좌측(원본) → 우측(처리)로 컬러 공간 변환: RGB → HSL */
   MimConvert(MilLeftSubImage, MilRightSubImage, M_RGB_TO_HSL);
  
   /* 8) 우측(HSL) 이미지에서 L(명도) 성분만 Child로 참조 */
   MbufChildColor(MilRightSubImage, M_LUMINANCE, &MilLumSubImage);
     
   /* 9) L(명도) 성분에 상수 오프셋 더하기 (포화 처리 포함) */
   MimArith(MilLumSubImage, IMAGE_LUMINANCE_OFFSET, MilLumSubImage, 
                                                M_ADD_CONST + M_SATURATION);
  
   /* 10) 처리 결과를 다시 RGB로 변환: HSL → RGB (우측 영역에 그대로) */
   MimConvert(MilRightSubImage, MilRightSubImage, M_HSL_TO_RGB); 

   /* 안내 메시지 */
   MosPrintf(MIL_TEXT("명도(L) 성분에 오프셋을 더해 이미지가 더 밝아졌습니다.\n"));
   MosPrintf(MIL_TEXT("종료하려면 키를 누르세요.\n"));
   MosGetch();

   /* 11) 서브 이미지 및 메인 이미지 버퍼 해제(생성 역순 권장) */
   MbufFree(MilLumSubImage);
   MbufFree(MilRedBandSubImage);
   MbufFree(MilGreenBandSubImage);
   MbufFree(MilBlueBandSubImage);
   MbufFree(MilRightSubImage);
   MbufFree(MilLeftSubImage);
   MbufFree(MilImage);

   /* 12) 기본 자원 해제(애플리케이션/시스템/디스플레이) */
   MappFreeDefault(MilApplication, MilSystem, MilDisplay, M_NULL, M_NULL);

   return 0;
}

