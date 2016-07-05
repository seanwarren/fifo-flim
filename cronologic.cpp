#include "Cronologic.h"

#include <QMessageBox>
#include <QStandardPaths>
#include <QFileDialog>
#include <QInputDialog>
#include <chrono>
#include <cassert>
#include <algorithm>

using namespace std;

void CHECK(int err)
{
   if (err != 0)
   {
      std::string msg = "Cronologic Error Code: " + std::to_string(err);
      throw std::runtime_error(msg);
   }
}

Cronologic::Cronologic(QObject* parent) :
FifoTcspc(parent)
{
   QString mode = QInputDialog::getItem(nullptr, "Choose Imaging Mode", "Imaging Mode", { "FLIM", "PLIM" }, 0, false);

   if (mode == "PLIM")
      acq_mode = PLIM;
   else
      acq_mode = FLIM;

   if (acq_mode == PLIM)
      modulator = new PLIMLaserModulator(this);

   processor = createEventProcessor<Cronologic>(this, 10000, 10000);
   
   threshold = { -60.0, -60.0, -60.0 };
   time_shift = { 0, 0, 0 };

   checkCard();
   configureCard();
   
   if (acq_mode == FLIM)
   {
      micro_downsample = 0;
      n_bins = 25;
   }
   else // PLIM
   {
      micro_downsample = 11;
      n_bins = 512;
   }

   int histogram_bits = ceil(log2(n_bins));

   micro_time_resolution_ps = bin_size_ps * (1 << micro_downsample);
   macro_time_resolution_ps = bin_size_ps * (1 << macro_downsample);

   cur_flimage = std::make_shared<FLIMage>(acq_mode == PLIM, micro_time_resolution_ps, macro_time_resolution_ps, histogram_bits, n_chan);

   processor->addTcspcEventConsumer(cur_flimage);

   startThread();

   int a = sizeof(TcspcEvent);
   a = 1;
}

enum ParameterClass { StartThreshold, Threshold, TimeShift, NumPixelsPLIM, Unknown };

ParameterClass getParameterClass(const QString& parameter, int& channel)
{
   channel = 0;

   if (parameter.startsWith("Threshold_"))
   {
      channel = parameter.mid(10).toInt();
      if ((channel >= 0) && (channel <= 3))
         return Threshold;
      else
         return Unknown;
   }
   else if (parameter.startsWith("Time_Shift_"))
   {
      channel = parameter.mid(11).toInt();
      if ((channel >= 0) && (channel <= 3))
         return TimeShift;
      else
         return Unknown;
   }
   else if (parameter == "Start_Threshold")
   {
      return StartThreshold;
   }
   else if (parameter == "Num_Pixel_PLIM")
   {
      return NumPixelsPLIM;
   }
   
   return Unknown;
}

void Cronologic::setParameter(const QString& parameter, ParameterType type, QVariant value) 
{
   int channel;
   ParameterClass c = getParameterClass(parameter, channel);

   if (c == TimeShift)
      time_shift[channel] = value.toInt();

   if (running) return;

   if (c == Threshold)
      threshold[channel] = value.toDouble();
   else if (c == StartThreshold)
      start_threshold = value.toDouble();
   else if (c == NumPixelsPLIM && acq_mode == PLIM)
      modulator->setNumPixels(value.toUInt());

};

QVariant Cronologic::getParameter(const QString& parameter, ParameterType type) 
{ 
   int channel;
   ParameterClass c = getParameterClass(parameter, channel);
   if (c == Threshold)
      return threshold[channel];
   else if (c == StartThreshold)
      return start_threshold;
   else if (c == TimeShift)
      return time_shift[channel];
   else if (c == NumPixelsPLIM && acq_mode == PLIM)
      return modulator->getNumPixels();

   return 0;
};

