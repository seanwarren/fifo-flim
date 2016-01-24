
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
}

void FifoTcspc::setScanning(bool scanning_)
{
	if (scanning_ & !scanning)
		startScanning();
	else if (!scanning_ & scanning)
		stopScanning();
}

void FifoTcspc::startScanning()
{
	configureModule();
	startFIFO();
}

void FifoTcspc::stopScanning()
{
	stopFIFO();

	setRecording(false);

}

cv::Mat FifoTcspc::GetImage()
{
	return cur_flimage->getIntensity().clone();
}

cv::Mat FifoTcspc::GetImageUnsafe()
{
	return cur_flimage->getIntensity();
}

void FifoTcspc::setImageSize(int n)
{
	if (!scanning)
	{
		n_x = n;
		n_y = n;
	}
}


void FifoTcspc::setRecording(bool recording_)
{
	if (processor->isRecording() != recording_)
	{
		if (recording_)
			processor->startRecording();
		else
         processor->stopRecording();
	}

   emit recordingStatusChanged(processor->isRecording());
}


void FifoTcspc::startRecording(const QString& filename)
{
   processor->startRecording(filename);
}


void FifoTcspc::startFIFO()
{
	cur_flimage->resize(n_x, n_y, 0);
	cur_flimage->setFrameAccumulation(frame_accumulation);

	scanning = true;

	// Start thread
   processor->start();

   startModule();
}

void FifoTcspc::stopFIFO()
{

   processor->stop();

   stopModule();

	scanning = false;
}



