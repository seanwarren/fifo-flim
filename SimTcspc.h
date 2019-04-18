#pragma once

#include "FifoTcspc.h"
#include "AbstractEventReader.h"
#include <random>


class SimTcspc : public FifoTcspc
{
   Q_OBJECT

public:
   SimTcspc(QObject* parent);
   ~SimTcspc();

   void init();
   size_t readPackets(std::vector<TcspcEvent>& buffer, double buffer_fill_factor); // return whether any packets were read

   double getSyncRateHz() { return 1e12/T; };
   
   TcspcAcquisitionParameters getAcquisitionParameters()
   {
      return TcspcAcquisitionParameters{
         time_resolution_ps,
         macro_resolution_ps,
         1 << n_bits,
         n_chan,
      };
   }
   
   bool usingPixelMarkers() { return false; }

   const QString describe() { return "Simulated TCSPC module"; }

   void setParameter(const QString& parameter, ParameterType type, QVariant value) 
   {
      if (parameter == "DisplacementFrequency")
         displacement_frequency = value.toDouble();
      if (parameter == "DisplacementAmplitude")
         displacement_amplitude = value.toDouble();
      if (parameter == "DisplacementAngle")
         displacement_angle = value.toDouble();
   };

   QVariant getParameter(const QString& parameter, ParameterType type) 
   {
      if (parameter == "DisplacementFrequency")
         return displacement_frequency;
      if (parameter == "DisplacementAmplitude")
         return displacement_amplitude;
      if (parameter == "DisplacementAngle")
         return displacement_angle;

      return QVariant();
   };

   QVariant getParameterLimit(const QString& parameter, ParameterType type, Limit limit)
   {
      if (limit == Limit::Min)
      {
         return 0;
      }
      else
      {
         if (parameter == "DisplacementAngle")
            return 360;
         else
            return 1e3;
      }

      return QVariant();
   };


private:

   Markers markers;

   void startModule();
   void configureModule();

   void setSyncThreshold(float threshold);
   float getSyncThreshold();

   void loadIntensityImage();
   void configureSyncTiming();

   void readRemainingPhotonsFromStream();

   int n_px = 64;
   int n_chan = 1;
   int n_bits = 8;

   int gen_frame = 0;

   int cur_px;
   int cur_py;

   double T = 12500;
   double time_resolution_ps;
   double macro_resolution_ps = 1e3; 

   int pixel_duration;
   int inter_line_duration;
   int inter_frame_duration;
   uint64_t cur_macro_time = 0;
   uint64_t macro_time_rollovers = 0;
   std::default_random_engine generator;
   std::chrono::system_clock::time_point last_frame_time;

   int px_offset;

   double displacement_frequency = 100;
   double displacement_amplitude = 0;
   double displacement_angle = 0;

protected:

   void addEvent(uint64_t macro_time, uint32_t micro_time, uint8_t channel, uint8_t mark, std::vector<TcspcEvent>& buffer, int& idx);

   cv::Mat intensity;

};
