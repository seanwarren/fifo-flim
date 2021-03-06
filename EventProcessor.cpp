#include "EventProcessor.h"
#include <iostream>

void EventProcessor::start()
{
   packet_buffer.reset();

   for (auto& consumer : consumers)
      consumer->eventStreamAboutToStart();

   running = true;
   reader_thread = std::async(std::launch::async, &EventProcessor::readerThread, this);
   processor_thread = std::async(std::launch::async, &EventProcessor::processorThread, this);
}


void EventProcessor::processorThread()
{
   int ridx = 0;
   size_t n_consumers = consumers.size();
   while (running)
   {
      packet_buffer.waitForNextBuffer();
      size_t n = packet_buffer.getProcessingBufferSize();
      auto& buffer = packet_buffer.getNextBufferToProcess();
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
               if (evt.isMark() && (evt.mark() & TcspcEvent::FrameMarker))
               {
                  frame_increment++;

                  if (run_continuously)
                  {
                     if (frame_idx == -1)
                        consumer->nextImageStarted();
                  }
                  else
                  {
                     if ((frame_idx + frame_increment) % frames_per_image == 0)
                     {
                        evt.addMark(TcspcEvent::Mark::ImageMarker);
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

               }
               consumer->addEvent(evt);
            }
         }
      }

      frame_idx += frame_increment;
      image_idx += image_increment;

      if (frame_increment_callback != nullptr)
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
         size_t n_read = reader_fcn(*buffer, packet_buffer.fillFactor());

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


   reader_thread.get();

   packet_buffer.setStreamFinished();

   processor_thread.get();

   for (auto& consumer : consumers)
      consumer->eventStreamFinished();

   packet_buffer.reset();
}
