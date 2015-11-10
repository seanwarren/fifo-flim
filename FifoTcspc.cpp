#include "BH.h"
#include <QMessageBox>
#include <QStandardPaths>
#include <QFileDialog>
#include <chrono>
#include <cassert>

using namespace std;
 
FifoTcspc(QObject* parent) :
	ImageSource(parent),
	photon_buffer(PhotonBuffer(1000, 10000))
{

}

void FifoTcspc::SetScanning(bool scanning_)
{
	if (scanning_ & !scanning)
		StartScanning();
	else if (!scanning_ & scanning)
		StopScanning();
}

void FifoTcspc::StartScanning()
{
	ConfigureFIFO();
	StartFIFO();
}

void FifoTcspc::StopScanning()
{
	StopFIFO();

	SetRecording(false);

}

cv::Mat FifoTcspc::GetImage()
{
	return cur_flimage->GetIntensity().clone();
}

cv::Mat FifoTcspc::GetImageUnsafe()
{
	return cur_flimage->GetIntensity();
}

void FifoTcspc::SetImageSize(int n)
{
	if (!scanning)
	{
		n_x = n;
		n_y = n;
	}
}

void BH::WriteFileHeader()
{
	// Write header
	quint32 spc_header;
	quint32 magic_number = 0xF1F0;
	quint32 header_size = 4;
	quint32 format_version = 1;

	CHECK(SPC_get_fifo_init_vars(act_mod, NULL, NULL, NULL, &spc_header));

	data_stream << magic_number << header_size << format_version << n_x << n_y << spc_header;
}

void FifoTcspc::SetRecording(bool recording_)
{
	if (recording != recording_)
	{
		if (recording_)
		{
			StartRecording();
		}
		else
		{
			recording = false;
			data_stream.setDevice(nullptr);
			file.close();
		}
	}

	emit RecordingStatusChanged(recording);
}

void FifoTcspc::StartRecording(const QString& specified_file_name)
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
			WriteFileHeader();

		recording = true;
	}
}


void BH::StartFIFO()
{
	StartModule();

	cur_flimage->Resize(n_x, n_y, spc_header);
	cur_flimage->SetFrameAccumulation(frame_accumulation);

	if (recording)
		WriteFileHeader();

	terminate = false;
	scanning = true;

	// Start thread
	reader_thread = std::thread(&BH::ReaderThread, this);
	processor_thread = std::thread(&BH::ProcessorThread, this);

}

void FifoTcspc::StopFIFO()
{
	terminate = true;

	if (reader_thread.joinable())
		reader_thread.join();
	if (processor_thread.joinable())
		processor_thread.join();

	scanning = false;
}


void FifoTcspc::ProcessorThread()
{
	while (!terminate)
		ProcessPhotons();
}

void FifoTcspc::ProcessPhotons()
{
	vector<Photon>& buffer = photon_buffer.GetNextBufferToProcess();

	if (buffer.empty()) // TODO: use a condition variable here
		return;

	for (auto& p : buffer)
		cur_flimage->AddPhotonEvent(p);

	photon_buffer.FinishedProcessingBuffer();
}

