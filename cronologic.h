#pragma once

#include "TimeTagger4_interface.h"
#include "ImageSource.h"
#include "FLIMage.h"
#include "FifoTcspc.h"

#define MARK_PHOTON 0x0
#define MARK_PIXEL  0x1
#define MARK_LINE   0x2
#define MARK_FRAME  0x4

struct cl_event 
{
   uint32_t hit_fast;
   uint64_t hit_slow;
};

inline
QDataStream& operator <<(QDataStream &out, const cl_event &c)
{
   out << c.hit_fast << c.hit_slow;
   return out;
}

inline
QDataStream& operator >>(QDataStream &in, cl_event &c)
{
   in >> c.hit_fast >> c.hit_slow;
   return in;
}

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

   PacketBuffer<cl_event> packet_buffer;


   timetagger4_device* device;

   double bin_size_ps;
   double coarse_factor_ps;

   double last_mark_rising_time = 0;
};


class CLEvent : public TcspcEvent
{
public:

   /*
   Interpret photon data based on B&H manual p485
   */
   CLEvent(cl_event evt)
   {
      uint32_t p = evt.hit_fast;
      uint64_t s = evt.hit_slow;

      channel = readBits(p, 4);
      flags = readBits(p, 4);
      micro_time = readBits(p, 24);

      mark = readBits(s, 4);
      macro_time = readBits(s, 60);
   }

   bool isPixelClock() const { return mark & MARK_PIXEL; } // TODO
   bool isLineClock() const { return mark & MARK_LINE; } // TODO 
   bool isFrameClock() const { return mark & MARK_LINE; } // TODO
   bool isValidPhoton() const { return mark & MARK_PHOTON; }

   uint32_t flags;
   int64_t macro_time;

private:

};