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
   size_t readPackets(std::vector<sim_event>& buffer); // return whether any packets were read

   virtual double getSyncRateHz() { return 1e12/T; };
   virtual double getMicroBaseResolutionPs() { return time_resolution_ps; }
   virtual double getMacroBaseResolutionPs() { return T; }
   virtual int getNumChannels() { return n_chan; }
   virtual int getNumTimebins() { return 1 << n_bits; };

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

   void addEvent(uint64_t macro_time, uint32_t micro_time, uint8_t channel, uint8_t mark, std::vector<sim_event>& buffer, int& idx)
   {
      uint64_t new_macro_time_rollovers = macro_time / 0xFFFF;

      if ((idx + new_macro_time_rollovers - macro_time_rollovers) > buffer.size())
         buffer.resize(idx * 2);

      while (new_macro_time_rollovers > macro_time_rollovers)
      {
         buffer[idx++] = { 0, 0, 0xF, 0xF };
         macro_time_rollovers++;
      }

      buffer[idx++] = { macro_time & 0xFFFF, micro_time, channel, mark };
   }

};


class SimEvent : public TcspcEvent
{
public:

   SimEvent(const sim_event evt)
   {
      macro_time = evt.macro_time;
      if (evt.mark != 0)
         micro_time = 0xF | (evt.mark << 4);
      else
         micro_time = evt.channel | (evt.micro_time << 4);
   }
};