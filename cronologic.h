#pragma once

#include "TimeTagger4_interface.h"
#include "ImageSource.h"
#include "FLIMage.h"
#include "FifoTcspc.h"


struct cl_event 
{
   uint16_t hit_fast;
   uint16_t hit_slow;
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
   double getMicroBaseResolutionPs() { return bin_size_ps; }
   double getMacroBaseResolutionPs() { return macro_time_resolution_ps; }
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
   double macro_time_resolution_ps;
   
   const int macro_downsample = 7;

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