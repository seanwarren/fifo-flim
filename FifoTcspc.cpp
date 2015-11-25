
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
	if (recording != recording_)
	{
		if (recording_)
		{
			startRecording();
		}
		else
		{
			recording = false;
			data_stream.setDevice(nullptr);
			file.close();
		}
	}

	emit recordingStatusChanged(recording);
}

void FifoTcspc::startRecording(const QString& specified_file_name)
{
	QString file_name = specified_file_name;

	if (recording)
		return;

	if (file_name == "")
	{
		QString folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
		folder.append("/FLIM data.spc");
		file_name = QFileDialog::getSaveFileName(nullptr, "Choose a file name", folder, "SPC file (*.spc)");
	}

	if (!file_name.isEmpty())
	{

		file.setFileName(file_name);
		file.open(QIODevice::WriteOnly);
		data_stream.setDevice(&file);
		data_stream.setByteOrder(QDataStream::LittleEndian);

		if (scanning)
			writeFileHeader();

		recording = true;
	}
}


void FifoTcspc::startFIFO()
{
	startModule();

	cur_flimage->resize(n_x, n_y, spc_header);
	cur_flimage->setFrameAccumulation(frame_accumulation);

	if (recording)
		writeFileHeader();

	terminate = false;
	scanning = true;

	// Start thread
   reader_thread = std::thread(&FifoTcspc::readerThread, this);
   processor_thread = std::thread(&FifoTcspc::processorThread, this);

}

void FifoTcspc::stopFIFO()
{
	terminate = true;

	if (reader_thread.joinable())
		reader_thread.join();
	if (processor_thread.joinable())
		processor_thread.join();

	scanning = false;
}


void FifoTcspc::processorThread()
{
	while (!terminate)
		processPhotons();
}