QVariant Cronologic::getParameterLimit(const QString& parameter, ParameterType type, Limit limit)
{
   int channel;
   ParameterClass c = getParameterClass(parameter, channel);
   if (c == Threshold || c == StartThreshold)
   {
      if (limit == Limit::Min)
         return -1000.0;
      else
         return 1180.0;
   }
   else if (c == TimeShift)
   {
      if (limit == Limit::Min)
         return 0;
      else
         return 25;
   }
   else if (c == NumPixelsPLIM)
   {
      if (limit == Limit::Min)
         return 1;
      else
         return 2048;
   }

   return QVariant();
};

QVariant Cronologic::getParameterMinIncrement(const QString& parameter, ParameterType type)
{ 
   int channel;
   ParameterClass c = getParameterClass(parameter, channel);
   if (c == Threshold || c == StartThreshold)
      return 1.0;
   else if (c == TimeShift)
      return 1;

   return 0.0;
}; 

EnumerationList Cronologic::getEnumerationList(const QString& parameter)
{ 
   return EnumerationList(); 
};

bool Cronologic::isParameterWritable(const QString& parameter)
{ 
   int channel;
   ParameterClass c = getParameterClass(parameter, channel);
   if (c == Threshold || c == StartThreshold)
      return !running;
   else if (c == TimeShift)
      return true;
   else if (c == NumPixelsPLIM)
      return acq_mode == PLIM;

   return true;
};

bool Cronologic::isParameterReadOnly(const QString& parameter) 
{ 
   return false; 
};














void Cronologic::checkCard()
{
   // prepare initialization
   timetagger4_init_parameters params;
   timetagger4_get_default_init_parameters(&params);
   params.buffer_size[0] = 8 * 1024 * 1024;	
   params.board_id = 0;						// number copied to "card" field of every packet, allowed range 0..255
   params.card_index = 0;						// initialize first card found in the system
   // ordering depends on PCIe slot position if more than one card is present

   // initialize card
   int error_code;
   const char * error_message;
   device = timetagger4_init(&params, &error_code, &error_message);
   CHECK(error_code);

   board_name = "Cronologic TimeTagger4-";

   // print board information
   timetagger4_static_info staticinfo;
   timetagger4_get_static_info(device, &staticinfo);
   printf("Board Serial        : %d.%d\n", staticinfo.board_serial >> 24, staticinfo.board_serial & 0xffffff);
   printf("Board Configuration : TimeTagger4-");
   switch (staticinfo.board_configuration&TIMETAGGER4_BOARDCONF_MASK)
   {
   case TIMETAGGER4_1G_BOARDCONF:
      printf("1G\n");
      board_name.append("1G");
      break;
   case TIMETAGGER4_2G_BOARDCONF:
      printf("2G\n");
      board_name.append("2G");
      break;
   default:
      printf("unknown\n");
   }
   printf("Board Revision      : %d\n", staticinfo.board_revision);
   printf("Firmware Revision   : %d.%d\n", staticinfo.firmware_revision, staticinfo.subversion_revision);
   printf("Driver Revision     : %d.%d.%d.%d\n", ((staticinfo.driver_revision >> 24) & 255), ((staticinfo.driver_revision >> 16) & 255), ((staticinfo.driver_revision >> 8) & 255), (staticinfo.driver_revision & 255));

}



