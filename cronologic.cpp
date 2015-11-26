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
   processor = createEventProcessor<Cronologic, CLEvent, cl_event>(this, 100, 10000);

   checkCard();
   configureCard();
   
   cur_flimage = new FLIMage(1, 1, 0, 8);

   coarse_factor_ps = 5000.0 / 3; // 1.6666 ns from docs

   processor->setFLIMage(cur_flimage);

   StartThread();
}

void Cronologic::checkCard()
{
   // prepare initialization
   timetagger4_init_parameters params;
   timetagger4_get_default_init_parameters(&params);
   params.buffer_size[0] = 8 * 1024 * 1024;	// use 8 MByte as packet buffer
   params.board_id = 0;						// number copied to "card" field of every packet, allowed range 0..255
   params.card_index = 0;						// initialize first card found in the system
   // ordering depends on PCIe slot position if more than one card is present

   // initialize card
   int error_code;
   const char * error_message;
   device = timetagger4_init(&params, &error_code, &error_message);
   CHECK(error_code);

   // print board information
   timetagger4_static_info staticinfo;
   timetagger4_get_static_info(device, &staticinfo);
   printf("Board Serial        : %d.%d\n", staticinfo.board_serial >> 24, staticinfo.board_serial & 0xffffff);
   printf("Board Configuration : TimeTagger4-");
   switch (staticinfo.board_configuration&TIMETAGGER4_BOARDCONF_MASK)
   {
   case TIMETAGGER4_1G_BOARDCONF:
      printf("1G\n"); break;
   case TIMETAGGER4_2G_BOARDCONF:
      printf("2G\n"); break;
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

   // reset TDC time counter on falling edge of start pulse
   config.start_rising = 0;

   // set config of the 4 TDC channels, inputs A - D
   for (int i = 0; i < TIMETAGGER4_TDC_CHANNEL_COUNT; i++)
   {
      // enable recording hits on TDC channel
      config.channel[i].enabled = true;

      config.auto_trigger_period = 0; // TESTING
      config.auto_trigger_random_exponent = 4; // TESTING

      // measure falling edge
      config.trigger[i + 1].rising = true;
      config.trigger[i + 1].falling = true;

      // do not filter any hits
      // fine timestamp is a 30 bit unsigned int
      config.channel[i].start = 0;				// discard hits with fine timestamp less than start
      config.channel[i].stop = 0x3fffffff;		// discard hits with fine timestamp more than stop
   }

   // generate low active pulse on LEMO output A - D
   for (int i = 0; i < TIMETAGGER4_TIGER_COUNT; i++)
   {
      config.tiger_block[i].enable = true;
      config.tiger_block[i].start = 2 + i;
      config.tiger_block[i].stop = 3 + i;
      config.tiger_block[i].negate = 1;
      config.tiger_block[i].retrigger = 0;
      config.tiger_block[i].extend = 0;
      config.tiger_block[i].enable_lemo_output = 0;
      config.tiger_block[i].sources = TIMETAGGER4_TRIGGER_SOURCE_AUTO;
   }

   for (int i = 0; i < TIMETAGGER4_TDC_CHANNEL_COUNT + 1; i++)
      config.dc_offset[i] = TIMETAGGER4_DC_OFFSET_N_NIM;

   // start group on falling edges on the start channel
   config.trigger[0].falling = true;	// enable packet generation on falling edge of start pulse
   config.trigger[0].rising = false;	// disable packet generation on rising edge of start pulse


   // activate configuration
   CHECK(timetagger4_configure(device, &config));

   timetagger4_param_info parinfo;
   timetagger4_get_param_info(device, &parinfo);
   printf("\nTDC binsize         : %0.2f ps\n", parinfo.binsize);
}

void Cronologic::init()
{
   FifoTcspc::init();
}



void Cronologic::startModule()
{
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


bool Cronologic::readPackets(std::vector<cl_event>& buffer)
{

   timetagger4_read_in read_config;
   timetagger4_read_out read_data;

   read_config.acknowledge_last_read = true;

   int status = timetagger4_read(device, &read_config, &read_data);

   if (status > 0)
      return false;

   size_t buffer_length = buffer.size();
   
   // iterate over all packets received with the last read
   int idx = 0;
   crono_packet* p = read_data.first_packet;
   while (p <= read_data.last_packet && idx < buffer_length)
   {
      int hit_count = 2 * p->length;
      if (p->flags & TIMETAGGER4_PACKET_FLAG_ODD_HITS)
         hit_count -= 1;

      int ignore = false;
      uint32_t* packet_data = (uint32_t*)(p->data);
      for (int i = 0; i < hit_count; i++)
      {
         cl_event evt;
         evt.hit_fast = *(packet_data + i);

         uint64_t hit_slow = p->timestamp << 4;

         // If channel == 4 then we've got a marker not a photon
         // We determine what kind of marker based on the duration 
         // of the marker, i.e. time between rising and falling edge
         if ((evt.hit_fast & 0xF) == 4) 
         {
            // Is the marker the rising or falling edge?
            bool rising = evt.hit_fast & 0x10;

            // Get arrival time of edge
            double micro_time = (evt.hit_fast >> 8) * bin_size_ps;
            double macro_time = p->timestamp * coarse_factor_ps;
            double time = micro_time + macro_time;

            if (rising)
            {
               // We don't want to include rising edge in data stream
               last_mark_rising_time = time;
               ignore = true;
            }
            else
            {
               uint64_t marker = 0;
               double marker_length = time - last_mark_rising_time;
               if (marker_length < 20e4)
                  marker = MARK_PIXEL;
               else if (marker_length < 40e4)
                  marker = MARK_LINE;
               else
                  marker = MARK_FRAME;

               hit_slow = hit_slow | marker;
            }
         }

         evt.hit_slow = hit_slow;
   
         if (!ignore)
            buffer[idx++] = evt;
      }
      p = crono_next_packet(p);
   }

   // if this fails we need to increase the buffer size
   assert(p == read_data.last_packet);

   if (idx > 0)
   {
      buffer.resize(idx);
      return true;
   }
   else
   {
      return false;
   }

   return false;
}


//============================================================
// Configure card for FIFO mode
//
// This function is mostly lifted from the use_spcm.c 
// example file
//============================================================
void Cronologic::configureModule()
{

}


