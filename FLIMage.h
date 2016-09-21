#pragma once

#include <QObject>
#include <QTime>
#include <QTimer>
#include <opencv2/core.hpp>
#include <iostream>
#include <fstream>
#include <mutex>
#include <queue>

#include "TcspcEvent.h"
#include "FlimDataSource.h"

class FLIMage : public FlimDataSource, public TcspcEventConsumer
{
   Q_OBJECT
public:

   FLIMage(bool using_pixel_markers, float time_resolution_ps, float macro_resolution_ps, int histogram_bits = 0, int n_chan = 1, QObject* parent = 0);

   cv::Mat getIntensity() 
   {
      std::lock_guard<std::mutex> lk(cv_mutex);
      return intensity.clone(); 
   }

   cv::Mat getMeanArrivalTime() 
   { 
      std::lock_guard<std::mutex> lk(cv_mutex);
      return mean_arrival_time.clone();
   }

   void setFrameAccumulation(int frame_accumulation_) { frame_accumulation = frame_accumulation_; }

   int getNumChannels() { return n_chan; }
   double getTimeResolution() { return time_resolution_ps; }

   std::list<std::vector<quint16>>& getHistogramData() { return image_histograms; }
   std::vector<uint>& getCurrentDecay(int channel) { return decay[channel]; }
   std::vector<double>& getCountRates() { return count_rate; };
   std::vector<double>& getMaxInstantCountRates() { return max_instant_count_rate; };
   double getMaxCountInPixel() { return max_pixel_counts; }

   void addEvent(const TcspcEvent& evt);
   void nextImageStarted() {};

   void eventStreamFinished() { active = false; };
   void eventStreamAboutToStart();

   void refreshDisplay();

   void setImageSize(int n_x, int n_y)
   {
      resize(n_x, n_y);
   }

   void setBidirectional(bool bi_directional_) { bi_directional = bi_directional_; }
   
protected:

   void resize(int n_x, int n_y);
   bool isValidPixel();

   int n_chan = 1;
   int n_x = 1, n_y = 1;
   int cur_x = -1, cur_y = -1;
   int line_active = false;
   int frame_idx = -1;
   int frame_accumulation = 1;
   int num_end = -1;
   uint64_t events = 0;

   uint64_t macro_time_offset = 0;
   uint64_t line_start_time = 0;
   double line_duration = 1;
   double frame_duration = 0;
   bool active = false;

   bool construct_histogram = false;
   uint n_bins = 0;
   int bit_shift = 0;

   cv::Mat intensity;
   cv::Mat sum_time;
   cv::Mat mean_arrival_time;
   float time_resolution_ps;
   float macro_resolution_ps;

   std::vector<quint16> cur_histogram;
   std::list<std::vector<uint16_t>> image_histograms;

   std::vector<std::vector<uint>> decay;
   std::vector<std::vector<uint>> next_decay;

   std::vector<double> count_rate;
   std::vector<double> max_instant_count_rate;
   uint64_t max_pixel_counts = 0;

   std::vector<uint64_t> counts_this_frame;
   std::vector<double> max_rate_this_frame;
   uint64_t last_frame_marker_time;
    
   std::vector<std::queue<double>> recent_photon_times;

   const int refresh_time_ms = 1000;

   bool using_pixel_markers = false;
   bool bi_directional = false;

   int cur_dir = +1;

   std::mutex cv_mutex;
   std::mutex decay_mutex;

   QTimer* timer;
};