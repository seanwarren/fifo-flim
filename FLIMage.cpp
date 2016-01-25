#include "FLIMage.h"

FLIMage::FLIMage(int histogram_bits, int n_chan, QObject* parent) :
   QObject(parent),
   n_chan(n_chan)
{
   histogram_bits = std::min(histogram_bits, 12);
   //construct_histogram = histogram_bits >= 0;

   n_bins = 1 << (histogram_bits);
   bit_shift = 12 - histogram_bits;

   decay.resize(n_chan, std::vector<uint>(n_bins, 0));
   next_decay.resize(n_chan, std::vector<uint>(n_bins, 0));

   resize(n_x, n_y);

   next_refresh = QTime::currentTime().addMSecs(refresh_time_ms);
}


void FLIMage::resize(int n_x_, int n_y_)
{
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
             (frame_idx >= 1) &&
             (cur_x >= 0) &&
             (cur_x < n_x) &&
             (cur_y >= 0) &&
             (cur_y < n_y);

}

void FLIMage::addPhotonEvent(const TcspcEvent& p)
{
   //photon_events.push_back(p);
   //PhotonInfo photon(p);

   if (frame_idx >= 0)
   {
      if ((p.mark & TcspcEvent::PixelMarker) && using_pixel_markers)
      {
         cur_x++;

         if (isValidPixel() && (frame_idx % frame_accumulation == 0))
         {
            intensity.at<quint16>(cur_x, cur_y) = 0;
            sum_time.at<float>(cur_x, cur_y) = 0;
            mean_arrival_time.at<float>(cur_x, cur_y) = 0;
         }
      }

      if (p.mark & TcspcEvent::LineStartMarker)
      {
         line_start_time = p.macro_time;
         line_active = true;

         cur_x = -1;
         cur_y++;
      }

      if (p.mark & TcspcEvent::LineEndMarker)
      {
         uint64_t this_line_duration = p.macro_time - line_start_time;
         frame_duration += this_line_duration;

         line_active = false;
         num_end++;
      }
   }
   
   if (p.mark & TcspcEvent::FrameMarker)
   {
      // Check if we're finished an image
      if (construct_histogram && (frame_idx % frame_accumulation == (frame_accumulation-1)))
      {
         image_histograms.push_back(cur_histogram);
         size_t sz = cur_histogram.size();
         for (int i = 0; i < sz; i++)
            cur_histogram[i] = 0;
      }

      if ((using_pixel_markers==false) && (frame_idx > 0))
      {
         int measured_lines = cur_y + 1;
         line_duration = static_cast<double>(frame_duration) / measured_lines;

         if ((measured_lines != n_y) || (measured_lines != n_x))
            resize(measured_lines, measured_lines);

         frame_duration = 0;
      }

      frame_idx++;
      cur_x = -1;
      cur_y = -1;
      num_end = -1;
   }

   if (p.mark == TcspcEvent::Photon)
   {
      if (!using_pixel_markers && (n_x > 1))
      {
         cur_x = ((p.macro_time - line_start_time) * n_x) / line_duration;
      }

      if (isValidPixel()) // is a photon
      {

         int tx = cur_x;
         int ty = cur_y;
         if ((n_x == 1) && (n_y == 1))
         {
            tx = 0;
            ty = 0;
         }

         if (p.channel < n_chan)
         {
            assert(tx < n_x);
            assert(ty < n_y);
            assert(tx >= 0);
            assert(ty >= 0);

            float p_intensity = (++intensity.at<quint16>(tx, ty));
            float p_sum_time = (sum_time.at<float>(tx, ty) += p.micro_time);
            mean_arrival_time.at<float>(tx, ty) = p_sum_time / p_intensity;
            next_decay[p.channel][p.micro_time]++;
         }

         if (construct_histogram)
         {
            int bin = p.micro_time >> bit_shift;
            cur_histogram[cur_x*n_bins + cur_y*n_x*n_bins + bin]++;
         }
      }
   }

   if (QTime::currentTime() > next_refresh)
   {
      decay = next_decay;

      for (int i = 0; i < n_chan; i++)
         std::fill(next_decay[i].begin(), next_decay[i].end(), 0);

      next_refresh = QTime::currentTime().addMSecs(refresh_time_ms);
      emit decayUpdated();
   }

}
