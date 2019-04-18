#include "SimTcspc.h"

#include <QMessageBox>
#include <QStandardPaths>
#include <QFileDialog>
#include <chrono>
#include <cassert>
#include <cmath>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <iostream>

using namespace std;

const double PI = 3.141592653589793238463;

SimTcspc::SimTcspc(QObject* parent) :
   FifoTcspc(FlimStatus({ "SYNC" }, {"Host Buffer"}), parent)
{
   time_resolution_ps = T / (1 << n_bits);

   markers = Markers{ 0x0, 0x1, 0x2, 0x4, 0x8 };

   loadIntensityImage();
   configureSyncTiming();

   processor = createEventProcessor<SimTcspc>(this, 1000, 2000);
   startThread();
}

void SimTcspc::loadIntensityImage()
{
   try
   {  
#ifdef SUPPRESS_OPENCV_HIGHGUI
      throw (std::runtime_error("Compile in Release or Debug mode"));
#else
      cv::Mat I = cv::imread("simulated_intensity.png", -1);
      I.convertTo(intensity, CV_16U);
#endif
   }
   catch (std::exception e)
   {
      std::cout << "Error loading simulated intensity image" << std::endl;
      std::cout << e.what() << std::endl;
      intensity = cv::Mat(256, 256, CV_16U, cv::Scalar(400));
   }

   int i_px = std::min(intensity.size().width, intensity.size().height);
   n_px = i_px / 2;
   px_offset = i_px / 4;
   cur_px = n_px;
   cur_py = n_px;
}

void SimTcspc::configureSyncTiming()
{
   double line_rate_hz = 1000;
   uint64_t line_flyback_time = 2; // as a fraction of line time
   uint64_t frame_flyback_time = 1; // as a fraction of frame time
   double macro_unit = 1e12 / macro_resolution_ps; // convert s -> macro_resolution_unit

   pixel_duration = macro_unit / (line_rate_hz * n_px);
   inter_line_duration = pixel_duration * n_px * line_flyback_time;
   inter_frame_duration = inter_line_duration;
}

void SimTcspc::init()
{
   FifoTcspc::init();
}



void SimTcspc::startModule()
{
   macro_time_rollovers = 0;
   cur_macro_time = 0;
   cur_px = n_px;
   cur_py = n_px;
   gen_frame = -1;

   flim_status.rates["SYNC"] = 80e6;
}

SimTcspc::~SimTcspc()
{
}

void SimTcspc::addEvent(uint64_t macro_time, uint32_t micro_time, uint8_t channel, uint8_t mark, std::vector<TcspcEvent>& buffer, int& idx)
{
   uint64_t new_macro_time_rollovers = macro_time / (1 << 16);
   uint64_t rollover_max = 0xFFFF;

   while ((idx + new_macro_time_rollovers - macro_time_rollovers) > buffer.size())
      buffer.resize(buffer.size() * 2);

   while (new_macro_time_rollovers > macro_time_rollovers)
   {
      uint16_t r = (uint16_t)std::min(new_macro_time_rollovers - macro_time_rollovers, rollover_max);
      buffer[idx++] = { r, 0xF };
      macro_time_rollovers += r;
   }

   TcspcEvent evt;
   evt.macro_time = macro_time & 0xFFFF;
   if (mark != 0)
      evt.micro_time = 0xF | (mark << 4);
   else
      evt.micro_time = channel | (micro_time << 4);

   buffer[idx++] = evt;

}


size_t SimTcspc::readPackets(std::vector<TcspcEvent>& buffer, double buffer_fill_factor)
{

   size_t buffer_length = buffer.size();

   int idx = 0;

   FlimWarningStatus buffer_status = OK;

   if (buffer_fill_factor > 0.8)
      buffer_status = Critical;
   else if (buffer_fill_factor > 0.5)
      buffer_status = Warning;

   flim_status.warnings["Host Buffer"] = FlimWarning(buffer_status);

   do
   {
      // start new line?
      if (cur_px == n_px)
      {
         cur_px = 0;

         // start new frame?
         if (cur_py >= (n_px - 1))
         {
            cur_py = 0;
            if (gen_frame >= 0)
            {
               addEvent(cur_macro_time, 0, 0, markers.LineEndMarker, buffer, idx);
               cur_macro_time += inter_line_duration + inter_frame_duration;
            }

            addEvent(cur_macro_time, 0, 0, markers.FrameMarker, buffer, idx);
            addEvent(cur_macro_time, 0, 0, markers.LineStartMarker, buffer, idx);
            gen_frame++;
         }
         else
         {
            cur_py++;
            addEvent(cur_macro_time, 0, 0, markers.LineEndMarker, buffer, idx);
            cur_macro_time += inter_line_duration;
            addEvent(cur_macro_time, 0, 0, markers.LineStartMarker, buffer, idx);
         }
      }

      double approx_frame_time = n_px * (n_px * pixel_duration + inter_line_duration);

      double local_scale = (gen_frame > 0) ? 1.0 : 0.0;

      double theta = cur_macro_time / approx_frame_time * displacement_frequency * 2.0 * PI;
      int xsel = cur_px + px_offset + cos(PI / 180.0 * displacement_angle) * displacement_amplitude * local_scale * sin(theta);
      int ysel = cur_py + px_offset + sin(PI / 180.0 * displacement_angle) * displacement_amplitude * local_scale * cos(theta);

      cv::Size sz = intensity.size();
      cv::Rect rect(0, 0, sz.width, sz.height);

      // Average number of photons
      double N = 1;
      if (rect.contains(cv::Point(ysel, xsel)))
         N += 0.04 * intensity.at<uint16_t>(ysel, xsel);

      std::poisson_distribution<int> N_dist(N);
      std::uniform_int_distribution<int> ch_dist(0, n_chan - 1);

      size_t n = N_dist(generator);
      if (n >= buffer_length - idx) n = buffer_length - idx - 1;

      std::vector<std::exponential_distribution<double>> exp_dist;

      double tau = static_cast<double>(cur_px) / n_px * 4000 + 500;
      tau = 3000;
      for (int i = 0; i < n_chan; i++)
         exp_dist.push_back(std::exponential_distribution<double>(1 / (tau + i * 500)));

      std::normal_distribution<double> irf_dist(1000, 100);


      for (int i = 0; i < n; i++)
      {
         double intpart;
         uint8_t channel = ch_dist(generator);
         double t = exp_dist[channel](generator) + irf_dist(generator);
         t = modf(t / T, &intpart);

         uint32_t micro_time = static_cast<int>(t * 256);
         uint64_t macro_time = cur_macro_time + (i * pixel_duration) / n; // space photons evenly across pixel

         addEvent(macro_time, micro_time, channel, markers.PhotonMarker, buffer, idx);
      }

      cur_macro_time += pixel_duration;
      cur_px++;
   } while (idx < 0.8 * buffer.size());

   return idx;
}


void SimTcspc::configureModule()
{

}


