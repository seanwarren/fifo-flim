#include "Cronologic.h"

#include <QMessageBox>
#include <QStandardPaths>
#include <QFileDialog>
#include <chrono>
#include <cassert>

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
   acq_mode = PLIM;

   if (acq_mode == FLIM)
      processor = createEventProcessor<Cronologic, CLFlimEvent, cl_event>(this, 1000, 10000);
   else
      processor = createEventProcessor<Cronologic, CLPlimEvent, cl_event>(this, 1000, 10000);

   checkCard();
   configureCard();
   
   cur_flimage = std::make_shared<FLIMage>(acq_mode == PLIM, bin_size_ps, bin_size_ps, 5, 3);

   processor->addTcspcEventConsumer(cur_flimage);

   StartThread();
}

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

   // set config of the 4 TDC channels, inputs A - D
   for (int i = 0; i < TIMETAGGER4_TDC_CHANNEL_COUNT; i++)
   {
      // enable recording hits on TDC channel
      config.channel[i].enabled = true;

      // measure falling edge
      config.trigger[i + 1].rising = false;
      config.trigger[i + 1].falling = true;

      // do not filter any hits
      // fine timestamp is a 30 bit unsigned int
      config.channel[i].start = 0;				// discard hits with fine timestamp less than start
      config.channel[i].stop = 1<<30; //25 * 6 - 1; // 1 << 24;		// discard hits with fine timestamp more than stop
   }

   for (int i = 1; i < TIMETAGGER4_TDC_CHANNEL_COUNT + 1; i++)
      config.dc_offset[i] = -0.090;

   config.dc_offset[4] = 1; // arduino signal is only ~1.8V driving 50Ohm load
   config.trigger[4].rising = true;
   config.trigger[4].falling = true;

   // start group on falling edges on the start channel
   config.trigger[0].rising = true;	// disable packet generation on rising edge of start pulse
   config.trigger[0].falling = false;	// enable packet generation on falling edge of start pulse
   config.dc_offset[0] = TIMETAGGER4_DC_OFFSET_N_NIM; 
//   config.dc_offset[0] = TIMETAGGER4_DC_OFFSET_P_LVCMOS_33;

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

   CHECK(timetagger4_start_tiger(device));
   // start data capture
   CHECK(timetagger4_start_capture(device));
}

void Cronologic::stopModule()
{
   CHECK(timetagger4_stop_tiger(device));
   CHECK(timetagger4_stop_capture(device));

}

Cronologic::~Cronologic()
{
   stopFIFO();

   // deactivate XTDC4
   timetagger4_close(device);
}

#define crono_next_packet_2(current) ((crono_packet*) (((__int64) (current)) +( ((current)->type&128?0:1) + 2) * 8))


size_t Cronologic::readPackets(std::vector<cl_event>& buffer)
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
         if (continues++ > 1000)
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
         int update_count = 10000;
         if (packet_count % update_count == 0)
         {
            double period_ps = bin_size_ps * static_cast<double>(p->timestamp - last_update_time) / update_count;
            sync_rate_hz = 1e12 / period_ps;
            last_update_time = p->timestamp;

            rates.sync = sync_rate_hz;
            emit ratesUpdated(rates);
         }
         packet_count++;

         if ((idx + 1) >= buffer_length)
            break;

         int hit_count = 2 * p->length;
         if (p->flags & TIMETAGGER4_PACKET_FLAG_ODD_HITS)
              hit_count -= 1;

         if (p->flags & TIMETAGGER4_PACKET_FLAG_SLOW_SYNC)
            std::cout << "Slow sync\n";

         if (p->flags > 1)
            std::cout << "Flag: " << (int) p->flags << "\n";
        
         uint64_t hit_slow = p->timestamp;
         hit_slow = (hit_slow & 0xFFFFFFFFFFFFFFF) << 4;

         if (acq_mode == AcquisitionMode::PLIM)
         {
            // Insert pixel marker for PLIM
            cl_event evt;
            evt.hit_slow = hit_slow | MARK_PIXEL;
            buffer[idx++] = evt;
         }

         int ignore = false;
         uint32_t* packet_data = (uint32_t*)(p->data);
         for (int i = 0; i < hit_count; i++)
         {
            if (idx >= buffer.size())
               buffer.resize(idx * 2);

            cl_event evt;
            evt.hit_fast = *(packet_data + i);
            int channel = evt.hit_fast & 0xF;
            uint64_t marker = 0;

            // If channel == 3 then we've got a marker not a photon
            // We determine what kind of marker based on the duration 
            // of the marker, i.e. time between rising and falling edge
            if (channel == 3)
            {
               // Is the marker the rising or falling edge?
               bool rising = evt.hit_fast & 0x10;
               
               // Get arrival time of edge
               int hit_fast = evt.hit_fast;
               uint64_t micro_time = (hit_fast >> 8);
               uint64_t macro_time = p->timestamp;
               uint64_t time = micro_time + macro_time;

               if (rising)
               {
                  // We don't want to include rising edge in data stream
                  last_mark_rise_time = time;
                  ignore = true;
               }  
               else
               {
                  {
                     uint64_t marker_length_i = time - last_mark_rise_time;
                     double marker_length = marker_length_i * bin_size_ps; // TODO: convert times to ints

                     if (marker_length < 30e3)
                     {
                        if (line_active)
                        {
                           marker = MARK_PIXEL;
                           n_pixel++;
                        }
                     }
                     else if (marker_length < 160e3)
                     {
                        marker = MARK_FRAME; // 154
                        n_line = 0;
                     }
                     else if (marker_length < 180e3)
                     {
                        marker = MARK_LINE_END; // 166
                        line_active = false;
                     }
                     else if (marker_length < 210e3)
                     {
                        marker = MARK_LINE_START;
                        line_active = true;
                        n_line++;
                        n_pixel = 0;
                     }
                  }
                  
                 last_mark_rise_time = -1;
               }
            }

            evt.hit_slow = hit_slow | marker;

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

}


