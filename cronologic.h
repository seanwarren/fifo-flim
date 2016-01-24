#pragma once

#include "TimeTagger4_interface.h"
#include "ImageSource.h"
#include "FLIMage.h"
#include "FifoTcspc.h"

struct cl_event 
{
   uint32_t hit_fast;
   uint64_t hit_slow;
};

class Cronologic : public FifoTcspc
{
	Q_OBJECT

public:
   Cronologic(QObject* parent);
   ~Cronologic();

	void init();
	void saveSDT(FLIMage& image, const QString& filename);

   size_t readPackets(std::vector<cl_event>& buffer); // return whether any packets were read

private:

   void checkCard();
   void configureCard();

   void startModule();
   void configureModule();
   void stopModule();

   void setSyncThreshold(float threshold);
   float getSyncThreshold();

   void readRemainingPhotonsFromStream();


   timetagger4_device* device;

   double bin_size_ps;
   
   double last_mark_rise_time = -1;
   int n_line = 0;
   int n_pixel = 0;
   bool line_active = false;

   std::vector<uint32_t> t_offset;
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
      micro_time = (readBits(p, 24) + 10) % 25;

      mark = readBits(s, 4);
      macro_time = readBits(s, 60);
   }

   bool isPixelClock() const { return mark & MARK_PIXEL; }
   bool isLineStartClock() const { return mark & MARK_LINE_START; }  
   bool isLineEndClock() const { return mark & MARK_LINE_START; }
   bool isFrameClock() const { return mark & MARK_FRAME; }
   bool isValidPhoton() const { return mark == MARK_PHOTON; }

   uint32_t flags;
   int64_t macro_time;

private:

};