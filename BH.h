#pragma once

#include "ImageSource.h"
#include "FLIMage.h"
#include "PhotonBuffer.h"
#include <Spcm_def.h>


#include <string>
#include <iostream>
#include <vector>
#include <thread>
#include <algorithm>
#include <QObject>
#include <QTimer>
#include <QFile>
#include <QDataStream>

class FifoTcspc : public ImageSource
{
	Q_OBJECT

public:
	FifoTcspc(QObject* parent);
	virtual ~FifoTcspc();

	void Init();

	void SetScanning(bool scanning);
	void SetImageSize(int n);
	void StartScanning();
	void StopScanning();

	void SetFrameAccumulation(int frame_accumulation_)
	{
		frame_accumulation = frame_accumulation_;
		cur_flimage->SetFrameAccumulation(frame_accumulation);
	}

	void SetRecording(bool recording);
	void StartRecording(const QString& filename = "");

	int GetFrameAccumulation() { return frame_accumulation; }

	cv::Mat GetImage();
	cv::Mat GetImageUnsafe();

signals:

   void RecordingStatusChanged(bool recording);

protected:

	void StartFIFO();
	void StopFIFO();

	virtual void StartModule() = 0;
	virtual void ConfigureModule() = 0;
	virtual void ReadFIFO() = 0;

	void ProcessorThread();
   virtual void ReaderThread() = 0;

   virtual void WriteFileHeader() = 0;

   void ProcessPhotons();

	FLIMage* cur_flimage;

	int frame_accumulation = 1;
	int n_x = 1;
	int n_y = 1;

	std::thread processor_thread;
	std::thread reader_thread;

	bool terminate = false;
	bool scanning = false;

	PhotonBuffer photon_buffer;
	int photons_read = 0;
	int photons_processed = 0;

	// Recording
	//===================================
	QString folder;
	QString file_name;
	QFile file;
	QDataStream data_stream;
	bool recording = false;
   int spc_header = 0;

};

class BH : public FifoTcspc
{
	Q_OBJECT

public:
	BH(QObject* parent, short module_type);
	~BH();

	void Init();

	rate_values ReadRates();
	void SaveSDT(FLIMage& image, const QString& filename);

signals:

	void RecordingStatusChanged(bool recording);
   void RatesUpdated(rate_values rates);
   void FifoUsageUpdated(float usage);

private:

	void StartModule();
   void ConfigureModule();

	void WriteFileHeader();
	void SetSyncThreshold(float threshold);
	float GetSyncThreshold();

	void ReaderThread();

	bool ReadPhotons(); // return whether there are more photons to read
	void ReadRemainingPhotonsFromStream();

	void ActivateSPCMCards(short module_type, bool force_activation = false);
	float GetParameter(short par_id);
	void SetParameter(short par_id, float value);

	// SPC card configuration parameters
	//====================================
	short module_type;
	int total_no_of_spc = 0;
	int no_of_active_spc = 0;
	int mod_active[MAX_NO_OF_SPC];
	short act_mod;
	SPCdata spc_dat;

	// FIFO parameters
	//====================================
	unsigned short fpga_version;
	unsigned long max_ph_to_read;
	unsigned long max_words_in_buf;
	unsigned long words_per_phot;
	short fifo_type;
	unsigned long fifo_size;
	short fifo_stopt_possible;

	QTimer* rate_timer;
};