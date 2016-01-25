#pragma once

#include <QString>
#include <QFile>
#include <QDataStream>
#include <thread>
#include <functional>
#include "PacketBuffer.h"
#include "LZ4Stream.h"

class EventProcessor
{
public:

   void start();
   void stop();

   void startRecording(const QString& filename = "");
   void stopRecording();
   bool isRecording() { return recording; }

   void setFLIMage(FLIMage* flimage_) { flimage = flimage_; };
   FLIMage* getFLIMage() { return flimage; };

   void writeFileHeader();

   template<typename evt>
   std::vector<evt> getNextBufferToFill();

protected:

   virtual void processorThread() = 0;
   virtual void readerThread() = 0;

   std::thread processor_thread;
   std::thread reader_thread;

   // Recording
   //===================================
   QString folder;
   QString file_name;
   QFile file;
   QDataStream data_stream;
   LZ4Stream lz4_stream;

   bool running = true;
   bool recording = false;
   int spc_header = 0;

   FLIMage* flimage;
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
   while (running)
   {

      size_t n = packet_buffer.getProcessingBufferSize();
      
      if (n == 0) // TODO: use a condition variable here
      {
         QThread::usleep(100);
         continue;
      }
      
      vector<evt> buffer = packet_buffer.getNextBufferToProcess();
      
      
//#pragma omp parallel sections num_threads(2)
      {
         // SECTION 1: Process for live display
         {
            for (int i = 0; i < n; i++)
            {
               auto evt = Event(buffer[i]);
               flimage->addPhotonEvent(evt);
            }
         }
//#pragma omp section
         // SECTION 2: Write to disk
         {
            if (recording)
               lz4_stream.write(reinterpret_cast<char*>(buffer.data()), n*sizeof(evt));
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
      else
      {
         //QThread::msleep(100);
      }
   }
}


template<class Event, typename evt>
void EventProcessorPrivate<Event,evt>::processPhotons()
{

};

