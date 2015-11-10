#pragma once

#include <QObject>
#include <cv.h>
#include <iostream>
#include <fstream>

typedef qint32 Photon;

class PhotonInfo
{
public:

   /*
   Interpret photon data based on B&H manual p485
   */
   PhotonInfo(Photon p)
   {
      macro_time = ReadBits(p, 12);
      rout = ReadBits(p, 4);
      adc = ReadBits(p, 12);
      mark = ReadBits(p, 1);
      gap = ReadBits(p, 1);
      macro_time_overflow = ReadBits(p, 1);
      invalid = ReadBits(p, 1);
   }

   bool IsPixelClock() { return mark && (rout & 0x1); }
   bool IsLineClock() { return mark && (rout & 0x2); }
   bool IsFrameClock() { return mark && (rout & 0x4); }
   bool IsValidPhoton() { return !mark && !invalid; }

   int macro_time;
   int rout;
   int adc;
   bool mark;
   bool gap;
   bool macro_time_overflow;
   bool invalid;

private:

   int ReadBits(Photon& p, int n_bits)
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
   void Resize(int n_x, int n_y, int sdt_header);

   cv::Mat& GetIntensity() { return intensity; }
   cv::Mat& GetMeanArrivalTime() { return mean_arrival_time; }

   void AddPhotonEvent(Photon p);
   void SetFrameAccumulation(int frame_accumulation_) { frame_accumulation = frame_accumulation_; }
 
   std::list<std::vector<quint16>>& GetHistogramData() { return image_histograms; }

   void WriteSPC(std::string filename);
   
protected:

   bool IsValidPixel();

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
   std::vector<Photon> photon_events;
   std::vector<quint16> cur_histogram;
   std::list<std::vector<quint16>> image_histograms;
};