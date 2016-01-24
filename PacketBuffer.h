#pragma once

#include <vector>
#include "FLIMage.h"

template<class T> 
class PacketBuffer
{
   typedef
   enum { BufferEmpty, BufferFilling, BufferFilled, BufferProcessing }
   BufferState;


public:
   PacketBuffer(int n_buffers, int buffer_length) :
      n_buffers(n_buffers), buffer_length(buffer_length)
   {
      // Allocate buffers
      buffer = std::vector<std::vector<T>>(n_buffers, std::vector<T>(buffer_length));
      buffer_state = std::vector<BufferState>(n_buffers, BufferEmpty);
      buffer_size = std::vector<size_t>(n_buffers, 0);
   }

   void reset()
   {
      std::fill(buffer_state.begin(), buffer_state.end(), BufferEmpty);
   }

   std::vector<T>& getNextBufferToFill()
   {
      // return an empty vector if there is no valid buffer
      if (buffer_state[fill_idx] != BufferEmpty)
      {
         qWarning("Internal buffer overflowed");
         return empty_buffer;
      }

      // Get buffer and increment index of next buffer
      buffer_state[fill_idx] = BufferFilling;
      buffer_size[fill_idx] = 0;
      return buffer[fill_idx];
   }

   void finishedFillingBuffer(size_t size)
   {
      // State state of buffer to empty and increment pointer
      buffer_size[fill_idx] = size;
      buffer_state[fill_idx] = BufferFilled;
      fill_idx = (fill_idx + 1) % n_buffers;
   }

   void failedToFillBuffer()
   {
      buffer_state[fill_idx] = BufferEmpty;
   }

   std::vector<T>& getNextBufferToProcess()
   {
      // return an empty vector if there is no valid buffer
      if (buffer_state[process_idx] != BufferFilled)
         return empty_buffer;

      // Get buffer and increment index of next buffer
      buffer_state[process_idx] = BufferProcessing;
      return buffer[process_idx];
   }

   size_t getProcessingBufferSize()
   {
      if (buffer_state[process_idx] != BufferFilled)
         return 0;
      return buffer_size[process_idx];
   }

   void finishedProcessingBuffer()
   {
      // Set state of buffer to empty
      buffer_state[process_idx] = BufferEmpty;

      // Resize buffer in case it was returned partially filled
      //buffer[process_idx].resize(buffer_length);

      // Increment index of point to next buffer
      process_idx = (process_idx + 1) % n_buffers;
   }


private:

   int fill_idx = 0;
   int process_idx = 0;

   int n_buffers;
   int buffer_length;

   std::vector<T> empty_buffer;
   std::vector<BufferState> buffer_state;
   std::vector<std::vector<T>> buffer;
   std::vector<size_t> buffer_size;

};
