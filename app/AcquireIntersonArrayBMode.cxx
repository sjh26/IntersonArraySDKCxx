/*=========================================================================

Library:   IntersonArray

Copyright Kitware Inc. 28 Corporate Drive,
Clifton Park, NY, 12065, USA.

All rights reserved.

Licensed under the Apache License, Version 2.0 ( the "License" );
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

=========================================================================*/
#include <Windows.h> // Sleep

#include "itkImageFileWriter.h"

#include "IntersonArrayCxxControlsHWControls.h"
#include "IntersonArrayCxxImagingCapture.h"
#include "IntersonArrayCxxImagingScanConverter.h"

#include "AcquireIntersonArrayBModeCLP.h"

typedef IntersonArrayCxx::Imaging::Scan2DClass Scan2DClassType;

const unsigned int Dimension = 3;
typedef Scan2DClassType::BmodePixelType    PixelType;
typedef itk::Image< PixelType, Dimension > ImageType;


struct CallbackClientData
{
  ImageType *        Image;
  itk::SizeValueType FrameIndex;
};


void __stdcall pasteIntoImage( PixelType * buffer, void * clientData )
{
  CallbackClientData * callbackClientData = static_cast< CallbackClientData * >( clientData );
  ImageType * image = callbackClientData->Image;

  const ImageType::RegionType & largestRegion = image->GetLargestPossibleRegion();
  const ImageType::SizeType imageSize = largestRegion.GetSize();
  const itk::SizeValueType imageFrames = imageSize[2];
  if( callbackClientData->FrameIndex >= imageFrames )
    {
    return;
    }

  const int maxVectors = Scan2DClassType::MAX_VECTORS;
  const int maxSamples = Scan2DClassType::MAX_SAMPLES;
  const int framePixels = maxVectors * maxSamples;

  PixelType * imageBuffer = image->GetPixelContainer()->GetBufferPointer();
  imageBuffer += framePixels * callbackClientData->FrameIndex;
  std::memcpy( imageBuffer, buffer, framePixels * sizeof( PixelType ) );

  std::cout << "Acquired frame: " << callbackClientData->FrameIndex << std::endl;
  ++(callbackClientData->FrameIndex);
}


void __stdcall pasteIntoScanConvertedImage( PixelType * buffer, void * clientData )
{
  CallbackClientData * callbackClientData = static_cast< CallbackClientData * >( clientData );
  ImageType * image = callbackClientData->Image;

  const ImageType::RegionType & largestRegion = image->GetLargestPossibleRegion();
  const ImageType::SizeType imageSize = largestRegion.GetSize();
  const itk::SizeValueType imageFrames = imageSize[2];
  if( callbackClientData->FrameIndex >= imageFrames )
    {
    return;
    }

  const int framePixels = imageSize[0] * imageSize[1];

  PixelType * imageBuffer = image->GetPixelContainer()->GetBufferPointer();
  imageBuffer += framePixels * callbackClientData->FrameIndex;
  std::memcpy( imageBuffer, buffer, framePixels * sizeof( PixelType ) );

  std::cout << "Acquired frame: " << callbackClientData->FrameIndex << std::endl;
  ++(callbackClientData->FrameIndex);
}


