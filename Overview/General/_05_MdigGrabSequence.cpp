﻿/********************************************************************************/
/*
 * File name: MDigGrabSequence.cpp
 *
 * Synopsis:  This example shows how to record a sequence and play it back in 
 *            real time. the acquisition can be done in memory only or to 
 *            an AVI file. 
 *
 * NOTE:      This example assumes that the hard disk is sufficiently fast 
 *            to keep up with the grab. Also, removing the sequence display or 
 *            the text annotation while grabbing will reduce the CPU usage and
 *            might help if some frames are missed during acquisition. 
 *            If the disk or system are not fast enough, choose the option to 
 *            record uncompressed images to memory.
 *
 * Copyright © Matrox Electronic Systems Ltd., 1992-2025.
 * All Rights Reserved
 */
#include <mil.h>

/* Sequence file name.*/
#define SEQUENCE_FILE M_TEMP_DIR MIL_TEXT("MilSequence.avi")

/* Quantization factor to use during the compression.
   Valid values are 1 to 99 (higher to lower quality). 
*/
#define COMPRESSION_Q_FACTOR 50

/* Annotation flag. Set to M_YES to draw the frame number in the saved image. */
#define FRAME_NUMBER_ANNOTATION M_YES

/* Maximum number of images for the multiple buffering grab. */
#define NB_GRAB_IMAGE_MAX 20

/* User's record hook function prototype (called for every grabbed frame). */
MIL_INT MFTYPE RecordFunction(MIL_INT HookType, MIL_ID HookId, void* HookDataPtr);

/* User's record function hook data structure. */
typedef struct
   {
   MIL_ID MilSystem;
   MIL_ID MilDisplay;
   MIL_ID MilImageDisp;
   MIL_ID MilCompressedImage;
   MIL_INT NbGrabbedFrames;
   MIL_INT SaveSequenceToDisk;
   } HookDataStruct;


