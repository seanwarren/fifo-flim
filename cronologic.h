#pragma once

#include "TimeTagger4_interface.h"
#include "ImageSource.h"
#include "FLIMage.h"
#include "FifoTcspc.h"
#include "PLIMLaserModulator.h"


struct cl_event 
{
   uint16_t hit_fast;
   uint16_t hit_slow;
      // 0:3  [4]  channel     (channel==0xF => event is marker)
      // 4:15 [12] micro_time
};

class Cronologic : public FifoTcspc
{
	Q_OBJECT

   enum AcquisitionMode { FLIM, PLIM } ;

public:
   Cronologic(QObject* parent);
   ~Cronologic();

	void init();

   size_t readPackets(std::vector<TcspcEvent>& buffer); // return whether any packets were read

   const QString describe() { return board_name; }
   double getSyncRateHz() { return sync_rate_hz; }
   double getMicroBaseResolutionPs() { return micro_time_resolution_ps; }
   double getMacroBaseResolutionPs() { return macro_time_resolution_ps; }
   int getNumChannels() { return n_chan; }
   int getNumTimebins() { return n_bins; }
   bool usingPixelMarkers() { return acq_mode == PLIM; };

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
   double macro_time_resolution_ps;
   double micro_time_resolution_ps;
   int n_bins;
   const int n_chan = 3;

   const int macro_downsample = 6;
   int micro_downsample = 0;

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
   
   int sync_divider = 16;
   double sync_rate_hz = 80.2e6;//TODO
   double sync_period_bins = 25;//TODO
   bool running = false;

   PLIMLaserModulator* modulator = nullptr;

   AcquisitionMode acq_mode;

   QString board_name;
};