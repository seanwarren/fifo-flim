#pragma once

#include "EventProcessor.h"
#include "TcspcEvent.h"
#include "FlimFileWriter.h"
#include <fstream>

#define READ(fs, x) fs.read(reinterpret_cast<char *>(&x), sizeof(x))

class FlimFileReader
{
public:

   FlimFileReader(QString filename) 
   {
      fs = std::ifstream(filename.toStdString(), std::ifstream::binary);
      readHeader();
      fs.seekg(data_position);

      image = std::make_shared<FLIMage>(using_pixel_markers, microtime_resolution, macrotime_resolution, 0, n_chan);
      image->setBidirectional(bi_directional);

      processor = createEventProcessor<FlimFileReader>(this, 10000, 10000);
      processor->addTcspcEventConsumer(image);
      processor->start();
   }

   std::shared_ptr<FLIMage> getFLIMage() { return image; }


   size_t readPackets(std::vector<TcspcEvent>& buffer)
   {
      size_t idx = 0;
      size_t buffer_size = buffer.size();
      while (idx < buffer_size && !fs.eof())
         fs.read(reinterpret_cast<char*>(&buffer[idx++]), sizeof(TcspcEvent));
      return idx;
   }

   void readHeader()
   {
      uint32_t magic;

      READ(fs, magic);

      if (magic != 0xF1F0)
         throw std::runtime_error("Wrong magic string, this is not a valid FFD file");

      READ(fs, version);
      READ(fs, data_position);


      char tag_name[255];
      uint32_t tag_name_length, tag_data_length;
      uint16_t tag_type;

      auto isTag = [&](const char* t) { return strncmp(t, tag_name, tag_name_length - 1) == 0; };

      do
      {
         READ(fs, tag_name_length);

         tag_name_length = std::min(tag_name_length, (uint32_t)255);
         fs.read(tag_name, tag_name_length);
         
         READ(fs, tag_type);
         READ(fs, tag_data_length);

         std::vector<char> data(tag_data_length);
         char* data_ptr = data.data();
         fs.read(data_ptr, tag_data_length);

         if (tag_type == TagDouble)
         {
            double value = *(double*)data_ptr;

            if (isTag("MicrotimeResolutionUnit_ps"))
               microtime_resolution = value;
            if (isTag("MacrotimeResolutionUnit_ps"))
               macrotime_resolution = value;
         }
         else if (tag_type == TagInt64)
         {
            int64_t value = *(int64_t*)data_ptr;

            if (isTag("NumChannels"))
               n_chan = value;
            else if (isTag("NumTimeBins"))
               n_timebins_native = value;
         }
         else if (tag_type == TagUInt64)
         {
            uint64_t value = *(uint64_t*)data_ptr;
         }
         else if (tag_type == TagBool)
         {
            bool value = *(bool*)data_ptr;

            if (isTag("UsingPixelMarkers"))
               using_pixel_markers = value;
            if (isTag("BidirectionalScan"))
               bi_directional = value;
         }
         else if (tag_type == TagDate)
         {
            std::string value;
            value.resize(tag_data_length);
            memcpy(&value[0], data_ptr, tag_data_length);
         }
         else if (tag_type == TagString)
         {
            std::string value;
            value.resize(tag_data_length);
            memcpy(&value[0], data_ptr, tag_data_length);
         }


      } while (tag_type != TagEndHeader);

      data_position = fs.tellg();
   }

protected:

   std::shared_ptr<EventProcessor> processor;
   std::ifstream fs;

   uint32_t version = 1;
   uint32_t data_position = 0;
   uint64_t n_timebins_native = 1;
   uint64_t n_chan = 1;
   double microtime_resolution;
   double macrotime_resolution;
   bool using_pixel_markers = false;
   bool bi_directional = false;

   std::shared_ptr<FLIMage> image;
};