/* Main function. */
/* -------------- */
int MosMain(void)
{    
   MIL_ID     MilApplication, MilRemoteApplication, MilSystem, MilDigitizer, MilDisplay, MilImageDisp;
   MIL_ID     MilGrabImages[NB_GRAB_IMAGE_MAX];
   MIL_ID     MilCompressedImage = M_NULL;
   MIL_INT    CompressAttribute=0;
   MIL_INT    NbFrames=0,   Selection=1,   LicenseModules=0,   n=0;
   MIL_INT    FrameCount=0, FrameMissed=0, NbFramesReplayed=0, Exit=0;
   MIL_DOUBLE FrameRate=0,  TimeWait=0,    TotalReplay=0;
   MIL_INT    SaveSequenceToDisk = M_NO;
   HookDataStruct UserHookData;   

   /* Allocate defaults. */
   MappAllocDefault(M_DEFAULT, &MilApplication, &MilSystem, &MilDisplay,
                                              &MilDigitizer, M_NULL);

   /* Allocate an image and display it. */
   MbufAllocColor(MilSystem,
                  MdigInquire(MilDigitizer, M_SIZE_BAND, M_NULL),
                  MdigInquire(MilDigitizer, M_SIZE_X, M_NULL),
                  MdigInquire(MilDigitizer, M_SIZE_Y, M_NULL),
                  8L+M_UNSIGNED,
                  M_IMAGE+M_GRAB+M_DISP, &MilImageDisp);
   MbufClear(MilImageDisp, 0x0);
   MdispSelect(MilDisplay, MilImageDisp);

   /* Grab continuously on display. */
   MdigGrabContinuous(MilDigitizer, MilImageDisp);

   /* Print a message */
   MosPrintf(MIL_TEXT("\nSEQUENCE ACQUISITION:\n"));
   MosPrintf(MIL_TEXT("--------------------\n\n"));

   /* Inquire MIL licenses. */
   MsysInquire(MilSystem, M_OWNER_APPLICATION, &MilRemoteApplication);
   MappInquire(MilRemoteApplication, M_LICENSE_MODULES, &LicenseModules);

   /* Ask for the sequence format. */
   MosPrintf(MIL_TEXT("Choose the sequence format:\n"));
   MosPrintf(MIL_TEXT("1) Uncompressed images to memory (up to %ld frames).\n"), NB_GRAB_IMAGE_MAX );
   MosPrintf(MIL_TEXT("2) Uncompressed images to an AVI file.\n") );
   if (LicenseModules & (M_LICENSE_JPEGSTD | M_LICENSE_JPEG2000))
      {
      if(LicenseModules & M_LICENSE_JPEGSTD)
         MosPrintf(MIL_TEXT("3) Compressed lossy JPEG images to an AVI file.\n"));
      if(LicenseModules & M_LICENSE_JPEG2000)
         MosPrintf(MIL_TEXT("4) Compressed lossy JPEG2000 images to an AVI file.\n"));
      }
   
   bool ValidSelection = false;
   /* Set the buffer attribute. */
   while (!ValidSelection)
      {
      Selection = MosGetch();
      ValidSelection = true;
      switch (Selection)
         {
         case '1':
         case '\r':
            MosPrintf(MIL_TEXT("\nUncompressed images to memory selected.\n"));
            CompressAttribute = M_NULL;
            SaveSequenceToDisk = M_NO;
            break;

         case '2':
            MosPrintf(MIL_TEXT("\nUncompressed images to file selected.\n"));
            CompressAttribute = M_NULL;
            SaveSequenceToDisk = M_YES;
            break;

         case '3':
            MosPrintf(MIL_TEXT("\nJPEG images to file selected.\n"));
            CompressAttribute = M_COMPRESS + M_JPEG_LOSSY;
            SaveSequenceToDisk = M_YES;
            break;

         case '4':
            MosPrintf(MIL_TEXT("\nJPEG 2000 images to file selected.\n"));
            CompressAttribute = M_COMPRESS + M_JPEG2000_LOSSY;
            SaveSequenceToDisk = M_YES;
            break;

         default:
            MosPrintf(MIL_TEXT("\nInvalid selection !.\n"));
            ValidSelection = false;
            break;
         }
      }

   /* Allocate a compressed buffer if required. */
   if (CompressAttribute)
      {
      MbufAllocColor(MilSystem,
                     MdigInquire(MilDigitizer, M_SIZE_BAND, M_NULL),
                     MdigInquire(MilDigitizer, M_SIZE_X, M_NULL),
                     MdigInquire(MilDigitizer, M_SIZE_Y, M_NULL),
                     8L+M_UNSIGNED, M_IMAGE+CompressAttribute, &MilCompressedImage);
      MbufControl(MilCompressedImage, M_Q_FACTOR, COMPRESSION_Q_FACTOR);
      }

   /* Allocate the grab buffers to hold the sequence buffering. */
   for (NbFrames=0, n=0; n<NB_GRAB_IMAGE_MAX; n++)
      {
      /* Minimum number of buffers required. */
      if (n == 2)
         MappControl(M_DEFAULT, M_ERROR, M_PRINT_DISABLE);

      MbufAllocColor(MilSystem,
                     MdigInquire(MilDigitizer, M_SIZE_BAND, M_NULL),
                     MdigInquire(MilDigitizer, M_SIZE_X, M_NULL),
                     MdigInquire(MilDigitizer, M_SIZE_Y, M_NULL),
                     8L+M_UNSIGNED, M_IMAGE+M_GRAB, &MilGrabImages[n]);
      if (MilGrabImages[n])
        {
         NbFrames++;
         MbufClear(MilGrabImages[n], 0xFF);
        }
      else
         break;
      }
   MappControl(M_DEFAULT, M_ERROR, M_PRINT_ENABLE);
   
   /* Halt continuous grab. */
   MdigHalt(MilDigitizer);

   /* If the sequence must be saved to disk. */
   if (SaveSequenceToDisk)
       {
       /* Open the AVI file if required. */
       MosPrintf(MIL_TEXT("\nSaving the sequence to an AVI file...\n"));
       MbufExportSequence(SEQUENCE_FILE, M_DEFAULT, M_NULL, M_NULL, M_DEFAULT, M_OPEN);
       }
   else
       MosPrintf(MIL_TEXT("\nSaving the sequence to memory...\n\n"));
       
   /* Initialize User's archiving function hook data structure. */
   UserHookData.MilSystem           = MilSystem;
   UserHookData.MilDisplay          = MilDisplay;
   UserHookData.MilImageDisp        = MilImageDisp;
   UserHookData.MilCompressedImage  = MilCompressedImage;
   UserHookData.SaveSequenceToDisk  = SaveSequenceToDisk;
   UserHookData.NbGrabbedFrames     = 0;

   /* Acquire the sequence. The processing hook function will
      be called for each image grabbed to record and display it. 
      If sequence is not saved to disk, stop after NbFrames.
   */
   MdigProcess(MilDigitizer, MilGrabImages, NbFrames, 
               SaveSequenceToDisk ? M_START : M_SEQUENCE, 
               M_DEFAULT, RecordFunction, &UserHookData);

   /* Wait for a key press. */
   if (SaveSequenceToDisk)
      {
      MosPrintf(MIL_TEXT("\nPress any key to stop recording.\n\n"));
      MosGetch();
      }

   /* Wait until we have at least 2 frames to avoid an invalid frame rate. */
   do
      {
      MdigInquire(MilDigitizer, M_PROCESS_FRAME_COUNT, &FrameCount);
      }
   while(FrameCount < 2);

   /* Stop the sequence acquisition. */
   MdigProcess(MilDigitizer, MilGrabImages, NbFrames, M_STOP, 
                             M_DEFAULT, RecordFunction, &UserHookData);

   /* Read and print final statistics. */
   MdigInquire(MilDigitizer, M_PROCESS_FRAME_COUNT,  &FrameCount);
   MdigInquire(MilDigitizer, M_PROCESS_FRAME_RATE,   &FrameRate);
   MdigInquire(MilDigitizer, M_PROCESS_FRAME_MISSED, &FrameMissed);
   MosPrintf(MIL_TEXT("\n\n%d frames recorded (%d missed), at %.1f frames/sec ")
             MIL_TEXT("(%.1f ms/frame).\n\n"), (int)UserHookData.NbGrabbedFrames, 
                                           (int)FrameMissed, FrameRate, 1000.0/FrameRate);

   /* Sequence file closing if required. */
   if (SaveSequenceToDisk)
       MbufExportSequence(SEQUENCE_FILE, M_DEFAULT, M_NULL, M_NULL, FrameRate, M_CLOSE);

   /* Wait for a key to playback. */
   MosPrintf(MIL_TEXT("Press any key to start the sequence playback.\n"));
   MosGetch();

   /* Playback the sequence until a key is pressed. */
   if (UserHookData.NbGrabbedFrames > 0)
      {
      MIL_INT KeyPressed = 0;
      do
         {
         /* If sequence must be loaded. */
         if (SaveSequenceToDisk)
            {
            /* Inquire information about the sequence. */
            MosPrintf(MIL_TEXT("\nPlaying sequence from the AVI file...\n"));
            MosPrintf(MIL_TEXT("Press any key to end playback.\n\n"));
            MbufDiskInquire(SEQUENCE_FILE, M_NUMBER_OF_IMAGES, &FrameCount);
            MbufDiskInquire(SEQUENCE_FILE, M_FRAME_RATE, &FrameRate);
            MbufDiskInquire(SEQUENCE_FILE, M_COMPRESSION_TYPE, &CompressAttribute);

            /* Open the sequence file. */
            MbufImportSequence(SEQUENCE_FILE, M_DEFAULT, M_NULL, 
               M_NULL, M_NULL, M_NULL, M_NULL, M_OPEN);
            }
         else
             MosPrintf(MIL_TEXT("\nPlaying sequence from memory...\n\n"));

      
         /* Copy the images to the screen respecting the sequence frame rate. */
         TotalReplay = 0.0;
         NbFramesReplayed = 0;
         for (n=0; n < FrameCount; n++)
            {
            /* Reset the time. */
            MappTimer(M_DEFAULT, M_TIMER_RESET, M_NULL);

            /* If image was saved to disk. */
            if (SaveSequenceToDisk)
               {
               /* Load image directly to the display. */
               MbufImportSequence(SEQUENCE_FILE, M_DEFAULT, M_LOAD, M_NULL, &MilImageDisp, n, 1, M_READ);
               }
            else 
               {
               /* Copy the grabbed image to the display. */
               MbufCopy(MilGrabImages[n], MilImageDisp);
               }
            NbFramesReplayed++;
            MosPrintf(MIL_TEXT("Frame #%d             \r"), (int)NbFramesReplayed);

            /* Check for a pressed key to exit. */
            if (MosKbhit() && (n >= (NB_GRAB_IMAGE_MAX-1)))
               {
               MosGetch();
               break;
               }

            /* Wait to have a proper frame rate. */
            MappTimer(M_DEFAULT, M_TIMER_READ, &TimeWait);
            TotalReplay += TimeWait;
            TimeWait = (1/FrameRate) - TimeWait;
            MappTimer(M_DEFAULT, M_TIMER_WAIT, &TimeWait);
            TotalReplay += (TimeWait > 0) ? TimeWait: 0.0;
            }

         /* Close the sequence file. */
         if (SaveSequenceToDisk)
            MbufImportSequence(SEQUENCE_FILE, M_DEFAULT, M_NULL, 
               M_NULL, M_NULL, M_NULL, M_NULL, M_CLOSE);

         /* Print statistics. */
         MosPrintf(MIL_TEXT("\n\n%d frames replayed, at a frame rate of %.1f frames/sec ")
            MIL_TEXT("(%.1f ms/frame).\n\n"), (int)NbFramesReplayed, n/TotalReplay, 
            1000.0*TotalReplay/n);
         MosPrintf(MIL_TEXT("Press <Enter> to end (or any other key to playback again).\n"));
         KeyPressed = MosGetch();
         }
      while((KeyPressed != '\r') && (KeyPressed != '\n'));
      }

   /* Free all allocated buffers. */
   MbufFree(MilImageDisp);
   for (n=0; n<NbFrames; n++)
       MbufFree(MilGrabImages[n]);
   if (MilCompressedImage)
      MbufFree(MilCompressedImage);

   /* Free defaults. */
   MappFreeDefault(MilApplication, MilSystem, MilDisplay, MilDigitizer, M_NULL);

   return 0;
}


