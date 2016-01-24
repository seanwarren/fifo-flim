#include "FLIMage.h"

FLIMage::FLIMage(int n_x, int n_y, int sdt_header, int histogram_bits, QObject* parent) :
   QObject(parent)   
{
   histogram_bits = std::min(histogram_bits, 12);
   //construct_histogram = histogram_bits >= 0;

   n_bins = 1 << (histogram_bits);
   bit_shift = 12 - histogram_bits;

   decay.resize(n_bins, 0);
   next_decay.resize(n_bins, 0);

   resize(n_x, n_y, sdt_header);

   next_refresh = QTime::currentTime().addMSecs(refresh_time_ms);
}

void FLIMage::resize(int n_x_, int n_y_, int sdt_header_)
{
   n_x = n_x_;
   n_y = n_y_;
   sdt_header = sdt_header_;

   cur_histogram.resize(n_x * n_y * n_bins);
   intensity = cv::Mat(n_x, n_y, CV_16U, cvScalar(0));
   sum_time = cv::Mat(n_x, n_y, CV_32F, cvScalar(0));
   mean_arrival_time = cv::Mat(n_x, n_y, CV_32F, cvScalar(0));

   if ((n_x == 1) && (n_y == 1))
      cur_x = cur_y = frame_idx = 0;
   else
      cur_x = cur_y = frame_idx = -1;
}

bool FLIMage::isValidPixel()
{
   if ((n_x == 0) && (n_y == 1)) return true;
   return (frame_idx >= 0) && (cur_x >= 0) && (cur_x < n_x) && (cur_y >= 0) && (cur_y < n_x);
}

void FLIMage::addPhotonEvent(const TcspcEvent& p)
{
   //photon_events.push_back(p);
   //PhotonInfo photon(p);

   if (p.mark & TcspcEvent::PixelClock)
   {
      cur_x++;

      if (isValidPixel() && (frame_idx % frame_accumulation == 0))
      {
         intensity.at<quint16>(cur_x, cur_y) = 0;
         sum_time.at<float>(cur_x, cur_y) = 0;
         mean_arrival_time.at<float>(cur_x, cur_y) = 0;
      }
   }
   if (p.mark & TcspcEvent::LineClock)
   {
      cur_x = -1;
      cur_y++;

      if (cur_y >= n_y)
      {
         cur_y = n_y - 1;
         std::cout << "Extra line!\n";
      }
   }
   if (p.mark & TcspcEvent::FrameClock)
   {
      // Check if we're finished an image
      if (construct_histogram && (frame_idx % frame_accumulation == (frame_accumulation-1)))
      {
         image_histograms.push_back(cur_histogram);
         size_t sz = cur_histogram.size();
         for (int i = 0; i < sz; i++)
            cur_histogram[i] = 0;
      }

      frame_idx++;
      cur_x = -1;
      cur_y = -1;
   }
   /*
   cur_x = 0;
   cur_y = 0;
   frame_idx = 0;
   */
   if ((p.mark == TcspcEvent::Photon) && isValidPixel()) // is a photon
   {
      float p_intensity = (++intensity.at<quint16>(cur_x, cur_y));
      float p_sum_time = (sum_time.at<float>(cur_x, cur_y) += p.micro_time);
      mean_arrival_time.at<float>(cur_x, cur_y) = p_sum_time / p_intensity;
      next_decay[p.micro_time]++;

      if (construct_histogram)
      {
         int bin = p.micro_time >> bit_shift;
         cur_histogram[cur_x*n_bins + cur_y*n_x*n_bins + bin]++;
      }
   }

   if (QTime::currentTime() > next_refresh)
   {
      decay = next_decay;
      std::fill(next_decay.begin(), next_decay.end(), 0);
      next_refresh = QTime::currentTime().addMSecs(refresh_time_ms);
      emit decayUpdated();
   }

}

void FLIMage::writeSPC(std::string filename)
{
   std::ofstream os(filename, std::ofstream::out | std::ofstream::binary);
   os << sdt_header;
   //os.write(reinterpret_cast<char*>(photon_events.data()), photon_events.size() * sizeof(Photon));
}
