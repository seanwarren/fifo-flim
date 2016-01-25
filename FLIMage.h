#pragma once

#include <QObject>
#include <QTime>
#include <cv.h>
#include <iostream>
#include <fstream>


#define MarkPhoton  0
#define MarkPixelClock 1
#define MarkLineStartClock 2
#define MarkLineEndClock 4
#define MarkFrameClock 8

class TcspcEvent
{
public:
   
   /*
   const static uchar Photon = 0;
   const static uchar PixelClock = 1;
   const static uchar LineStartClock = 2;
   const static uchar LineEndClock = 4;
   const static uchar FrameClock = 8;
   */
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
   void resize(int n_x, int n_y, int sdt_header = 0);

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
   int line_active = false;
   int frame_idx = -1;
   int frame_accumulation = 1;

   long long line_start_time = 0;
   double line_duration = 1;
   long long frame_duration = 0;

   bool construct_histogram = false;
   int n_bins = 0;
   int bit_shift = 0;

   cv::Mat intensity;
   cv::Mat sum_time;
   cv::Mat mean_arrival_time;

   short sdt_header;
   std::vector<quint16> cur_histogram;
   std::list<std::vector<quint16>> image_histograms;

   std::vector<uint> decay;
   std::vector<uint> next_decay;

   QTime next_refresh;

   const int refresh_time_ms = 1000;

   bool using_pixel_markers = false;
};