int main( int argc, char * argv[] )
{
  PARSE_ARGS;

  typedef IntersonArrayCxx::Controls::HWControls HWControlsType;
  IntersonArrayCxx::Controls::HWControls hwControls;

  Scan2DClassType scan2D;

  typedef IntersonArrayCxx::Imaging::ScanConverter ScanConverterType;
  ScanConverterType scanConverter;

  typedef HWControlsType::FoundProbesType FoundProbesType;
  FoundProbesType foundProbes;
  hwControls.FindAllProbes( foundProbes );
  if( foundProbes.empty() )
    {
    std::cerr << "Could not find the probe." << std::endl;
    return EXIT_FAILURE;
    }
  hwControls.FindMyProbe( 0 );
  const unsigned int probeId = hwControls.GetProbeID();
  if( probeId == 0 )
    {
    std::cerr << "Could not find the probe." << std::endl;
    return EXIT_FAILURE;
    }

  const bool upDown = false;
  const bool leftRight = false;
  const int width = 1000;
  const int height = 650;
  if( hwControls.ValidDepth( depth ) == depth )
    {
    ScanConverterType::ScanConverterError converterError =
      scanConverter.HardInitScanConverter( depth,
                                           upDown,
                                           leftRight,
                                           width,
                                           height );
    if( converterError != ScanConverterType::SUCCESS )
      {
      std::cerr << "Error during hard scan converter initialization: "
                << converterError << std::endl;
      return EXIT_FAILURE;
      }
    }
  else
    {
    std::cerr << "Invalid requested depth for probe." << std::endl;
    }


  const itk::SizeValueType framesToCollect = frames;
  const int maxVectors = Scan2DClassType::MAX_VECTORS;
  const int maxSamples = Scan2DClassType::MAX_SAMPLES;

  ImageType::Pointer image = ImageType::New();
  typedef ImageType::RegionType RegionType;
  RegionType imageRegion;
  ImageType::IndexType imageIndex;
  imageIndex.Fill( 0 );
  imageRegion.SetIndex( imageIndex );
  ImageType::SizeType imageSize;
  if( scanConvert )
    {
    imageSize[0] = width;
    imageSize[1] = height;
    }
  else
    {
    imageSize[0] = maxSamples;
    imageSize[1] = maxVectors;
    }
  imageSize[2] = framesToCollect;
  imageRegion.SetSize( imageSize );
  image->SetRegions( imageRegion );
  image->Allocate();

  CallbackClientData clientData;

  clientData.Image = image.GetPointer();
  clientData.FrameIndex = 0;

  if( scanConvert )
    {
    scan2D.SetNewScanConvertedBmodeImageCallback( &pasteIntoScanConvertedImage, &clientData );
    }
  else
    {
    scan2D.SetNewBmodeImageCallback( &pasteIntoImage, &clientData );
    }

  HWControlsType::FrequenciesType frequencies;
  hwControls.GetFrequency( frequencies );
  if( !hwControls.SetFrequency( frequencies[frequencyIndex] ) )
    {
    std::cerr << "Could not set the frequency." << std::endl;
    return EXIT_FAILURE;
    }

  if( !hwControls.SendHighVoltage( highVoltage ) )
    {
    std::cerr << "Could not set the high voltage." << std::endl;
    return EXIT_FAILURE;
    }
  if( !hwControls.EnableHighVoltage() )
    {
    std::cerr << "Could not enable high voltage." << std::endl;
    return EXIT_FAILURE;
    }

  if( !hwControls.SendDynamic( gain ) )
    {
    std::cerr << "Could not set dynamic gain." << std::endl;
    }

  hwControls.DisableHardButton();

  scan2D.AbortScan();
  scan2D.SetRFData( false );
  if( !hwControls.StartMotor() )
    {
    std::cerr << "Could not start motor." << std::endl;
    return EXIT_FAILURE;
    };
  scan2D.StartReadScan();
  Sleep( 100 ); // "time to start"
  if( !hwControls.StartBmode() )
    {
    std::cerr << "Could not start B-mode collection." << std::endl;
    return EXIT_FAILURE;
    };

  while( clientData.FrameIndex < framesToCollect )
    {
    Sleep( 100 );
    }

  hwControls.StopAcquisition();
  scan2D.StopReadScan();
  Sleep( 100 ); // "time to stop"
  scan2D.DisposeScan();
  hwControls.StopMotor();

  typedef itk::ImageFileWriter< ImageType > WriterType;
  WriterType::Pointer writer = WriterType::New();
  writer->SetFileName( outputImage );
  writer->SetInput( image );
  try
    {
    writer->Update();
    }
  catch( itk::ExceptionObject & error )
    {
    std::cerr << "Error: " << error << std::endl;
    return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;
}
