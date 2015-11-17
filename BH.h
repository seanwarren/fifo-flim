#pragma once

#include "FifoTcspc.h"
#include <Spcm_def.h>

#include <string>
#include <iostream>
#include <vector>
#include <algorithm>
#include <QTimer>
#include <QFile>

typedef qint32 Photon;

class BH : public FifoTcspc
{
	Q_OBJECT

public:
	BH(QObject* parent, short module_type);
	~BH();

	void init();

	rate_values readRates();
	void saveSDT(FLIMage& image, const QString& filename);

private:

	void startModule();
   void configureModule();

	void writeFileHeader();
	void setSyncThreshold(float threshold);
	float getSyncThreshold();

	void readerThread();

	bool readPhotons(); // return whether there are more photons to read
	void readRemainingPhotonsFromStream();
   void processPhotons();

	void activateSPCMCards(short module_type, bool force_activation = false);
	float getParameter(short par_id);
	void setParameter(short par_id, float value);

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



class BHEvent : public TcspcEvent
{
public:

   /*
   Interpret photon data based on B&H manual p485
   */
   BHEvent(Photon p)
   {
      macro_time = readBits(p, 12);
      rout = readBits(p, 4);
      micro_time = readBits(p, 12);
      mark = readBits(p, 1);
      gap = readBits(p, 1);
      macro_time_overflow = readBits(p, 1);
      invalid = readBits(p, 1);
   }

   bool isPixelClock() const { return mark && (rout & 0x1); }
   bool isLineClock() const { return mark && (rout & 0x2); }
   bool isFrameClock() const { return mark && (rout & 0x4); }
   bool isValidPhoton() const { return !mark && !invalid; }

   int rout;
   bool mark;
   bool gap;
   bool macro_time_overflow;
   bool invalid;

private:

   int readBits(Photon& p, int n_bits)
   {
      int mask = (1 << n_bits) - 1;
      int value = p & mask;
      p = p >> n_bits;

      return value;
   }
};