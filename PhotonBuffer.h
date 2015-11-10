#pragma once

#include <vector>
#include "FLIMage.h"

class PhotonBuffer
{
   typedef
   enum { BufferEmpty, BufferFilling, BufferFilled, BufferProcessing }
   BufferState;


public:
   PhotonBuffer(int n_buffers, int buffer_length) :
      n_buffers(n_buffers), buffer_length(buffer_length)
   {
      // Allocate buffers
      buffer = std::vector<std::vector<Photon>>(n_buffers, std::vector<Photon>(buffer_length));
      buffer_state = std::vector<BufferState>(n_buffers, BufferEmpty);
   }

   void Reset()
   {
      // Reset buffer contents
      int fill_idx = 0;
      int process_idx = 0;

      std::fill(buffer_state.begin(), buffer_state.end(), BufferEmpty);
   }

   std::vector<Photon>& GetNextBufferToFill()
   {
      // return an empty vector if there is no valid buffer
      if (buffer_state[fill_idx] != BufferEmpty)
         throw std::exception("Buffer overflowed!");

      // Get buffer and increment index of next buffer
      buffer_state[fill_idx] = BufferFilling;
      return buffer[fill_idx];
   }

   void FinishedFillingBuffer()
   {
      // State state of buffer to empty and increment pointer
      buffer_state[fill_idx] = BufferFilled;
      fill_idx = (fill_idx + 1) % n_buffers;
   }

   void FailedToFillBuffer()
   {
      buffer_state[fill_idx] = BufferEmpty;
   }

   std::vector<Photon>& GetNextBufferToProcess()
   {
      // return an empty vector if there is no valid buffer
      if (buffer_state[process_idx] != BufferFilled)
         return empty_buffer;

      // Get buffer and increment index of next buffer
      buffer_state[process_idx] = BufferProcessing;
      return buffer[process_idx];
   }

   void FinishedProcessingBuffer()
   {
      // Set state of buffer to empty
      buffer_state[process_idx] = BufferEmpty;

      // Resize buffer in case it was returned partially filled
      buffer[process_idx].resize(buffer_length);

      // Increment index of point to next buffer
      process_idx = (process_idx + 1) % n_buffers;
   }


private:

   int fill_idx = 0;
   int process_idx = 0;

   int n_buffers;
   int buffer_length;

   std::vector<Photon> empty_buffer;
   std::vector<BufferState> buffer_state;
   std::vector<std::vector<Photon>> buffer;

};
