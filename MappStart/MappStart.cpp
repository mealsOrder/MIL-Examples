
/*
 * 파일명(File name): MAppStart.cpp
 *
 * 개요(Synopsis):
 *   이 프로그램은 MIL 애플리케이션과 시스템을 할당한 후,
 *   그래픽 함수를 이용하여 환영 메시지를 화면에 표시하는 예제입니다.
 *   또한, 에러가 발생했을 때 이를 확인하는 방법을 보여줍니다.
 *
 * 저작권(Copyright):
 *   © Matrox Electronic Systems Ltd., 1992-2025.
 *   All Rights Reserved.
 */
#include <mil.h>   // MIL( Matrox Imaging Library ) 헤더 포함

 /*------------------------------------------------------------*/
 /* 프로그램 시작 함수                                         */
 /*------------------------------------------------------------*/
    int MosMain(void)
{
    /* MIL 리소스(자원) 식별자 선언 */
    MIL_ID MilApplication;  // 애플리케이션 ID
    MIL_ID MilSystem;       // 시스템 ID
    MIL_ID MilDisplay;      // 디스플레이(화면) ID
    MIL_ID MilImage;        // 이미지 버퍼 ID

    /*--------------------------------------------------------*/
    /* 1. MIL의 기본 자원 할당                               */
    /*--------------------------------------------------------*/
    // MappAllocDefault() 함수는 기본적인 MIL 자원을 한 번에 생성하는 함수입니다.
    //  - M_DEFAULT : 기본 설정으로 시스템을 선택
    //  - MilApplication, MilSystem, MilDisplay, MilImage 등은 각각의 자원 ID를 반환받습니다.
    //  - M_NULL : 사용하지 않는 디지타이저(Grabber)는 생략
    MappAllocDefault(M_DEFAULT, &MilApplication, &MilSystem, &MilDisplay, M_NULL, &MilImage);

    /*--------------------------------------------------------*/
    /* 2. 자원 할당 후 에러가 없는 경우 그래픽 처리 수행     */
    /*--------------------------------------------------------*/
    if (!MappGetError(M_DEFAULT, M_GLOBAL, M_NULL)) // 에러가 없으면 실행
    {
        /* 그래픽 요소(텍스트, 사각형) 그리기 */

        // 텍스트 색상 설정 (0xF0은 밝은 회색 계열)
        MgraControl(M_DEFAULT, M_COLOR, 0xF0);

        // 기본 큰 글꼴 설정
        MgraFont(M_DEFAULT, M_FONT_DEFAULT_LARGE);

        // 이미지 버퍼 위에 텍스트 출력 (좌표 160, 230)
        MgraText(M_DEFAULT, MilImage, 160L, 230L, MIL_TEXT(" Welcome to MIL !!! "));

        // 색상 변경 (0xC0은 좀 더 어두운 회색)
        MgraControl(M_DEFAULT, M_COLOR, 0xC0);

        // 사각형 3개를 그려 장식 효과 (겹치는 테두리)
        MgraRect(M_DEFAULT, MilImage, 100L, 150L, 530L, 340L);
        MgraRect(M_DEFAULT, MilImage, 120L, 170L, 510L, 320L);
        MgraRect(M_DEFAULT, MilImage, 140L, 190L, 490L, 300L);

        /* 콘솔창에 텍스트 메시지 출력 */
        MosPrintf(MIL_TEXT("\nSYSTEM ALLOCATION:\n"));
        MosPrintf(MIL_TEXT("------------------\n\n"));
        MosPrintf(MIL_TEXT("System allocation successful.\n\n"));
        MosPrintf(MIL_TEXT("     \"Welcome to MIL !!!\"\n\n"));
    }
    else
    {
        /* 에러가 발생한 경우 콘솔에 메시지 출력 */
        MosPrintf(MIL_TEXT("System allocation error !\n\n"));
    }

    /*--------------------------------------------------------*/
    /* 3. 키 입력 대기 후 종료 처리                          */
    /*--------------------------------------------------------*/
    MosPrintf(MIL_TEXT("Press any key to end.\n"));
    MosGetch();  // 사용자 키 입력 대기

    /*--------------------------------------------------------*/
    /* 4. 사용한 MIL 자원 해제                                */
    /*--------------------------------------------------------*/
    // MappFreeDefault()는 MappAllocDefault()로 생성한 모든 자원을 한 번에 해제
    MappFreeDefault(MilApplication, MilSystem, MilDisplay, M_NULL, MilImage);

    return 0;  // 프로그램 정상 종료
}