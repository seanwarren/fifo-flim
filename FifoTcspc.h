#pragma once

#include "ParametricImageSource.h"
#include "PacketBuffer.h"
#include <QObject>
#include <QDataStream>
#include <thread>
#include <QFile> 
#include "LZ4Stream.h"

class FlimRates
{
public:
   FlimRates(float sync = 0.0f, float cfd = 0.0f, float tac = 0.0f, float adc = 0.0f) :
      sync(sync), cfd(cfd), tac(tac), adc(adc)
   {}

   float sync;
   float cfd;
   float tac;
   float adc;
};


class FifoTcspc : public ParametricImageSource
{
   Q_OBJECT

public:
   FifoTcspc(QObject* parent);
   virtual ~FifoTcspc() {};

   virtual void init() {};

   void setScanning(bool scanning);
   void setImageSize(int n);
   void startScanning();
   void stopScanning();

   void setFrameAccumulation(int frame_accumulation_)
   {
      frame_accumulation = frame_accumulation_;
      //cur_flimage->SetFrameAccumulation(frame_accumulation);
   }

   void setRecording(bool recording);
   void startRecording(const QString& filename = "");

   int getFrameAccumulation() { return frame_accumulation; }

   cv::Mat GetImage();
   cv::Mat GetImageUnsafe();

   FLIMage** getPreviewFLIMage() { return &cur_flimage; };

signals:

   void recordingStatusChanged(bool recording);
   void ratesUpdated(FlimRates rates);
   void fifoUsageUpdated(float usage);

protected:

   void startFIFO();
   void stopFIFO();

   virtual void startModule() = 0;
   virtual void configureModule() = 0;
   
   void processorThread();
   virtual void readerThread() = 0;

   virtual void writeFileHeader() = 0;

   virtual void processPhotons() = 0;

   FLIMage* cur_flimage;

   int frame_accumulation = 1;
   int n_x = 1;
   int n_y = 1;

   std::thread processor_thread;
   std::thread reader_thread;

   bool terminate = false;
   bool scanning = false;

   int packets_read = 0;
   int packets_processed = 0;

   // Recording
   //===================================
   QString folder;
   QString file_name;
   QFile file;
   QDataStream data_stream;
   LZ4Stream lz4_stream;
   
   bool recording = false;
   int spc_header = 0;

};
