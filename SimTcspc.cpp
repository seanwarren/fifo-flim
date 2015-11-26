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
   auto read_fcn = std::bind(&SimTcspc::readPackets, this, std::placeholders::_1);
   processor = new EventProcessorPrivate<SimEvent, sim_event>(read_fcn, 10000, 2000);

   n_x = n_px;
   n_y = n_px;

   cur_flimage = new FLIMage(n_px, n_px, 0, n_bits);
   processor->setFLIMage(cur_flimage);
   StartThread();
}


void SimTcspc::init()
{
   FifoTcspc::init();
}



void SimTcspc::startModule()
{

}

SimTcspc::~SimTcspc()
{
}


bool SimTcspc::readPackets(std::vector<sim_event>& buffer)
{
   QThread::usleep(50);

   size_t buffer_length = buffer.size();


   double N = abs((cur_px - (n_px >> 1)) * (cur_py - (n_px >> 1))) * 5 + 500;

   std::poisson_distribution<int> N_dist(N);
   uint8_t channel = 1;

   int idx = 0;
   int n = N_dist(generator);

   if (n > buffer_length - 5)
      n = buffer_length - 5;

   if (cur_px == n_px)
   {
      cur_px = 0;

      if (cur_py >= (n_px - 1))
      {
         cur_py = 0;
         buffer[idx++] = { 0, 0, MARK_FRAME };
         buffer[idx++] = { 0, 0, MARK_LINE };
      }
      else
      {
         cur_py++;
         buffer[idx++] = { 0, 0, MARK_LINE };
      }
   }

   buffer[idx++] = { 0, 0, MARK_PIXEL };

   double tau = static_cast<double>(cur_px) / n_px * 4000 + 500;
   std::exponential_distribution<double> exp_dist(1 / tau);
   std::normal_distribution<double> irf_dist(1000, 100);


   for (int i = 0; i < n; i++)
   {  
      double intpart;
      double t = exp_dist(generator) + irf_dist(generator);
      t = modf(t / T, &intpart);

      uint32_t g = static_cast<int>(t * 256);

      buffer[idx++] = { g, channel, MARK_PHOTON };
   }

   cur_px++;


   if (idx > 0)
   {
      buffer.resize(idx);
      return true;
   }
   else
   {
      return false;
   }

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