void Cronologic::configureCard()
{
   // prepare configuration
   timetagger4_configuration config;
   // fill configuration data structure with default values
   // so that the configuration is valid and only parameters
   // of interest have to be set explicitly
   timetagger4_get_default_configuration(device, &config);

   int n_chan = getNumChannels();

   // set config of the 4 TDC channels, inputs A - D
   for (int i = 0; i < n_chan; i++)
   {
      // enable recording hits on TDC channel
      config.channel[i].enabled = true;

      // measure falling edge
      config.trigger[i + 1].rising = false;
      config.trigger[i + 1].falling = true;

      // do not filter any hits
      // fine timestamp is a 30 bit unsigned int
      config.channel[i].start = 0;				// discard hits with fine timestamp less than start
      config.channel[i].stop =  1 << 30;		// discard hits with fine timestamp more than stop
   }

   for (int i = 0; i < n_chan; i++)
      config.dc_offset[i+1] = threshold[i] * 1e-3;

   int marker_channel = 3;
   config.channel[marker_channel].enabled = true;
   config.channel[marker_channel].start = 0;
   config.channel[marker_channel].stop = 1 << 30;
   
   config.dc_offset[marker_channel+1] = 1; // arduino signal is only ~1.8V driving 50Ohm load
   config.trigger[marker_channel+1].rising = true;
   config.trigger[marker_channel+1].falling = true;

   // start group on falling edges on the start channel
   config.trigger[0].rising = true;	// disable packet generation on rising edge of start pulse
   config.trigger[0].falling = false;	// enable packet generation on falling edge of start pulse
   config.dc_offset[0] = start_threshold * 1e-3; 

   // activate configuration
   CHECK(timetagger4_configure(device, &config));

   timetagger4_param_info parinfo;
   timetagger4_get_param_info(device, &parinfo);
   printf("\nTDC binsize         : %0.2f ps\n", parinfo.binsize);

   bin_size_ps = parinfo.binsize;
}

void Cronologic::init()
{
   FifoTcspc::init();
}



void Cronologic::startModule()
{
   last_mark_rise_time = 0;
   macro_time_rollovers = -1;

   if (acq_mode == PLIM)
   {
      int n_px = modulator->getNumPixels();
      cur_flimage->setImageSize(n_px, n_px);
      modulator->setModulation(true);
   }


   CHECK(timetagger4_start_tiger(device));
   // start data capture
   CHECK(timetagger4_start_capture(device));
   running = true;
}

void Cronologic::stopModule()
{
   CHECK(timetagger4_stop_tiger(device));
   CHECK(timetagger4_stop_capture(device));
   running = false;

   if (acq_mode == PLIM)
      modulator->setModulation(false);
}

Cronologic::~Cronologic()
{
   stopFIFO();

   // deactivate XTDC4
   timetagger4_close(device);
}