/* User's record function called each time a new buffer is grabbed. */
/* -------------------------------------------------------------------*/

/* Local defines for the annotations. */
#define STRING_LENGTH_MAX  20
#define STRING_POS_X       20
#define STRING_POS_Y       20

MIL_INT MFTYPE RecordFunction(MIL_INT HookType, MIL_ID HookId, void* HookDataPtr)
   {
   HookDataStruct *UserHookDataPtr = (HookDataStruct *)HookDataPtr;
   MIL_ID ModifiedImage = 0;
   MIL_INT n = 0;
   MIL_TEXT_CHAR Text[STRING_LENGTH_MAX]= {MIL_TEXT('\0'),};

   /* Retrieve the MIL_ID of the grabbed buffer. */
   MdigGetHookInfo(HookId, M_MODIFIED_BUFFER+M_BUFFER_ID, &ModifiedImage);

   /* Increment the frame count. */
   UserHookDataPtr->NbGrabbedFrames++;
   MosPrintf(MIL_TEXT("Frame #%d               \r"), (int)UserHookDataPtr->NbGrabbedFrames);

   /* Draw the frame count in the image if enabled. */
   if (FRAME_NUMBER_ANNOTATION == M_YES)
     {
     MosSprintf(Text, STRING_LENGTH_MAX, MIL_TEXT(" %d "), 
                                          (int)UserHookDataPtr->NbGrabbedFrames);
     MgraText(M_DEFAULT, ModifiedImage, STRING_POS_X, STRING_POS_Y, Text);
   }

   /* Copy the new grabbed image to the display. */
   MbufCopy(ModifiedImage, UserHookDataPtr->MilImageDisp);

   /* Compress the new image if required. */
   if (UserHookDataPtr->MilCompressedImage)
      {
      MbufCopy(ModifiedImage, UserHookDataPtr->MilCompressedImage);
      }

   /* Record the new image. */
   if (UserHookDataPtr->SaveSequenceToDisk)
      {
      MbufExportSequence(SEQUENCE_FILE, M_DEFAULT, UserHookDataPtr->MilCompressedImage? 
                         &UserHookDataPtr->MilCompressedImage: &ModifiedImage,
                         1, M_DEFAULT, M_WRITE);
      }

   return 0;
   }
