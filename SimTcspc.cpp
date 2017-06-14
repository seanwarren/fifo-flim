#include "SimTcspc.h"

#include <QMessageBox>
#include <QStandardPaths>
#include <QFileDialog>
#include <chrono>
#include <cassert>
#include <cmath>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

using namespace std;

const double PI = 3.141592653589793238463;

SimTcspc::SimTcspc(QObject* parent) :
FifoTcspc(parent)
{
   time_resolution_ps = T / (1 << n_bits);

   loadIntensityImage();
   configureSyncTiming();

   processor = createEventProcessor<SimTcspc>(this, 1000, 2000);
   cur_flimage = make_shared<FLIMage>(false, time_resolution_ps, macro_resolution_ps, n_bits, n_chan);
   cur_flimage->setImageSize(n_px, n_px);

   processor->addTcspcEventConsumer(cur_flimage);
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
}

SimTcspc::~SimTcspc()
{
}


size_t SimTcspc::readPackets(std::vector<TcspcEvent>& buffer)
{

   size_t buffer_length = buffer.size();

   int idx = 0;

   // start new line?
   if (cur_px == n_px)
   {
      cur_px = 0;

      // start new frame?
      if (cur_py >= (n_px-1))
      {
         cur_py = 0;
         addEvent(cur_macro_time, 0, 0, MarkLineEnd, buffer, idx);
         cur_macro_time += inter_line_duration + inter_frame_duration; 
         addEvent( cur_macro_time, 0, 0, MarkFrame, buffer, idx);
         addEvent(cur_macro_time, 0, 0, MarkLineStart, buffer, idx);
         gen_frame++;
      }
      else
      {
         cur_py++;
         addEvent(cur_macro_time, 0, 0, MarkLineEnd, buffer, idx);
         cur_macro_time += inter_line_duration;
         addEvent(cur_macro_time, 0, 0, MarkLineStart, buffer, idx);
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

      addEvent(macro_time, micro_time, channel, MarkPhoton, buffer, idx);
   }

   cur_macro_time += pixel_duration;
   cur_px++;

   return idx;
}


void SimTcspc::configureModule()
{

}


