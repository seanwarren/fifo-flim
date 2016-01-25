#pragma once

#include <QObject>
#include <QTime>
#include <cv.h>
#include <iostream>
#include <fstream>

class TcspcEvent
{
public:
   
   enum Mark {
      Photon = 0,
      PixelMarker = 1,
      LineStartMarker = 2,
      LineEndMarker = 4,
      FrameMarker = 8
   };

   uint64_t macro_time;
   uint32_t micro_time;
   uchar channel;
   uchar mark;

protected:

   template<class T>
   T readBits(T& p, long n_bits)
   {
      T mask = (1LL << n_bits) - 1;
      T value = p & mask;
      p = p >> n_bits;

      return value;
   }

};

class FLIMage : public QObject
{
   Q_OBJECT
public:

   FLIMage(int histogram_bits = 0, int n_chan = 1, QObject* parent = 0);

   cv::Mat getIntensity() { return intensity; }
   cv::Mat getMeanArrivalTime() { return mean_arrival_time; }
   int getNumChannels() { return n_chan; }

   void addPhotonEvent(const TcspcEvent& p);
   void setFrameAccumulation(int frame_accumulation_) { frame_accumulation = frame_accumulation_; }
 
   std::list<std::vector<quint16>>& getHistogramData() { return image_histograms; }
   
   std::vector<uint>& getCurrentDecay(int channel) { return decay[channel]; }

signals:
   void decayUpdated();

protected:

   void init(int histogram_bits);
   void resize(int n_x, int n_y);
   bool isValidPixel();

   int n_chan = 1;
   int n_x = 1, n_y = 1;
   int cur_x = -1, cur_y = -1;
   int line_active = false;
   int frame_idx = -1;
   int frame_accumulation = 1;
   int num_end = -1;

   uint64_t line_start_time = 0;
   double line_duration = 1;
   double frame_duration = 0;

   bool construct_histogram = false;
   int n_bins = 0;
   int bit_shift = 0;

   cv::Mat intensity;
   cv::Mat sum_time;
   cv::Mat mean_arrival_time;

   short sdt_header;
   std::vector<quint16> cur_histogram;
   std::list<std::vector<quint16>> image_histograms;

   std::vector<std::vector<uint>> decay;
   std::vector<std::vector<uint>> next_decay;

   QTime next_refresh;

   const int refresh_time_ms = 1000;

   bool using_pixel_markers = false;

};