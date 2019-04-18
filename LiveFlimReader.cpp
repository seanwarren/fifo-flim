#include "LiveFlimReader.h"
#include <limits>


LiveEventReader::LiveEventReader() : 
   AbstractEventReader(sizeof(TcspcEvent))
{
   block.resize(block_size * packet_size);
   writer_block_it = block.begin();
}

void LiveEventReader::addEvent(const TcspcEvent& evt)
{
   std::copy_n((char*)&evt, packet_size, writer_block_it);
   writer_block_it += packet_size;
   if (writer_block_it == block.end())
   {
      //std::unique_lock<std::mutex> lk(m);
      data.push_back(block);
      cv.notify_one();
      writer_block_it = block.begin();
   }
};

double LiveEventReader::getProgress() 
{ 
   return 0; 
}; 

bool LiveEventReader::hasMoreData() 
{ 
   return has_more_data; 
};

void LiveEventReader::setEventStreamAboutToStart() 
{ 
   clear();
   has_more_data = true;
}

void LiveEventReader::setEventStreamFinished()
{ 
   has_more_data = false; 
   cv.notify_one();
}


std::tuple<class FifoEvent, uint64_t> LiveEventReader::getRawEvent()
{
   TcspcEvent evt = getPacket<TcspcEvent>();

   FifoEvent e;
   uint64_t macro_time_offset = 0;

   e.micro_time = evt.microTime();
   e.macro_time = evt.macro_time;
   e.channel = evt.channel();
   e.mark = evt.isMark() ? evt.mark() : 0;
   e.valid = !evt.isMacroTimeRollover();

   if (evt.isMacroTimeRollover())
      macro_time_offset += ((uint64_t)0xFFFF) * evt.macro_time;

   return std::tuple<FifoEvent, uint64_t>(e, macro_time_offset);
}

LiveFlimReader::LiveFlimReader(const TcspcAcquisitionParameters& params, QObject* parent) :
   params(params)
{

   macro_time_resolution_ps = params.macro_resolution_ps;

   markers.ImageMarker = 16;
   markers.FrameMarker = 8;
   markers.LineEndMarker = 4;
   markers.LineStartMarker = 2;
   markers.PixelMarker = 0x0; // no pixel marker

   live_event_reader = std::make_shared<LiveEventReader>();
   event_reader = live_event_reader;

   setNumChannels(params.n_channels);
   initaliseTimepoints(params.n_timebins, params.time_resolution_ps);
}

void LiveFlimReader::addEvent(const TcspcEvent& evt)
{
   live_event_reader->addEvent(evt);
}

void LiveFlimReader::setImageSize(int n_x_, int n_y_)
{
   n_x = n_x_;
   n_y = n_y_;
}


