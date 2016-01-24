#pragma once

#include "ParametricImageSource.h"
#include "PacketBuffer.h"
#include <QObject>
#include <QDataStream>
#include <thread>
#include <QFile> 
#include "LZ4Stream.h"
#include "EventProcessor.h"

#define MARK_PHOTON     0x0
#define MARK_PIXEL      0x1
#define MARK_LINE_START 0x2
#define MARK_LINE_END   0x4
#define MARK_FRAME      0x8


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
   }

   void startRecording(const QString& filename);
   void setRecording(bool recording);

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

   virtual void configureModule() = 0;
   virtual void startModule() {};
   virtual void stopModule() {};
   

   FLIMage* cur_flimage;

   int frame_accumulation = 1;
   int n_x = 1;
   int n_y = 1;

   bool scanning = false;

   int packets_read = 0;
   int packets_processed = 0;

   EventProcessor* processor;
};
