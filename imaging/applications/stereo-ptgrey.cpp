/**************************************************************************
 *
 * Title:   simplestereo
 * Copyright:   (C) 2006,2007,2008 Don Murray donm@ptgrey.com
 *
 * Description:
 *
 *    Get an image set from a Bumblebee or Bumblebee2 via DMA transfer
 *    using libdc1394 and process it with the Triclops stereo
 *    library. Based loosely on 'grab_gray_image' from libdc1394 examples.
 *
 *-------------------------------------------------------------------------
 *     License: LGPL
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *************************************************************************/

//=============================================================================
// Copyright © 2006,2007,2008 Point Grey Research, Inc. All Rights Reserved.
//
// This software is the confidential and proprietary information of Point
// Grey Research, Inc. ("Confidential Information").  You shall not
// disclose such Confidential Information and shall use it only in
// accordance with the terms of the license agreement you entered into
// with Point Grey Research Inc.
//
// PGR MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE SUITABILITY OF THE
// SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE, OR NON-INFRINGEMENT. PGR SHALL NOT BE LIABLE FOR ANY DAMAGES
// SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR DISTRIBUTING
// THIS SOFTWARE OR ITS DERIVATIVES.
//
//=============================================================================

//=============================================================================
//
// simplestereo.cpp
//
//=============================================================================

//=============================================================================
// System Includes
//=============================================================================
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include <dc1394/conversions.h>
#include <dc1394/control.h>
#include <dc1394/utils.h>


//=============================================================================
// PGR Includes
//=============================================================================
#include <snark/imaging/stereo/ptgrey/pgr_registers.h>
#include <snark/imaging/stereo/ptgrey/pgr_stereocam.h>


//=============================================================================
// a simple function to write a .pgm file
int
writePgm( char*     szFilename,
     unsigned char* pucBuffer,
     int        width,
     int        height )
{
  FILE* stream;
  stream = fopen( szFilename, "wb" );
  if( stream == NULL)
  {
     perror( "Can't open image file" );
     return 1;
  }

  fprintf( stream, "P5\n%u %u 255\n", width, height );
  fwrite( pucBuffer, width, height, stream );
  fclose( stream );
  return 0;
}

//=============================================================================
// a simple function to write a .ppm file
int
writePpm( char*     szFilename,
     unsigned char* pucBuffer,
     int        width,
     int        height )
{
  FILE* stream;
  stream = fopen( szFilename, "wb" );
  if( stream == NULL)
  {
     perror( "Can't open image file" );
     return 1;
  }

  fprintf( stream, "P6\n%u %u 255\n", width, height );
  fwrite( pucBuffer, 3*width, height, stream );
  fclose( stream );
  return 0;
}


//=============================================================================
// cleanup_and_exit()
// This is called when the program exits and destroys the existing connections
// to the 1394 drivers
void
cleanup_and_exit( dc1394camera_t* camera )
{
   dc1394_capture_stop( camera );
   dc1394_video_set_transmission( camera, DC1394_OFF );
   dc1394_camera_free( camera );
   exit( 0 );
}

