#include "EventProcessor.h"


void EventProcessor::start()
{
   running = true;
   reader_thread = std::thread(&EventProcessor::readerThread, this);
   processor_thread = std::thread(&EventProcessor::processorThread, this);
}

void EventProcessor::stop()
{
   running = false;
   if (reader_thread.joinable())
      reader_thread.join();

   if (processor_thread.joinable())
      processor_thread.join();

}