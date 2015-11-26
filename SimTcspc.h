#pragma once

#include "FLIMage.h"
#include "FifoTcspc.h"
#include <random>

#define MARK_PHOTON 0x0
#define MARK_PIXEL  0x1
#define MARK_LINE   0x2
#define MARK_FRAME  0x4

struct sim_event
{
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
   void saveSDT(FLIMage& image, const QString& filename);
   bool readPackets(std::vector<sim_event>& buffer); // return whether any packets were read

private:

   void startModule();
   void configureModule();

   void setSyncThreshold(float threshold);
   float getSyncThreshold();

   void readRemainingPhotonsFromStream();

   int n_px = 64;
   int n_bits = 8;

   int cur_px = n_px;
   int cur_py = n_px;

   double T = 12500;

   std::default_random_engine generator;
};


class SimEvent : public TcspcEvent
{
public:

   SimEvent(const sim_event evt)
   {
      channel = evt.channel;
      micro_time = evt.micro_time;
      mark = evt.mark;
   }

   bool isPixelClock() const { return mark & MARK_PIXEL; }
   bool isLineClock() const { return mark & MARK_LINE; }
   bool isFrameClock() const { return mark & MARK_FRAME; }
   bool isValidPhoton() const { return mark == MARK_PHOTON; }
};