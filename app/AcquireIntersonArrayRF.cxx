#include <Windows.h> // Sleep

#include "itkImageFileWriter.h"

#include "IntersonCxxImagingScan2DClass.h"
#include "IntersonCxxControlsHWControls.h"

#include "AcquireIntersonRFCLP.h"

typedef IntersonCxx::Imaging::Scan2DClass Scan2DClassType;

const unsigned int Dimension = 3;
typedef Scan2DClassType::RFPixelType       PixelType;
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
  const itk::SizeValueType imageFrames = largestRegion.GetSize()[2];
  if( callbackClientData->FrameIndex >= imageFrames )
    {
    return;
    }

  const int maxVectors = Scan2DClassType::MAX_VECTORS;
  const int maxSamples = Scan2DClassType::MAX_RFSAMPLES;
  const int framePixels = maxVectors * maxSamples;

  PixelType * imageBuffer = image->GetPixelContainer()->GetBufferPointer();
  imageBuffer += framePixels * callbackClientData->FrameIndex;
  std::memcpy( imageBuffer, buffer, framePixels * sizeof( PixelType ) );

  std::cout << "Acquired frame: " << callbackClientData->FrameIndex << std::endl;
  ++(callbackClientData->FrameIndex);
}

int main( int argc, char * argv[] )
{
  PARSE_ARGS;


  typedef IntersonCxx::Controls::HWControls HWControlsType;
  IntersonCxx::Controls::HWControls hwControls;

  Scan2DClassType scan2D;

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

  int ret = EXIT_SUCCESS;

  const itk::SizeValueType framesToCollect = frames;
  const int maxVectors = Scan2DClassType::MAX_VECTORS;
  const int maxSamples = Scan2DClassType::MAX_RFSAMPLES;

  ImageType::Pointer image = ImageType::New();
  typedef ImageType::RegionType RegionType;
  RegionType imageRegion;
  ImageType::IndexType imageIndex;
  imageIndex.Fill( 0 );
  imageRegion.SetIndex( imageIndex );
  ImageType::SizeType imageSize;
  imageSize[0] = maxSamples;
  imageSize[1] = maxVectors;
  imageSize[2] = framesToCollect;
  imageRegion.SetSize( imageSize );
  image->SetRegions( imageRegion );
  image->Allocate();

  CallbackClientData clientData;
  clientData.Image = image.GetPointer();
  clientData.FrameIndex = 0;

  scan2D.SetNewRFImageCallback( &pasteIntoImage, &clientData );

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

  if( decimator )
    {
    if( !hwControls.EnableRFDecimator() )
      {
      std::cerr << "Could not enable RF decimator." << std::endl;
      return EXIT_FAILURE;
      }
    }
  else
    {
    if( !hwControls.DisableRFDecimator() )
      {
      std::cerr << "Could not disable RF decimator." << std::endl;
      return EXIT_FAILURE;
      }
    }

  hwControls.DisableHardButton();

  scan2D.AbortScan();
  scan2D.SetRFData( true );
  if( !hwControls.StartMotor() )
    {
    std::cerr << "Could not start motor." << std::endl;
    return EXIT_FAILURE;
    };
  scan2D.StartRFReadScan();
  Sleep( 100 ); // "time to start"
  if( !hwControls.StartRFmode() )
    {
    std::cerr << "Could not start RF collection." << std::endl;
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

  return ret;
}