size_t Cronologic::readPackets(std::vector<TcspcEvent>& buffer)
{

   timetagger4_read_in read_config;
   timetagger4_read_out read_data;

   read_config.acknowledge_last_read = true;

   size_t buffer_length = buffer.size();
   int idx = 0;
   int continues = 0;
   do
   {

      int status = timetagger4_read(device, &read_config, &read_data);
      
      if (status > 0)
      {
         QThread::usleep(10);
         if (continues++ > 200)
            break;
         continue;
      }
      continues = 0;
      
      CHECK(read_data.error_code);

      // iterate over all packets received with the last read
      crono_packet* p = read_data.first_packet;
      while (p <= read_data.last_packet)
      {
         // Measure sync rate
         int update_count = (acq_mode == PLIM) ? 1000 : 10000;
         if (packet_count % (update_count+1) == 0)
         {
            if (last_update_time > 0)
            {
               sync_period_bins = static_cast<double>(p->timestamp - last_update_time) / (update_count * sync_divider);
               
               if (acq_mode == FLIM)
               {
                  n_bins = ceil(sync_period_bins);
                  sync_rate_hz = 1e12 / (bin_size_ps * sync_period_bins);
               }
               else
               {
                  sync_rate_hz = 1.0; // TODO: workaround
               }
   
            }

            last_update_time = p->timestamp;
            rates.sync = sync_rate_hz;
         }
         packet_count++;

         int flags = p->flags;

         int hit_count = 2 * p->length;
         if (flags & TIMETAGGER4_PACKET_FLAG_ODD_HITS)
              hit_count -= 1;

         flags = flags & 0xFE; // get rid of odd hits flag


         if (flags & TIMETAGGER4_PACKET_FLAG_SLOW_SYNC)
         {
         }  //std::cout << "Slow sync\n";
         else if (flags)
            std::cout << "Flag: " << (int) p->flags << "\n";
        
         uint64_t macro_time = p->timestamp;

         if (acq_mode == AcquisitionMode::PLIM)
         {
            if ((idx + 1) >= buffer.size())
               buffer.resize(buffer.size() * 2);
            // Insert pixel marker for PLIM
            buffer[idx++] = { macro_time & 0xFFFF, 0xF | (MARK_PIXEL << 4) };
         }

         uint64_t div_macro_time = macro_time >> macro_downsample;
         uint64_t new_macro_time_rollovers = div_macro_time / (1 << 16);

         if (macro_time_rollovers == -1)
            macro_time_rollovers = new_macro_time_rollovers;

//         assert(new_macro_time_rollovers - macro_time_rollovers < 0xFFFF);
         uint64_t rollovers = new_macro_time_rollovers - macro_time_rollovers;
         uint64_t rollover_events = ceil(((double)rollovers) / (1 << 16));

         size_t required_size = idx + hit_count + rollover_events;
         if (required_size >= buffer.size())
            buffer.resize(required_size * 1.2);
         
         for (int i = 0; i < rollover_events; i++)
         {
            uint16_t rollovers_i = (uint16_t) std::min(rollovers, 0xFFFFULL);
            buffer[idx++] = { rollovers_i, 0xF };
            rollovers -= rollovers_i;
         }
         macro_time_rollovers = new_macro_time_rollovers;

         uint32_t* packet_data = (uint32_t*)(p->data);
         for (int i = 0; i < hit_count; i++)
         {            
            TcspcEvent evt;

            uint64_t hit_fast = *(packet_data + i);
            int channel = hit_fast & 0xF;
            
            uint64_t micro_time = hit_fast >> 8;
            uint64_t adj_macro_time = macro_time;

            if (acq_mode == FLIM)
            {
               // Correct for sync division
               double intpart;
               double micro_time_f = modf(micro_time / sync_period_bins, &intpart)*sync_period_bins;
               if (channel < 3)
                  micro_time_f += time_shift[channel];
               micro_time = ((uint64_t) std::round(micro_time_f)) % n_bins;
               adj_macro_time += std::round(intpart * sync_period_bins);
            }

            uint16_t downsampled_micro_time = micro_time >> micro_downsample;
            
            uint64_t marker = 0;
            int ignore = false;

            // If channel == 3 then we've got a marker not a photon
            // We determine what kind of marker based on the duration 
            // of the marker, i.e. time between rising and falling edge
            if (channel == 3)
            {
               // Is the marker the rising or falling edge?
               bool rising = hit_fast & 0x10;
               uint64_t time = (hit_fast>>8) + macro_time;
               //uint64_t t = micro_time + adj_macro_time;

               if (rising)
               {
                  // We don't want to include rising edge in data stream
                  last_mark_rise_time = time;
                  ignore = true;
                  
               }  
               else
               {
                  uint64_t marker_length_i = time - last_mark_rise_time;
                  double marker_length = marker_length_i * bin_size_ps; // TODO: convert times to ints
                  if (marker_length < 30e3)
                  {
                     marker = MARK_LINE_END; // 23e3
                     line_active = false;
                  }
                  else if (marker_length < 100e3)
                  {
                     marker = MARK_LINE_START; // 71
                     line_active = true;
                     n_line++;
                     n_pixel = 0;
                  }
                  else if (marker_length < 200e3)
                  {
                     marker = MARK_FRAME; // 154e3
                     n_line = 0;
                  }
                  else
                  {
                     std::cout << "Unknown length (too long)\n";
                  }
                  //adj_macro_time -= marker_length_i;
                  last_mark_rise_time = -1;
               }
            }

            if (marker)
               evt.micro_time = 0xF | (marker << 4);
            else
               evt.micro_time = channel | (downsampled_micro_time << 4);

            evt.macro_time = (adj_macro_time >> macro_downsample) & 0xFFFF;


            if (!ignore)
                buffer[idx++] = evt;
         }

         p = crono_next_packet(p);
      }
   } while (idx < (buffer_length * 0.5));
   return idx;
}


void Cronologic::configureModule()
{
   configureCard();
}


