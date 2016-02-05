#include "EventProcessor.h"


void EventProcessor::start()
{
   for (auto& consumer : consumers)
      consumer->eventStreamAboutToStart();

   running = true;
   reader_thread = std::thread(&EventProcessor::readerThread, this);
   processor_thread = std::thread(&EventProcessor::processorThread, this);
}

