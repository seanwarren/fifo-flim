#pragma once

#include <QString>
#include <thread>
#include <functional>
#include <memory>
#include <vector>
#include "PacketBuffer.h"

class EventProcessor
{
public:

   void start();
   void stop();

   template<typename evt>
   std::vector<evt> getNextBufferToFill();

   void addTcspcEventConsumer(std::shared_ptr<TcspcEventConsumer> consumer)
   {
      consumers.push_back(consumer);
   };

protected:

   virtual void processorThread() = 0;
   virtual void readerThread() = 0;

   std::thread processor_thread;
   std::thread reader_thread;

   std::vector<std::shared_ptr<TcspcEventConsumer>> consumers;

   bool running;
};

template<class Event, typename evt>
class EventProcessorPrivate : public EventProcessor
{
   typedef std::function<size_t(std::vector<evt>& buffer)> ReaderFcn;

   EventProcessorPrivate(ReaderFcn reader_fcn, int n_buffers, int buffer_length) :
      packet_buffer(n_buffers, buffer_length),
      reader_fcn(reader_fcn)
   {

   }

protected:

   void processPhotons();
   void processorThread();
   void readerThread();

   PacketBuffer<evt> packet_buffer;
   ReaderFcn reader_fcn;

   template<class Provider, class Event, typename evt>
   friend EventProcessor* createEventProcessor(Provider* obj, int n_buffers, int buffer_length);
};

template<class Provider, class Event, typename evt>
EventProcessor* createEventProcessor(Provider* obj, int n_buffers, int buffer_length)
{
   auto read_fcn = std::bind(&Provider::readPackets, obj, std::placeholders::_1);
   return new EventProcessorPrivate<Event, evt>(read_fcn, n_buffers, buffer_length);
}



template<class Event, typename evt>
void EventProcessorPrivate<Event, evt>::processorThread()
{
   size_t n_consumers = consumers.size();

   while (running)
   {
      size_t n = packet_buffer.getProcessingBufferSize();
      
      if (n == 0)
      {
		 packet_buffer.waitForNextBuffer();
         continue;
      }
      
      vector<evt> buffer = packet_buffer.getNextBufferToProcess();

      for (int c = 0; c < n_consumers; c++)
      {
         const auto& consumer = consumers[c];
         if (consumer->isProcessingEvents())
            for (int i = 0; i < n; i++)
            {
               TcspcEvent evt = Event(buffer[i]);
               consumer->addEvent(evt);
            }
      }

      packet_buffer.finishedProcessingBuffer();

   }
}

template<class Event, typename evt>
void EventProcessorPrivate<Event, evt>::readerThread()
{
   while (running)
   {
      std::vector<evt>& buffer = packet_buffer.getNextBufferToFill();

      if (!buffer.empty()) // failed to get buffer
      {

         size_t n_read = reader_fcn(buffer);

         if (n_read > 0)
            packet_buffer.finishedFillingBuffer(n_read);
         else
            packet_buffer.failedToFillBuffer();
      }
   }
}


template<class Event, typename evt>
void EventProcessorPrivate<Event,evt>::processPhotons()
{

};

