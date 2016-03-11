#pragma once

#include <QString>
#include <thread>
#include <functional>
#include <memory>
#include <vector>
#include "PacketBuffer.h"
#include "TcspcEvent.h"

// TODO: This class needs to be simplified now - have removed requirement for templates

class EventProcessor
{
public:

   void start();
   virtual void stop() = 0;

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

   void reset()
   { 
      frame_idx = -1;
      image_idx = -1; // goes to zero on first frame marker
   }

protected:

   virtual void processorThread() = 0;
   virtual void readerThread() = 0;

   std::thread processor_thread;
   std::thread reader_thread;

   std::vector<std::shared_ptr<TcspcEventConsumer>> consumers;
   std::function<void(void)> frame_increment_callback;

   bool running;
   int frames_per_image = 1;
   int frame_idx = -1;
   int image_idx = -1;
   int n_images = 1;
   bool run_continuously = true;
};

template<class Event>
class EventProcessorPrivate : public EventProcessor
{
   typedef std::function<size_t(std::vector<Event>& buffer)> ReaderFcn;

   EventProcessorPrivate(ReaderFcn reader_fcn, int n_buffers, int buffer_length) :
      packet_buffer(n_buffers, buffer_length),
      reader_fcn(reader_fcn)
   {

   }

   void stop();

protected:

   void processorThread();
   void readerThread();

   PacketBuffer<Event> packet_buffer;
   ReaderFcn reader_fcn;

   template<class Provider, class Event>
   friend EventProcessor* createEventProcessor(Provider* obj, int n_buffers, int buffer_length);
};

template<class Provider, class Event>
EventProcessor* createEventProcessor(Provider* obj, int n_buffers, int buffer_length)
{
   auto read_fcn = std::bind(&Provider::readPackets, obj, std::placeholders::_1);
   return new EventProcessorPrivate<Event>(read_fcn, n_buffers, buffer_length);
}



template<class Event>
void EventProcessorPrivate<Event>::processorThread()
{
   size_t n_consumers = consumers.size();
   while (running)
   {

		packet_buffer.waitForNextBuffer();
      size_t n = packet_buffer.getProcessingBufferSize();
      vector<Event> buffer = packet_buffer.getNextBufferToProcess();
      int frame_increment = 0;
      int image_increment = 0;

       for (int c = 0; c < n_consumers; c++)
      {
         frame_increment = 0;
         image_increment = 0;
         const auto& consumer = consumers[c];
         if (consumer->isProcessingEvents())
            for (int i = 0; i < n; i++)
            {
               TcspcEvent evt = buffer[i];
               if (evt.isMark() && (evt.mark() & TcspcEvent::FrameMarker) && !run_continuously)
               {
                  frame_increment++;
                  if ((frame_idx + frame_increment) % frames_per_image == 0)
                  {
                     image_increment++;
                     if (image_idx + image_increment == n_images)
                     {
                        consumer->imageSequenceFinished();
                        break; // don't send any more events
                     }
                     else
                     {
                        consumer->nextImageStarted();
                     }
                  }
               }
               consumer->addEvent(evt);
            }
      }

      frame_idx += frame_increment;
      image_idx += image_increment;

      for (int i = 0; i < frame_increment; i++)
         frame_increment_callback();

      packet_buffer.finishedProcessingBuffer();

   }
}

template<class Event>
void EventProcessorPrivate<Event>::readerThread()
{
   while (running)
   {
      std::vector<Event>* buffer = packet_buffer.getNextBufferToFill();

      if (buffer != nullptr) // failed to get buffer
      {
         size_t n_read = reader_fcn(*buffer);

         if (n_read > 0)
            packet_buffer.finishedFillingBuffer(n_read);
         else
            packet_buffer.failedToFillBuffer();
      }
   }
}

template<class Event>
void EventProcessorPrivate<Event>::stop()
{
   running = false;


   if (reader_thread.joinable())
      reader_thread.join();

   packet_buffer.setStreamFinished();

   if (processor_thread.joinable())
      processor_thread.join();

   for (auto& consumer : consumers)
      consumer->eventStreamFinished();

  packet_buffer.reset();
}
