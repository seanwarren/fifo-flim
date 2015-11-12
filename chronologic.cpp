#include "Chronologic.h"

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
      std::string msg = "Chronologic Error Code: " + std::to_string(err);
      throw std::runtime_error(msg);
   }
}

Chronologic::Chronologic(QObject* parent) :
FifoTcspc(parent),
packet_buffer(PacketBuffer<crono_packet>(1000, 10000))
{
   CheckCard();
   ConfigureCard();


   StartThread();
}

void Chronologic::CheckCard()
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
}

void Chronologic::ConfigureCard()
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
      config.channel[i].enabled = 1;

      // measure falling edge
      config.channel[i].rising = 0;

      // do not filter any hits
      // fine timestamp is a 30 bit unsigned int
      config.channel[i].start = 0;				// discard hits with fine timestamp less than start
      config.channel[i].stop = 0x3fffffff;		// discard hits with fine timestamp more than stop
   }

   // generate low active pulse on LEMO output A - D
   for (int i = 1; i < TIMETAGGER4_TIGER_COUNT; i++)
   {
      config.tiger_block[i].enable = 0;
      config.tiger_block[i].start = 2 + i;
      config.tiger_block[i].stop = 3 + i;
      config.tiger_block[i].negate = 1;
      config.tiger_block[i].retrigger = 0;
      config.tiger_block[i].extend = 0;
      config.tiger_block[i].enable_lemo_output = 0;
      config.tiger_block[i].sources = TIMETAGGER4_TRIGGER_SOURCE_AUTO;
   }

   // adjust base line for inputs Start, A - D
   // default NIM pulses
   // if TiGeR is used for triggering:
   // positive pulses: timetagger4_DC_OFFSET_P_LVCMOS_18
   // negative pulses: timetagger4_DC_OFFSET_N_LVCMOS_18
   for (int i = 0; i < 5; i++)
      config.dc_offset[i] = TIMETAGGER4_DC_OFFSET_N_NIM;

   // activate configuration
   CHECK(timetagger4_configure(device, &config));
}

void Chronologic::Init()
{
   FifoTcspc::Init();
}



void Chronologic::StartModule()
{
   // start data capture
   CHECK(timetagger4_start_capture(device));

   // start timing generator
   timetagger4_start_tiger(device);
}

Chronologic::~Chronologic()
{
   StopFIFO();

   // deactivate XTDC4
   timetagger4_close(device);
}

void Chronologic::WriteFileHeader()
{
   // Write header
   quint32 spc_header = 0;
   quint32 magic_number = 0xF1F0;
   quint32 header_size = 4;
   quint32 format_version = 1;

   data_stream << magic_number << header_size << format_version << n_x << n_y << spc_header;
}

void Chronologic::ProcessPhotons()
{
   vector<crono_packet>& buffer = packet_buffer.GetNextBufferToProcess();

   if (buffer.empty()) // TODO: use a condition variable here
      return;

   //for (auto& p : buffer)
   //   cur_flimage->AddPhotonEvent(p);

   packet_buffer.FinishedProcessingBuffer();
}


void Chronologic::ReaderThread()
{

   // configure read behaviour
   timetagger4_read_in read_config;
   // automatically acknowledge all data as processed
   // on the next call to xtdc4_read()
   // old packet pointers are invalid after calling xtdc4_read()
   read_config.acknowledge_last_read = 1;

   // structure with packet pointers for read data
   timetagger4_read_out read_data;

   // some book keeping
   int packet_count = 0;
   int empty_packets = 0;
   int packets_with_errors = 0;
   bool last_read_no_data = false;

   printf("Reading packets:\n");

   short ret = 0;
   bool send_stop_command = false;
   while (true)
   {
      int status = timetagger4_read(device, &read_config, &read_data);

      ReadPackets();

      if (terminate && !send_stop_command) // & ((spc_state & SPC_FEMPTY) != 0))
      {
         send_stop_command = true;
         timetagger4_stop_tiger(device);
      }
   }

   SetRecording(false);

   // Flush the FIFO buffer
   vector<Photon> b(1000);
   unsigned long read_size;
   do
   {
      read_size = 1000;
      // TODO: read remaining data here
   } while (read_size > 0);


}

bool Chronologic::ReadPackets()
{
   timetagger4_read_in read_config;
   read_config.acknowledge_last_read = 1;
   timetagger4_read_out read_data;

   int status = timetagger4_read(device, &read_config, &read_data);

   if (status > 0)
      return false;

   bool more_to_read = true;

   vector<crono_packet>& buffer = packet_buffer.GetNextBufferToFill();

   int photons_in_buffer = 0;
   size_t buffer_length = buffer.size();

   int word_length = 2;
   int packet_size = sizeof(crono_packet);
   
   while (photons_in_buffer < 0.75 * buffer_length)
   {
      unsigned long read_size = (static_cast<unsigned long>(buffer_length)-photons_in_buffer) * word_length;


      //if (recording)
         // TODO: data_stream.writeRawData(reinterpret_cast<char*>(ptr), read_size * words_per_packet);

         //ptr += read_size;

         //unsigned long n_photons_read = read_size / words_per_photon;
         //photons_in_buffer += n_photons_read;
         //photons_read += n_photons_read;

         int ret = 0;

      if (ret < 0)
         break;
      if (read_size == 0)
         break;

      if (ret == 1)
         more_to_read = false;
   }

   if (photons_in_buffer > 0)
   {
      buffer.resize(photons_in_buffer);
      packet_buffer.FinishedFillingBuffer();
   }
   else
   {
      packet_buffer.FailedToFillBuffer();
   }

   return more_to_read;
}


//============================================================
// Configure card for FIFO mode
//
// This function is mostly lifted from the use_spcm.c 
// example file
//============================================================
void Chronologic::ConfigureModule()
{

}
