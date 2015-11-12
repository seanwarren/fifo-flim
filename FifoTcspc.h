#pragma once

#include "ImageSource.h"
#include "PacketBuffer.h"
#include <QObject>
#include <QDataStream>
#include <thread>
#include <QFile> 

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

class FifoTcspc : public ImageSource
{
   Q_OBJECT

public:
   FifoTcspc(QObject* parent);
   virtual ~FifoTcspc() {};

   virtual void Init() {};

   void SetScanning(bool scanning);
   void SetImageSize(int n);
   void StartScanning();
   void StopScanning();

   void SetFrameAccumulation(int frame_accumulation_)
   {
      frame_accumulation = frame_accumulation_;
      cur_flimage->SetFrameAccumulation(frame_accumulation);
   }

   void SetRecording(bool recording);
   void StartRecording(const QString& filename = "");

   int GetFrameAccumulation() { return frame_accumulation; }

   cv::Mat GetImage();
   cv::Mat GetImageUnsafe();

signals:

   void RecordingStatusChanged(bool recording);
   void RatesUpdated(FlimRates rates);
   void FifoUsageUpdated(float usage);

protected:

   void StartFIFO();
   void StopFIFO();

   virtual void StartModule() = 0;
   virtual void ConfigureModule() = 0;
   
   void ProcessorThread();
   virtual void ReaderThread() = 0;

   virtual void WriteFileHeader() = 0;

   virtual void ProcessPhotons() = 0;

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
   bool recording = false;
   int spc_header = 0;

};
