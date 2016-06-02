#include <QMessageBox>
#include <QStandardPaths>
#include <QFileDialog>
#include <chrono>
#include <cassert>
#include "FifoTcspc.h"

using namespace std;
 
FifoTcspc::FifoTcspc(QObject* parent) :
	ParametricImageSource(parent)
{
   control_mutex = new QMutex;
}

void FifoTcspc::setLive(bool live_)
{
   assert(!acq_in_progress);

   processor->runContinuously();

   if (live_ && !live)
      startScanning();
   else if (!live_ && live)
      stopScanning();

   live = live_;
}

void FifoTcspc::startAcquisition(bool indeterminate)
{
   assert(!live);

   processor->setFrameIncrementCallback(std::bind(&FifoTcspc::frameIncremented, this));

   if (indeterminate)
   {
      processor->runContinuously();
   }
   else
   {
      processor->setFramesPerImage(frame_accumulation);
      processor->setNumImages(n_images);
      processor->reset();
   }

   acq_in_progress = true;
   acq_idx = 0;
   frame_idx = -1; // first frame marker will set to zero
   startScanning();

   emit acquisitionStatusChanged(acq_in_progress);
}

void FifoTcspc::cancelAcquisition()
{
   if (!acq_in_progress)
      return;

   acq_in_progress = false;
   stopScanning();

   emit acquisitionStatusChanged(acq_in_progress);
}

void FifoTcspc::frameIncremented()
{
   frame_idx++;
   double progress = static_cast<double>(frame_idx) / (n_images * frame_accumulation);
   emit progressUpdated(progress);

   if (frame_idx == (n_images * frame_accumulation))
      QMetaObject::invokeMethod(this, "cancelAcquisition"); // we want to do this on the correct thread
}


void FifoTcspc::startScanning()
{
	configureModule();
	startFIFO();
}

void FifoTcspc::stopScanning()
{
	stopFIFO();
}

cv::Mat FifoTcspc::getImage()
{
	return cur_flimage->getIntensity().clone();
}

cv::Mat FifoTcspc::getImageUnsafe()
{
	return cur_flimage->getIntensity();
}

void FifoTcspc::startFIFO()
{

   cur_flimage->setFrameAccumulation(frame_accumulation);

	scanning = true;

   startModule();

	// Start thread
   processor->start();

}

void FifoTcspc::stopFIFO()
{
   stopModule();
   processor->stop();
   scanning = false;
}



