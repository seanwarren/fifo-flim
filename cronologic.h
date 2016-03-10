#pragma once

#include "TimeTagger4_interface.h"
#include "ImageSource.h"
#include "FLIMage.h"
#include "FifoTcspc.h"


struct cl_event 
{
   uint32_t hit_fast;
   uint32_t hit_slow;
};

class Cronologic : public FifoTcspc
{
	Q_OBJECT

   enum AcquisitionMode { FLIM, PLIM } ;

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
   int getNumTimebins() { return (acq_mode == FLIM) ? 25 : 255; } // TODO

   void setParameter(const QString& parameter, ParameterType type, QVariant value);
   QVariant getParameter(const QString& parameter, ParameterType type);
   QVariant getParameterLimit(const QString& parameter, ParameterType type, Limit limit);
   QVariant getParameterMinIncrement(const QString& parameter, ParameterType type);
   EnumerationList getEnumerationList(const QString& parameter);
   bool isParameterWritable(const QString& parameter);
   bool isParameterReadOnly(const QString& parameter);

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
   uint64_t last_plim_start_time = 0;
   uint64_t macro_time_rollovers = 0;

   std::vector<uint32_t> t_offset;

   double start_threshold;
   std::vector<double> threshold;
   std::vector<int> time_shift;
   
   double sync_rate_hz = 0;
   bool running = false;

   FlimRates rates;

   AcquisitionMode acq_mode;

   QString board_name;
};


class CLFlimEvent : public TcspcEvent
{
public:


   CLFlimEvent(cl_event evt)
   {
      uint32_t p = evt.hit_fast;
      
      channel = readBits(p, 4);
      mark = readBits(p, 4);
      micro_time = readBits(p, 24);
      
      macro_time = evt.hit_slow;
   }

};

class CLPlimEvent : public TcspcEvent
{
public:


   CLPlimEvent(cl_event evt)
   {
      uint32_t p = evt.hit_fast;
      uint64_t s = evt.hit_slow;

      channel = readBits(p, 4);
      uint8_t flags = readBits(p, 4);
      micro_time = readBits(p, 24);

      mark = readBits(s, 4);
      macro_time = readBits(s, 60);
   }

};