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

   size_t readPackets(std::vector<cl_event>& buffer); // return whether any packets were read

   const QString describe() { return board_name; }
   double getSyncRateHz() { return sync_rate_hz; }
   double getMicroBaseResolutionPs() { return bin_size_ps; }
   double getMacroBaseResolutionPs() { return bin_size_ps; }
   int getNumChannels() { return 3; } // TODO
   int getNumTimebins() { return 25; } // TODO

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
   
   uint64_t last_mark_rise_time = -1;
   int n_line = 0;
   int n_pixel = 0;
   bool line_active = false;
   uint64_t packet_count = 0;
   uint64_t last_update_time = 0;

   std::vector<uint32_t> t_offset;

   double sync_rate_hz = 0;

   FlimRates rates;

   QString board_name;
};


class CLEvent : public TcspcEvent
{
public:


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

private:

};