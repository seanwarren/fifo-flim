#pragma once

#include "FifoTcspc.h"
#include <Spcm_def.h>

#include <string>
#include <iostream>
#include <vector>
#include <algorithm>
#include <QTimer>
#include <QFile>

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

   PacketBuffer<Photon> packet_buffer;
};