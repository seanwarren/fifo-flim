#include "EventProcessor.h"
#include <iostream>
#include <windows.h>

void EventProcessor::start()
{
   packet_buffer.reset();

   for (auto& consumer : consumers)
      consumer->eventStreamAboutToStart();

   running = true;
   reader_thread = std::thread(&EventProcessor::readerThread, this);
   processor_thread = std::thread(&EventProcessor::processorThread, this);

   //SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
   //SetThreadPriority(reader_thread.native_handle(), THREAD_PRIORITY_HIGHEST);
   //SetThreadPriority(processor_thread.native_handle(), THREAD_PRIORITY_ABOVE_NORMAL);
}


void EventProcessor::processorThread()
{
   int ridx = 0;
   size_t n_consumers = consumers.size();
   while (running)
   {
//      if ((ridx++) % 1000 == 0)
//         std::cout << "Fill factor: " << packet_buffer.fillFactor()  << "\n";

      packet_buffer.waitForNextBuffer();
      size_t n = packet_buffer.getProcessingBufferSize();
      std::vector<TcspcEvent> buffer = packet_buffer.getNextBufferToProcess();
      int frame_increment = 0;
      int image_increment = 0;

      for (int c = 0; c < n_consumers; c++)
      {
         frame_increment = 0;
         image_increment = 0;
         const auto& consumer = consumers[c];
         if (consumer->isProcessingEvents())
         {
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
      }

      frame_idx += frame_increment;
      image_idx += image_increment;

      for (int i = 0; i < frame_increment; i++)
         frame_increment_callback();

      packet_buffer.finishedProcessingBuffer();

   }
}

void EventProcessor::readerThread()
{
   while (running)
   {
      std::vector<TcspcEvent>* buffer = packet_buffer.getNextBufferToFill();

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

void EventProcessor::stop()
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
