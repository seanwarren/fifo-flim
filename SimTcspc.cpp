#include "SimTcspc.h"

#include <QMessageBox>
#include <QStandardPaths>
#include <QFileDialog>
#include <chrono>
#include <cassert>
#include <cmath>

using namespace std;


SimTcspc::SimTcspc(QObject* parent) :
FifoTcspc(parent)
{
   time_resolution_ps = T / (1 << n_bits);

   processor = createEventProcessor<SimTcspc, TcspcEvent>(this, 1000, 2000);
   cur_flimage = make_shared<FLIMage>(false, time_resolution_ps, 1e6, 8, 4);

   processor->addTcspcEventConsumer(cur_flimage);
   StartThread();
}


void SimTcspc::init()
{
   FifoTcspc::init();
}



void SimTcspc::startModule()
{
   cur_px = n_px;
   cur_py = n_px;

}

SimTcspc::~SimTcspc()
{
}


size_t SimTcspc::readPackets(std::vector<TcspcEvent>& buffer)
{
   QThread::usleep(5);

   size_t buffer_length = buffer.size();

   double N = abs((cur_px - (n_px >> 1)) * (cur_py - (n_px >> 1))) * 5 + 500;

   std::poisson_distribution<int> N_dist(N);
   std::uniform_int_distribution<int> ch_dist(0, n_chan-1);

   uint8_t channel = 0;

   int idx = 0;
   size_t n = N_dist(generator);

   if (n > buffer_length - 5)
      n = buffer_length - 5;

   if (cur_px == n_px)
   {
      cur_px = 0;

      if (cur_py >= (n_px-1))
      {
         cur_py = 0;
         addEvent(cur_macro_time, 0, 0, MARK_LINE_END, buffer, idx);
         cur_macro_time += inter_frame_duration; 
         addEvent( cur_macro_time, 0, 0, MARK_FRAME, buffer, idx);
         addEvent(cur_macro_time, 0, 0, MARK_LINE_START, buffer, idx);
      }
      else
      {
         cur_py++;
         addEvent(cur_macro_time, 0, 0, MARK_LINE_END, buffer, idx);
         cur_macro_time += inter_line_duration;
         addEvent(cur_macro_time, 0, 0, MARK_LINE_START, buffer, idx);
      }
   }

   //buffer[idx++] = { 0, 0, MARK_PIXEL };

   double tau = static_cast<double>(cur_px) / n_px * 4000 + 500;
   std::vector<std::exponential_distribution<double>> exp_dist;
   
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
      uint64_t macro_time = cur_macro_time + (i * pixel_duration) / n;

      addEvent(macro_time, micro_time, channel, MARK_PHOTON , buffer, idx);

   }

   cur_macro_time += pixel_duration;
   cur_px++;

   return idx;
}


//============================================================
// Configure card for FIFO mode
//
// This function is mostly lifted from the use_spcm.c 
// example file
//============================================================
void SimTcspc::configureModule()
{

}


