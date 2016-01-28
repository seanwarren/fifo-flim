#pragma once

#pragma once

#include "lz4.h"
#include "lz4hc.h"

#include <vector>
#include <algorithm>
#include <QIODevice>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include "PacketBuffer.h"

class LZ4ThreadedStream
{
public:
   LZ4ThreadedStream(QIODevice* output_device = nullptr) :
      output_device(output_device),
      buffer(1000, max_message_bytes)
   {
      stream = LZ4_createStreamHC();
      LZ4_resetStreamHC(stream, compression_level);

      cmp_buf_bytes = LZ4_COMPRESSBOUND(max_message_bytes);
      cmp_buf.resize(cmp_buf_bytes);

      cur_buffer = buffer.getNextBufferToFill();

      output_thread = std::thread(&LZ4ThreadedStream::outputThread, this);
   }

   ~LZ4ThreadedStream()
   {
      buffer.setStreamFinished();
      output_thread.join();
      LZ4_freeStreamHC(stream);
   }

   void close()
   {
      buffer.setStreamFinished();
      closed = true;
      output_thread.join();
   }

   void setDevice(QIODevice* output_device_) { output_device = output_device_; }

   void outputThread()
   {
      while (true)
      {
         buffer.waitForNextBuffer();
         if (buffer.streamFinished())
            return;

         size_t n = buffer.getProcessingBufferSize();

         std::vector<char> b = buffer.getNextBufferToProcess();

         const int cmp_bytes = LZ4_compress_HC_continue(stream, b.data(), cmp_buf.data(), (int)n, (int)cmp_buf_bytes);
         buffer.finishedProcessingBuffer();

         output_device->write(reinterpret_cast<const char*>(&cmp_bytes), sizeof(cmp_bytes));
         output_device->write(cmp_buf.data(), cmp_bytes);

         total_in += n;
         total_out += cmp_bytes;

         //double ratio = ((double)total_out) / total_in;
         //std::cout << ratio << "\n";

      }
   }

   void write(const char* data, size_t size)
   {
      if (closed)
      {
         qWarning("Device closed");
         return;
      }
      if (output_device == nullptr)
      {
         qWarning("No output stream set");
         return;
      }

      if (cur_buffer == nullptr)
      {
         cur_buffer = buffer.getNextBufferToFill();
         if (cur_buffer == nullptr)
         {
            qWarning("Warning, bytes may be lost due to buffer overflow");
            return;
         }
      }

      int idx = 0;
      while (size - idx > 0)
      {
         char* cb = cur_buffer->data(); // work around for slow STL checks in debug mode

         size_t num_to_copy = std::min(size - idx, max_message_bytes);
         for (int i = 0; i < num_to_copy; i++)
            cb[bytes_in_buffer++] = data[idx++];

         if (bytes_in_buffer == max_message_bytes)
         {
            buffer.finishedFillingBuffer(bytes_in_buffer);
            cur_buffer = buffer.getNextBufferToFill();
            bytes_in_buffer = 0;
         }
      }
   }

protected:

   size_t total_in = 0;
   size_t total_out = 0;

   LZ4_streamHC_t* stream;
   const size_t max_message_bytes = 16*1024;
   const int compression_level = 4;
   size_t cmp_buf_bytes;
   std::vector<char> cmp_buf;

   std::thread output_thread;
   
   QIODevice* output_device = nullptr;

   bool closed = false;

   PacketBuffer<char> buffer;
   std::vector<char>* cur_buffer;
   size_t bytes_in_buffer = 0;
};