#pragma once

#include "ParametricImageSource.h"
#include "PacketBuffer.h"
#include <QObject>
#include <QDataStream>
#include <thread>
#include <QFile> 
#include <memory>
#include "EventProcessor.h"
#include "FLIMage.h"

enum Markers
{
   MarkPhoton = 0x0,
   MarkPixel = 0x1,
   MarkLineStart = 0x2,
   MarkLineEnd = 0x4,
   MarkFrame = 0x8
};

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

   void setLive(bool live);

   Q_INVOKABLE void startAcquisition(bool indeterminate = false);
   Q_INVOKABLE void cancelAcquisition();

   void addTcspcEventConsumer(std::shared_ptr<TcspcEventConsumer> consumer) { processor->addTcspcEventConsumer(consumer); }

   void setFrameAccumulation(int frame_accumulation_);
   int getFrameAccumulation() { return frame_accumulation; }

   int getNumImages() { return n_images; };
   void setNumImages(int n_images_) { n_images = n_images_; };

   bool readyForAcquisition() { return !live && !acq_in_progress; };
   bool isLive() { return live; };
   bool acquisitionInProgress() { return acq_in_progress; }

   FlimRates getRates() { return rates; };

   virtual int getNumChannels() = 0;
   virtual int getNumTimebins() = 0;
   virtual double getSyncRateHz() = 0;
   virtual double getMicroBaseResolutionPs() = 0;
   virtual double getMacroBaseResolutionPs() = 0;
   virtual const QString describe() = 0;
   virtual bool usingPixelMarkers() = 0;


   cv::Mat getImage();
   cv::Mat getImageUnsafe();

   std::shared_ptr<FLIMage> getPreviewFLIMage() { return cur_flimage; };

   void frameIncremented();

signals:

   void frameAccumulationChanged(int frame_accumulation);
   void recordingStatusChanged(bool recording);
   void ratesUpdated(FlimRates rates);
   void fifoUsageUpdated(float usage);
   void acquisitionStatusChanged(bool acq_in_progress, bool indeterminate);
   void progressUpdated(double progress);

protected:

   void startFIFO();
   void stopFIFO();

   void startScanning();
   void stopScanning();

   virtual void configureModule() = 0;
   virtual void startModule() {};
   virtual void stopModule() {};
   
   std::shared_ptr<FLIMage> cur_flimage;

   int frame_accumulation = 1;
   int n_images = 1;

   int frame_idx = -1;

   bool scanning = false;
   bool live = false;
   bool acq_in_progress = false;
   int acq_idx = 0;

   uint64_t packets_read = 0;
   uint64_t packets_processed = 0;

   FlimRates rates;

   std::shared_ptr<EventProcessor> processor;
};