int main( int argc, char *argv[] )
{
   dc1394camera_t*  camera;
   dc1394error_t    err;
   dc1394_t * d;
   dc1394camera_list_t * list;
   unsigned int nThisCam;

   // Find cameras on the 1394 buses
   d = dc1394_new ();

   // Enumerate cameras connected to the PC
   err = dc1394_camera_enumerate (d, &list);
   if ( err != DC1394_SUCCESS )
   {
       fprintf( stderr, "Unable to look for cameras\n\n"
             "Please check \n"
             "  - if the kernel modules `ieee1394',`raw1394' and `ohci1394' "
             "are loaded \n"
             "  - if you have read/write access to /dev/raw1394\n\n");
       return 1;
    }

    if (list->num == 0)
    {
        fprintf( stderr, "No cameras found!\n");
        return 1;
    }

    printf( "There were %d camera(s) found attached to your PC\n", list->num  );

    // Identify cameras. Use the first stereo camera that is found
    for ( nThisCam = 0; nThisCam < list->num; nThisCam++ )
    {
        camera = dc1394_camera_new(d, list->ids[nThisCam].guid);

        if(!camera)
        {
            printf("Failed to initialize camera with guid %llx", list->ids[nThisCam].guid);
            continue;
        }

        printf( "Camera %d model = '%s'\n", nThisCam, camera->model );

        if ( isStereoCamera(camera))
        {
            printf( "Using this camera\n" );
            break;
        }
        dc1394_camera_free(camera);
   }

   if ( nThisCam == list->num )
   {
      printf( "No stereo cameras were detected\n" );
      return 0;
   }

   // Free memory used by the camera list
   dc1394_camera_free_list (list);

   PGRStereoCamera_t stereoCamera;

   // query information about this stereo camera
   err = queryStereoCamera( camera, &stereoCamera );
   if ( err != DC1394_SUCCESS )
   {
      fprintf( stderr, "Cannot query all information from camera\n" );
      cleanup_and_exit( camera );
   }

   if ( stereoCamera.nBytesPerPixel != 2 )
   {
      // can't handle XB3 3 bytes per pixel
      fprintf( stderr,
           "Example has not been updated to work with XB3 in 3 camera mode yet!\n" );
      cleanup_and_exit( stereoCamera.camera );
   }

   // set the capture mode
   printf( "Setting stereo video capture mode\n" );
   err = setStereoVideoCapture( &stereoCamera );
   if ( err != DC1394_SUCCESS )
   {
      fprintf( stderr, "Could not set up video capture mode\n" );
      cleanup_and_exit( stereoCamera.camera );
   }

   // have the camera start sending us data
   printf( "Start transmission\n" );
   err = startTransmission( &stereoCamera );
   if ( err != DC1394_SUCCESS )
   {
      fprintf( stderr, "Unable to start camera iso transmission\n" );
      cleanup_and_exit( stereoCamera.camera );
   }

   // give the auto-gain algorithms a chance to catch up
   printf( "Giving auto-gain algorithm a chance to stabilize\n" );
   sleep( 5 );

   // Allocate all the buffers.
   // Unfortunately color processing is a bit inefficient because of the number of
   // data copies.  Color data needs to be
   // - de-interleaved into separate bayer tile images
   // - color processed into RGB images
   // - de-interleaved to extract the green channel for stereo (or other mono conversion)

   // size of buffer for all images at mono8
   unsigned int   nBufferSize = stereoCamera.nRows *
                                stereoCamera.nCols *
                                stereoCamera.nBytesPerPixel;
   // allocate a buffer to hold the de-interleaved images
   unsigned char* pucDeInterlacedBuffer = new unsigned char[ nBufferSize ];
   unsigned char* pucRGBBuffer = NULL;;
   unsigned char* pucGreenBuffer = NULL;

   TriclopsInput input;
   if ( stereoCamera.bColor )
   {
      unsigned char* pucRGBBuffer   = new unsigned char[ 3 * nBufferSize ];
      unsigned char* pucGreenBuffer     = new unsigned char[ nBufferSize ];
      unsigned char* pucRightRGB    = NULL;
      unsigned char* pucLeftRGB     = NULL;
      unsigned char* pucCenterRGB   = NULL;

      // get the images from the capture buffer and do all required processing
      // note: produces a TriclopsInput that can be used for stereo processing
      extractImagesColor( &stereoCamera,
              DC1394_BAYER_METHOD_NEAREST,
              pucDeInterlacedBuffer,
              pucRGBBuffer,
              pucGreenBuffer,
              &pucRightRGB,
              &pucLeftRGB,
              &pucCenterRGB,
              &input );

      if ( !writePpm( "right.ppm", pucRightRGB, stereoCamera.nCols, stereoCamera.nRows ) )
     printf( "wrote right.ppm\n" );
      if ( !writePpm( "left.ppm", pucLeftRGB, stereoCamera.nCols, stereoCamera.nRows ) )
     printf( "wrote left.ppm\n" );
   }
   else
   {
      unsigned char* pucRightMono   = NULL;
      unsigned char* pucLeftMono    = NULL;
      unsigned char* pucCenterMono  = NULL;
      // get the images from the capture buffer and do all required processing
      // note: produces a TriclopsInput that can be used for stereo processing
      extractImagesMono( &stereoCamera,
              pucDeInterlacedBuffer,
              &pucRightMono,
              &pucLeftMono,
              &pucCenterMono,
              &input );


      if ( !writePgm( "right.pgm", pucRightMono, stereoCamera.nCols, stereoCamera.nRows ) )
     printf( "wrote right.pgm\n" );
      if ( !writePgm( "left.pgm", pucLeftMono, stereoCamera.nCols, stereoCamera.nRows ) )
     printf( "wrote left.pgm\n" );
   }

   // do stereo processing
   TriclopsError e;
   TriclopsContext triclops;
   printf( "Getting TriclopsContext from camera (slowly)... \n" );
   e = getTriclopsContextFromCamera( &stereoCamera, &triclops );
   if ( e != TriclopsErrorOk )
   {
      fprintf( stderr, "Can't get context from camera\n" );
      cleanup_and_exit( camera );
      return 1;
   }
   printf( "...done\n" );

   // make sure we are in subpixel mode
    triclopsSetResolution( triclops, 960, 1280 );
   
   triclopsSetSubpixelInterpolation( triclops, 1 );
   e = triclopsRectify( triclops, &input );
   if ( e != TriclopsErrorOk )
   {
      fprintf( stderr, "triclopsRectify failed!\n" );
      triclopsDestroyContext( triclops );
      cleanup_and_exit( camera );
      return 1;
   }

   e = triclopsStereo( triclops );
   if ( e != TriclopsErrorOk )
   {
      fprintf( stderr, "triclopsStereo failed!\n" );
      triclopsDestroyContext( triclops );
      cleanup_and_exit( camera );
      return 1;
   }

   // get and save the rectified and disparity images
   TriclopsImage image;
   triclopsGetImage( triclops, TriImg_RECTIFIED, TriCam_REFERENCE, &image );
   triclopsSaveImage( &image, "rectified.pgm" );
   fprintf( stderr, "rectified, %d x %d \n", image.nrows, image.ncols );
   printf( "wrote 'rectified.pgm'\n" );
   TriclopsImage16 image16;
   triclopsGetImage16( triclops, TriImg16_DISPARITY, TriCam_REFERENCE, &image16 );
   triclopsSaveImage16( &image16, "disparity.pgm" );
   printf( "wrote 'disparity.pgm'\n" );

//    float remap[image.nrows][ image.ncols][2][2];
//     for ( int r = 0; r <  image.nrows; r++ )
//     {
//         for ( int c = 0; c <  image.ncols; c++ )
//         {
//             float rawr, rawc;
//             triclopsUnrectifyPixel( triclops, TriCam_RIGHT, r, c, &rawr, &rawc );
//             remap[r][c][0][0] = rawr;
//             remap[r][c][0][1] = rawc;
//             triclopsUnrectifyPixel( triclops, TriCam_LEFT, r, c, &rawr, &rawc );
//             remap[r][c][1][0] = rawr;
//             remap[r][c][1][1] = rawc;
//         }
//     }

   printf( "Stop transmission\n" );
   //  Stop data transmission
   if ( dc1394_video_set_transmission( stereoCamera.camera, DC1394_OFF ) != DC1394_SUCCESS )
   {
      fprintf( stderr, "Couldn't stop the camera?\n" );
   }


   delete[] pucDeInterlacedBuffer;
   if ( pucRGBBuffer )
      delete[] pucRGBBuffer;
   if ( pucGreenBuffer )
      delete[] pucGreenBuffer;

   // close camera
   cleanup_and_exit( camera );

   return 0;
}

