#include "SimTcspc.h"

#include <QMessageBox>
#include <QStandardPaths>
#include <QFileDialog>
#include <chrono>
#include <cassert>
#include <cmath>

using namespace std;


SimTcspc::SimTcspc(QObject* parent) :
FifoTcspc(parent),
packet_buffer(PacketBuffer<sim_event>(10000, 2000))
{
   n_x = n_px;
   n_y = n_px;

   cur_flimage = new FLIMage(n_px, n_px, 0, n_bits);
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

void SimTcspc::writeFileHeader()
{
   // Write header
   quint32 spc_header = 0;
   quint32 magic_number = 0xF1F0;
   quint32 header_size = 4;
   quint32 format_version = 1;

   data_stream << magic_number << header_size << format_version << n_x << n_y << spc_header;
}


void SimTcspc::processPhotons()
{
   vector<sim_event>& buffer = packet_buffer.getNextBufferToProcess();

   if (buffer.empty()) // TODO: use a condition variable here
      return;
   
   for (auto& p : buffer)
   {
      auto evt = SimEvent(p);
      cur_flimage->addPhotonEvent(evt);

   }

   if (recording)
      for (auto& p : buffer)
         data_stream << p;

   packet_buffer.finishedProcessingBuffer();
}



void SimTcspc::readerThread()
{
   while (!terminate)
   {
      readPackets();
   }

   setRecording(false);
}

bool SimTcspc::readPackets()
{
   QThread::usleep(10);

   vector<sim_event>& buffer = packet_buffer.getNextBufferToFill();

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
      packet_buffer.finishedFillingBuffer();
   }
   else
   {
      packet_buffer.failedToFillBuffer();
   }

   return false;
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