/*
FLIMage::FLIMage(bool using_pixel_markers, float time_resolution_ps, float macro_resolution_ps, int histogram_bits, int n_chan, QObject* parent) :
   FlimDataSource(parent),
   using_pixel_markers(using_pixel_markers),
   time_resolution_ps(time_resolution_ps),
   macro_resolution_ps(macro_resolution_ps),
   n_chan(n_chan)
{
   histogram_bits = std::min(histogram_bits, 12);
   //construct_histogram = histogram_bits >= 0;

   n_bins = 1 << (histogram_bits);
   bit_shift = 12 - histogram_bits;

   decay.resize(n_chan, std::vector<uint>(n_bins, 0));
   next_decay.resize(n_chan, std::vector<uint>(n_bins, 0));
   count_rate.resize(n_chan);
   counts_this_frame.resize(n_chan);
   max_instant_count_rate.resize(n_chan);
   max_rate_this_frame.resize(n_chan);
   recent_photon_times.resize(n_chan);

   resize(n_x, n_y);

   timer = new QTimer(this);
   timer->setInterval(refresh_time_ms);
   timer->setSingleShot(false);
   connect(timer, &QTimer::timeout, this, &FLIMage::refreshDisplay);
   timer->start();
}

void FLIMage::eventStreamAboutToStart()
{
   active = true;

   macro_time_offset = 0;
   frame_duration = 0;
   line_start_time = 0;
   line_duration = -1;
   frame_duration = 0;
   cur_x = cur_y = -1;
   frame_idx = -1;

   std::fill(count_rate.begin(), count_rate.end(), 0);
   std::fill(max_instant_count_rate.begin(), max_instant_count_rate.end(), 0);
   std::fill(counts_this_frame.begin(), counts_this_frame.end(), 0);
   std::fill(max_instant_count_rate.begin(), max_instant_count_rate.end(), 0);

   std::lock_guard<std::mutex> lk(cv_mutex);

   intensity = cv::Mat(n_x, n_y, CV_16U, cvScalar(0));
   sum_time = cv::Mat(n_x, n_y, CV_32F, cvScalar(0));
   mean_arrival_time = cv::Mat(n_x, n_y, CV_32F, cvScalar(0));

};


void FLIMage::resize(int n_x_, int n_y_)
{
   std::lock_guard<std::mutex> lk(cv_mutex);

   n_x = n_x_;
   n_y = n_y_;

   cur_histogram.resize(n_x * n_y * n_bins);
   intensity = cv::Mat(n_x, n_y, CV_16U, cvScalar(0));
   sum_time = cv::Mat(n_x, n_y, CV_32F, cvScalar(0));
   mean_arrival_time = cv::Mat(n_x, n_y, CV_32F, cvScalar(0));

   if ((n_x == 1) && (n_y == 1))
      cur_x = cur_y = frame_idx = 0;
   else
      cur_x = cur_y = -1;

   std::cout << "Resize: " << n_x << " x " << n_y << "\n";
}

bool FLIMage::isValidPixel()
{
   if ((n_x == 1) && (n_y == 1)) return true;
   if (using_pixel_markers)
      return (line_active)    && 
             (frame_idx >= 0) && 
             (cur_x >= 0)     && 
             (cur_x < n_x)   && 
             (cur_y >= 0)     && 
             (cur_y < n_y);
   else
      return (line_active) &&
             (frame_idx >= 0) &&
             (cur_x >= 0) &&
             (cur_x < n_x) &&
             (cur_y >= 0) &&
             (cur_y < n_y);

}

void FLIMage::addEvent(const TcspcEvent& p)
{
   if (p.isMacroTimeRollover()) // rollover
   {
      macro_time_offset += ((uint64_t)0xFFFF) * p.macro_time;
      return;
   }

   uint64_t macro_time = p.macro_time + macro_time_offset;
   
   if (p.isMark())
   {
      uint8_t mark = p.mark();

      if (frame_idx >= 0)
      {
         if ((mark & TcspcEvent::PixelMarker) && using_pixel_markers)
         {
            cur_x++;

            
            //if (isValidPixel() && (frame_idx % frame_accumulation == 0))
            //{
            //   intensity.at<quint16>(cur_x, cur_y) = 0;
            //   sum_time.at<float>(cur_x, cur_y) = 0;
            //   mean_arrival_time.at<float>(cur_x, cur_y) = 0;
            //}
         }

         if (mark & TcspcEvent::LineStartMarker)
         {
            line_start_time = macro_time;
            line_active = true;

            cur_x = 0; // bit of a bodge - first pixel clock arrives too soon on current PLIM setup. should be -1

            if (bidirectional)
               cur_dir *= -1;
            cur_y++;
         }

         if (mark & TcspcEvent::LineEndMarker)
         {
            uint64_t this_line_duration = macro_time - line_start_time;
            frame_duration += this_line_duration;

            if (line_duration == -1)
               line_duration = this_line_duration;

            line_active = false;
            num_end++;
         }
      }

      if (mark & TcspcEvent::FrameMarker)
      {
         // Check if we're finished an image
         if (construct_histogram && (frame_idx % frame_accumulation == (frame_accumulation - 1)))
         {
            image_histograms.push_back(cur_histogram);
            size_t sz = cur_histogram.size();
            for (int i = 0; i < sz; i++)
               cur_histogram[i] = 0;
         }

         if (frame_idx > 0)
         {
            int measured_lines = cur_y + 1;
            line_duration = static_cast<double>(frame_duration) / measured_lines;

            if (measured_lines != n_y && !using_pixel_markers)
               resize(measured_lines, measured_lines);
         }

         // Calculate photon rates
         if (frame_idx >= 0)
         {
            double total_frame_time = n_y * line_duration * macro_resolution_ps * 1e-12;
            for (int i = 0; i < n_chan; i++)
            {
               count_rate[i] = counts_this_frame[i] / total_frame_time;
               counts_this_frame[i] = 0;

               max_instant_count_rate[i] = max_rate_this_frame[i];
               max_rate_this_frame[i] = 0;
            }
            last_frame_marker_time = macro_time;
            emit countRatesUpdated();
         }

         frame_idx++;
         cur_x = -1;
         cur_y = -1;
         cur_dir = 1;
         num_end = 0;
         frame_duration = 0;
      }
   }
   else // is photon
   {
      if (!using_pixel_markers && (n_x > 1))
      {
         cur_x = (macro_time - line_start_time) * (n_x / line_duration);
         if (cur_dir == -1)
            cur_x = n_x - 1 - cur_x;
      }

      if (isValidPixel()) // is at valid coordinate
      {

         int tx = cur_x;
         int ty = cur_y;
         if ((n_x == 1) && (n_y == 1))
         {
            tx = 0;
            ty = 0;
         }

         uint8_t channel = p.channel();

         if (channel < n_chan)
         {
            assert(tx < n_x);
            assert(ty < n_y);
            assert(tx >= 0);
            assert(ty >= 0);

            uint16_t micro_time = p.microTime();

            float p_intensity = (++intensity.at<quint16>(tx, ty));
            float p_sum_time = (sum_time.at<float>(tx, ty) += micro_time);
            mean_arrival_time.at<float>(tx, ty) = p_sum_time / p_intensity * time_resolution_ps;
            
            if (micro_time < n_bins)
               next_decay[channel][micro_time]++;

            counts_this_frame[channel]++;

            double cur_time = macro_time * macro_resolution_ps + p.micro_time * time_resolution_ps;
            recent_photon_times[channel].push(cur_time);

            // Calculate rate based on arrival times of most recent photons
            if (recent_photon_times[channel].size() > 100)
            {
               double last_time = recent_photon_times[channel].front();
               
               size_t n_photons = recent_photon_times[channel].size();
               double rate = 1e12 * n_photons / (cur_time - last_time);

               if (rate > max_rate_this_frame[channel])
                  max_rate_this_frame[channel] = rate;

               recent_photon_times[channel].pop();
            }

         }
         
         //if (construct_histogram)
         //{
         //   int bin = p.micro_time >> bit_shift;
         //   cur_histogram[cur_x*n_bins + cur_y*n_x*n_bins + bin]++;
        //}
      }
   }

}

void FLIMage::refreshDisplay()
{
   if (active)
   {
      std::lock_guard<std::mutex> lk(decay_mutex);

      decay = next_decay;

      for (int i = 0; i < n_chan; i++)
         std::fill(next_decay[i].begin(), next_decay[i].end(), 0);
      
      double max_dbl;
      cv::minMaxLoc(intensity, nullptr, &max_dbl);
      max_pixel_counts = (uint64_t) max_dbl;
      
      emit decayUpdated();
   }
}

*/