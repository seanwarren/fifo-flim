#pragma once

#include <QObject>
#include <cv.h>
#include <iostream>
#include <fstream>


class TcspcEvent
{
public:
   virtual bool isPixelClock() const = 0;
   virtual bool isLineClock() const = 0;
   virtual bool isFrameClock() const = 0;
   virtual bool isValidPhoton() const = 0;

   uint macro_time;
   uint micro_time;
   uint channel;
   uint mark;

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
};