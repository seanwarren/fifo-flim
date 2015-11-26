#pragma once

#include <QObject>
#include <QTime>
#include <cv.h>
#include <iostream>
#include <fstream>


class TcspcEvent
{
public:

   static const uchar Photon     = 0;
   static const uchar PixelClock = 1;
   static const uchar LineClock  = 2;
   static const uchar FrameClock = 4;

   uint macro_time;
   uint micro_time;
   uint channel;
   uchar mark;

protected:

   template<class T>
   int readBits(T& p, int n_bits)
   {
      int mask = (1 << n_bits) - 1;
      int value = p & mask;
      p = p >> n_bits;

      return value;
   }
};



class FLIMage : public QObject
{
   Q_OBJECT
public:

   FLIMage(int n_x, int n_y, int sdt_header, int histogram_bits = 0, QObject* parent = 0);
   void resize(int n_x, int n_y, int sdt_header);

   cv::Mat& getIntensity() { return intensity; }
   cv::Mat& getMeanArrivalTime() { return mean_arrival_time; }

   void addPhotonEvent(const TcspcEvent& p);
   void setFrameAccumulation(int frame_accumulation_) { frame_accumulation = frame_accumulation_; }
 
   std::list<std::vector<quint16>>& getHistogramData() { return image_histograms; }

   void writeSPC(std::string filename);
   
   std::vector<uint>& getCurrentDecay() { return decay; }

signals:
   void decayUpdated();

protected:

   bool isValidPixel();

   int n_x = 1, n_y = 1;
   int cur_x = -1, cur_y = -1;
   int frame_idx = -1;
   int frame_accumulation = 1;

   bool construct_histogram = false;
   int n_bins = 0;
   int bit_shift = 0;

   cv::Mat intensity;
   cv::Mat sum_time;
   cv::Mat mean_arrival_time;

   short sdt_header;
//   std::vector<Photon> photon_events;
   std::vector<quint16> cur_histogram;
   std::list<std::vector<quint16>> image_histograms;

   std::vector<uint> decay;
   std::vector<uint> next_decay;

   QTime next_refresh;

   const int refresh_time_ms = 200;
};