#pragma once

#include "FLIMage.h"
#include "FifoTcspc.h"
#include <random>

struct sim_event
{
   uint32_t macro_time;
   uint32_t micro_time;
   uint8_t channel;
   uint8_t mark;
};

class SimTcspc : public FifoTcspc
{
   Q_OBJECT

public:
   SimTcspc(QObject* parent);
   ~SimTcspc();

   void init();
   size_t readPackets(std::vector<TcspcEvent>& buffer); // return whether any packets were read

   double getSyncRateHz() { return 1e12/T; };
   double getMicroBaseResolutionPs() { return time_resolution_ps; }
   double getMacroBaseResolutionPs() { return T; }
   int getNumChannels() { return n_chan; }
   int getNumTimebins() { return 1 << n_bits; };
   bool usingPixelMarkers() { return false; }

   const QString describe() { return "Simulated TCSPC module"; }

private:

   void startModule();
   void configureModule();

   void setSyncThreshold(float threshold);
   float getSyncThreshold();

   void readRemainingPhotonsFromStream();

   int n_px = 64;
   int n_chan = 4;
   int n_bits = 8;

   int cur_px = n_px;
   int cur_py = n_px;

   double T = 12500;
   double time_resolution_ps;

   int pixel_duration = 10;
   int inter_line_duration = 100;
   int inter_frame_duration = 1000;
   uint64_t cur_macro_time = 0;
   uint64_t macro_time_rollovers = 0;
   std::default_random_engine generator;

protected:

   void addEvent(uint64_t macro_time, uint32_t micro_time, uint8_t channel, uint8_t mark, std::vector<TcspcEvent>& buffer, int& idx)
   {
      uint64_t new_macro_time_rollovers = macro_time / 0xFFFF;

      if ((idx + new_macro_time_rollovers - macro_time_rollovers) > buffer.size())
         buffer.resize(idx * 2);

      while (new_macro_time_rollovers > macro_time_rollovers)
      {
         buffer[idx++] = { 0, 0xF };
         macro_time_rollovers++;
      }

      TcspcEvent evt;
      evt.macro_time = macro_time & 0xFFFF;
      if (mark != 0)
         evt.micro_time = 0xF | (mark << 4);
      else
         evt.micro_time = channel | (micro_time << 4);

      buffer[idx++] = evt;

   }

};
