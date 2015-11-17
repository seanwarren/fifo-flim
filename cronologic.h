#pragma once

#include "TimeTagger4_interface.h"
#include "ImageSource.h"
#include "FLIMage.h"
#include "FifoTcspc.h"

struct cl_event 
{
   uint32_t hit;
   uint64_t timestamp;
};

class Cronologic : public FifoTcspc
{
	Q_OBJECT

public:
   Cronologic(QObject* parent);
   ~Cronologic();

	void init();
	void saveSDT(FLIMage& image, const QString& filename);

private:

   void checkCard();
   void configureCard();

   void startModule();
   void configureModule();

   void writeFileHeader();
   void setSyncThreshold(float threshold);
   float getSyncThreshold();

   void readerThread();
   void processPhotons();

   bool readPackets(); // return whether there are more photons to read
   void readRemainingPhotonsFromStream();

   timetagger4_device* device;

   PacketBuffer<cl_event> packet_buffer;
};


class CLEvent : public TcspcEvent
{
public:

   /*
   Interpret photon data based on B&H manual p485
   */
   CLEvent(cl_event evt)
   {
      uint32_t p = evt.hit;

      channel = readBits(p, 4);
      flags = readBits(p, 4);
      micro_time = readBits(p, 24);
   }

   bool isPixelClock() const { return false; } // TODO
   bool isLineClock() const { return false; } // TODO 
   bool isFrameClock() const { return false; } // TODO
   bool isValidPhoton() const { return channel < 4; }

   int flags;

private:

   int readBits(uint32_t& p, int n_bits)
   {
      int mask = (1 << n_bits) - 1;
      int value = p & mask;
      p = p >> n_bits;

      return value;
   }
};