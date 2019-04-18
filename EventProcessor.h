#pragma once

#include <QString>
#include <future>
#include <functional>
#include <memory>
#include <vector>
#include "PacketBuffer.h"
#include "TcspcEvent.h"

class EventProcessor
{
   typedef std::function<size_t(std::vector<TcspcEvent>& buffer, double buffer_fill_factor)> ReaderFcn;


public:

   EventProcessor(ReaderFcn reader_fcn, int n_buffers, int buffer_length) :
      packet_buffer(n_buffers, buffer_length),
      reader_fcn(reader_fcn)
   {

   }

   void start();
   void stop();

   template<typename evt>
   std::vector<evt> getNextBufferToFill();

   void addTcspcEventConsumer(std::shared_ptr<TcspcEventConsumer> consumer)
   {
      consumers.push_back(consumer);
   };

   void setFrameIncrementCallback(std::function<void(void)> frame_increment_callback_) { frame_increment_callback = frame_increment_callback_; };

   void setNumImages(int n_images_) { n_images = n_images_; run_continuously = false; }
   void setFramesPerImage(int frames_per_image_) { frames_per_image = frames_per_image_; }
  
   void runContinuously() { run_continuously = true; }

   bool isRunningContinuously() { return run_continuously; }

   void reset()
   { 
      frame_idx = -1;
      image_idx = -1; // goes to zero on first frame marker
   }

protected:

   void processorThread();
   void readerThread();

   PacketBuffer<TcspcEvent> packet_buffer;
   ReaderFcn reader_fcn;

   std::future<void> processor_thread;
   std::future<void> reader_thread;

   std::vector<std::shared_ptr<TcspcEventConsumer>> consumers;
   std::function<void(void)> frame_increment_callback;

   bool running;
   int frames_per_image = 1;
   int frame_idx = -1;
   int image_idx = -1;
   int n_images = 1;
   bool run_continuously = true;

   template<class Provider>
   friend std::shared_ptr<EventProcessor> createEventProcessor(Provider* obj, int n_buffers, int buffer_length);
};


template<class Provider>
std::shared_ptr<EventProcessor> createEventProcessor(Provider* obj, int n_buffers, int buffer_length)
{
   auto read_fcn = std::bind(&Provider::readPackets, obj, std::placeholders::_1, std::placeholders::_2);
   return std::make_shared<EventProcessor>(read_fcn, n_buffers, buffer_length);
}
