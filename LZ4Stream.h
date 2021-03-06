#pragma once

#include "lz4.h"
#include "lz4hc.h"

#include <vector>
#include <algorithm>
#include <QIODevice>



class LZ4Stream
{
public:
   LZ4Stream(QIODevice* output_device = nullptr) :
      output_device(output_device)
   {
      stream = LZ4_createStreamHC();
      LZ4_resetStreamHC(stream, compression_level);

      cmp_buf_bytes = LZ4_COMPRESSBOUND(max_message_bytes);
      
      cmp_buf.resize(cmp_buf_bytes);
      ring_buf.resize(ring_buffer_bytes);
   }

   ~LZ4Stream()
   {
      LZ4_freeStreamHC(stream);
   }

   size_t getMessageSize()
   {
      return max_message_bytes;
   }
   
   void setDevice(QIODevice* output_device_) { output_device = output_device_; }
   
   void write(const char* data, size_t size)
   {
      if (output_device == nullptr)
      {
         qWarning("No output stream set");
         return;
      }
      
      size_t remaining_bytes = size;
      size_t cur_pos_input = 0;
      
      while (remaining_bytes > 0)
      {
         int msg_bytes = (int) std::min(max_message_bytes, remaining_bytes);
         
         for(size_t i=0; i<msg_bytes; i++)
            ring_buf[cur_pos_ring+i] = data[cur_pos_input + i];
         
         char* ring_ptr = ring_buf.data() + cur_pos_ring;
         
         const int cmp_bytes = LZ4_compress_HC_continue(stream, ring_ptr, cmp_buf.data(), (int)msg_bytes, (int)cmp_buf_bytes);
         output_device->write(cmp_buf.data(), cmp_bytes);
         
         remaining_bytes -= msg_bytes;
         cur_pos_input += msg_bytes;
         cur_pos_ring += msg_bytes;
         
         if (cur_pos_ring + max_message_bytes >= ring_buffer_bytes)
            cur_pos_ring = 0;
      }
   }
   
protected:
   
   LZ4_streamHC_t* stream;
   const size_t max_message_bytes = 16 * 1024;
   const size_t ring_buffer_bytes = 128 * 1024;
   const int compression_level = 9;
   size_t cmp_buf_bytes;
   std::vector<char> cmp_buf;
   std::vector<char> ring_buf;
   
   size_t cur_pos_ring = 0;
   
   QIODevice* output_device = nullptr;
};