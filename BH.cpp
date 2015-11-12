#include "BH.h"
#include <QMessageBox>
#include <QStandardPaths>
#include <QFileDialog>
#include <chrono>
#include <cassert>

using namespace std;

void CHECK(int err)
{
   if (err < 0)
   {
      std::cout << "BH Error Code: " << err;
      throw std::exception("BH Error", err);
   }
}

BH::BH(QObject* parent, short module_type) :
FifoTcspc(parent),
module_type(module_type),
packet_buffer(PacketBuffer<Photon>(1000, 10000))
{
   // Load INI configuration file
   char ini_name[] = "C:/users/bsherloc/desktop/FIFO.ini";
   CHECK(SPC_init(ini_name));

   // Activate cards and check we've got at least one that works
   ActivateSPCMCards(module_type);

   if (total_no_of_spc == 0)
      throw std::exception("No SPC modules found");

   if (no_of_active_spc == 0)
   {
      QMessageBox msgbox(QMessageBox::Warning, "Force SPC card connection?", "Some SPC cards are in use, would you like to try and connect to them anyway?");
      msgbox.addButton(QMessageBox::Yes);
      msgbox.addButton(QMessageBox::No);

      if (msgbox.exec() == QMessageBox::Yes)
         ActivateSPCMCards(module_type, true);

      if (no_of_active_spc == 0)
         throw std::exception("No compatible SPC modules found");
   }

   // Get parameters from card
   CHECK(SPC_get_parameters(act_mod, &spc_dat));

   CHECK(SPC_set_parameter(act_mod, SYNC_THRESHOLD, -16.0));

   StartThread();
}

void BH::Init()
{
   FifoTcspc::Init();

   // Setup rate timer
   SPC_clear_rates(act_mod);
   rate_timer = new QTimer(this);
   connect(rate_timer, &QTimer::timeout, this, &BH::ReadRates);
   rate_timer->start(1000);
}


//============================================================
// Read rates from the active card
//============================================================
rate_values BH::ReadRates()
{
//   cout << "Pixel Marks: " << pmark << "\n";
//   pmark = 0;

   float usage;
   rate_values rates;
   SPC_read_rates(act_mod, &rates);
   SPC_get_fifo_usage(act_mod, &usage);
   emit RatesUpdated(rates);
   emit FifoUsageUpdated(usage);
   return rates;
}


/*
   Activate SPC cards of type module_type, e.g. M_SPC830
*/
void BH::ActivateSPCMCards(short module_type, bool force)
{
   // Cycle through SPC cards and work out which are ok to use
   //============================================================
   SPCModInfo mod_info[MAX_NO_OF_SPC];
   for (int i = 0; i < MAX_NO_OF_SPC; i++)
   {
      SPC_get_module_info(i, (SPCModInfo *)&mod_info[i]);
      if (mod_info[i].module_type != M_WRONG_TYPE)
         total_no_of_spc++;
      if (mod_info[i].init == INIT_SPC_OK || (mod_info[i].init == INIT_SPC_MOD_IN_USE && force))
      {
         no_of_active_spc++; 
         mod_active[i] = 1;
         act_mod = i;
      }
      else
         mod_active[i] = 0;
   }

   // Disable cards of incorrect type
   //============================================================
   short work_mode = SPC_get_mode();
   for (int i = 0; i < MAX_NO_OF_SPC; i++)
   {
      if (mod_active[i])
      {
         if (mod_info[i].module_type != module_type)
         {
            mod_active[i] = 0;
            no_of_active_spc--;
         }
      }
   }

   // this will deactivate unused modules in DLL
   SPC_set_mode(work_mode, force, mod_active);
}


float BH::GetParameter(short par_id)
{
   float par;
   SPC_get_parameter(act_mod, par_id, &par);
   return par;
}

void BH::SetParameter(short par_id, float par)
{
   SPC_set_parameter(act_mod, par_id, par);
}


void BH::StartModule()
{
   CHECK(SPC_set_parameter(act_mod, SCAN_SIZE_X, n_x));
   CHECK(SPC_set_parameter(act_mod, SCAN_SIZE_Y, n_y));

   // Setup timer
   //SPC_set_parameter(act_mod, STOP_ON_TIME, 1);
   //SPC_set_parameter(act_mod, COLLECT_TIME, 20.0);

   // Start BH card
   CHECK(SPC_start_measurement(act_mod));

   unsigned int spc_header;
   CHECK(SPC_get_fifo_init_vars(act_mod, NULL, NULL, NULL, &spc_header));
}

BH::~BH()
{
   StopFIFO();

   // Deactivate all modules
   for (int i = 0; i < 8; i++)
      mod_active[i] = 0;

   SPC_set_mode(SPC_HARD, false, mod_active);

}

void BH::WriteFileHeader()
{
   // Write header
   quint32 spc_header;
   quint32 magic_number = 0xF1F0;
   quint32 header_size = 4;
   quint32 format_version = 1;

   CHECK(SPC_get_fifo_init_vars(act_mod, NULL, NULL, NULL, &spc_header));

   data_stream << magic_number << header_size << format_version << n_x << n_y << spc_header;
}



void BH::ReaderThread()
{
   short ret = 0; 
   bool send_stop_command = false;
   while (true)
   {
      short spc_state;
      SPC_test_state(act_mod, &spc_state);

      if (spc_state & SPC_WAIT_TRG) // wait for trigger 
         continue;

      if ((spc_state & SPC_FEMPTY) && send_stop_command) // FIFO empty
         break;

      if (spc_state & SPC_FOVFL)
         // Fifo overrun occured 
         //  - macro time information after the overrun is not consistent
         //    consider to break the measurement and lower photon's rate
         throw std::exception("FIFO buffer overflowed!");

      if (spc_state & SPC_TIME_OVER & SPC_FEMPTY)
         break;

      if (!(spc_state & SPC_FEMPTY))
         ReadPhotons();

      if (terminate && !send_stop_command) // & ((spc_state & SPC_FEMPTY) != 0))
      {
         send_stop_command = true;
         // Need to do this twice - see BH docs
         CHECK(SPC_stop_measurement(act_mod));
         CHECK(SPC_stop_measurement(act_mod));
      }
   }

   SetRecording(false);

   // Flush the FIFO buffer
   vector<Photon> b(1000);
   unsigned long read_size;
   do
   {
      read_size = 1000;
      SPC_read_fifo(act_mod, &read_size, reinterpret_cast<unsigned short*>(b.data()));
   } while (read_size > 0);


}

bool BH::ReadPhotons()
{
   short ret = 0;
   bool more_to_read = true;

   vector<Photon>& buffer = packet_buffer.GetNextBufferToFill();

   int photons_in_buffer = 0;
   size_t buffer_length = buffer.size();

   int word_length = 2;
   int words_per_photon = sizeof(Photon) / word_length;

   unsigned short* ptr = reinterpret_cast<unsigned short*>(buffer.data());

   while (photons_in_buffer < 0.75 * buffer_length)
   {
      unsigned long read_size = (static_cast<unsigned long>(buffer_length) - photons_in_buffer) * word_length;

      // before the call read_size contains required number of words to read from fifo
      // after the call current_cnt contains number of words read from fifo  
      short ret = SPC_read_fifo(act_mod, &read_size, ptr);
      
      if (recording)
         data_stream.writeRawData(reinterpret_cast<char*>(ptr), read_size * words_per_photon);

      ptr += read_size;

      unsigned long n_photons_read = read_size / words_per_photon;
      photons_in_buffer += n_photons_read;
      packets_read += n_photons_read;

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
void BH::ConfigureModule()
{

   float curr_mode;

   // in most of the modules types with FIFO mode it is possible to stop the fifo measurement 
   //   after specified Collection time
   fifo_stopt_possible = 1;
   short first_write = 1;



   SPC_get_version(act_mod, &fpga_version);

   // SPC-150 and the newest SPC-140 & SPC-830 can record in FIFO modes 
   //                   up to 4 external markers events
   //  predefined mode FIFO32_M is used for Fifo Imaging - it uses markers 0-2 
   //     as  Pixel, Line & Frame clocks
   // in normal Fifo mode ( ROUT_OUT ) recording markers 0-3 can be enabled by 
   //    setting a parameter ROUTING_MODE ( bits 8-11 )


   // before the measurement sequencer must be disabled
   SPC_enable_sequencer(act_mod, 0);

   // set correct measurement mode
   SPC_get_parameter(act_mod, MODE, &curr_mode);

   /*

   switch (module_type){
   case M_SPC130:
      SPC_set_parameter(act_mod, MODE, FIFO130);
      fifo_type = FIFO_130;
      fifo_size = 262144;  // 256K 16-bit words
      if (fpga_version < 0x306)  // < v.C6
         fifo_stopt_possible = 0;
      break;

   case M_SPC600:
   case M_SPC630:
      SPC_set_parameter(act_mod, MODE, FIFO_32);  // or FIFO_48
      fifo_type = FIFO_32;  // or FIFO_48
      fifo_size = 2 * 262144;   // 512K 16-bit words
      if (fpga_version <= 0x207)  // <= v.B7
         fifo_stopt_possible = 0;
      break;

   case M_SPC830:
      // ROUT_OUT for 830 == fifo
      // with FPGA v. > CO  also FIFO32_M possible 
      SPC_set_parameter(act_mod, MODE, FIFO_32M);   // ROUT_OUT in 830 == fifo
      // or FIFO_32M
      fifo_type = FIFO_32M;    // or FIFO_IMG
      fifo_size = 64 * 262144; // 16777216 ( 16M )16-bit words
      break;

   case M_SPC140:
      // ROUT_OUT for 140 == fifo
      // with FPGA v. > BO  also FIFO32_M possible 
      SPC_set_parameter(act_mod, MODE, ROUT_OUT);   // or FIFO_32M
      fifo_type = FIFO_140;  // or FIFO_IMG
      fifo_size = 16 * 262144;  // 4194304 ( 4M ) 16-bit words
      break;

   case M_SPC150:
      // ROUT_OUT in 150 == fifo
      if (curr_mode != ROUT_OUT &&  curr_mode != FIFO_32M){
         SPC_set_parameter(act_mod, MODE, ROUT_OUT);
         curr_mode = ROUT_OUT;
      }
      fifo_size = 16 * 262144;  // 4194304 ( 4M ) 16-bit words
      if (curr_mode == ROUT_OUT)
         fifo_type = FIFO_150;
      else  // FIFO_IMG ,  marker 3 can be enabled via ROUTING_MODE
         fifo_type = FIFO_IMG;
      break;

   }

   unsigned short rout_mode, scan_polarity;
   float fval;

   // ROUTING_MODE sets active markers and their polarity in Fifo mode (not for FIFO32_M)
   // bits 8-11 - enable Markers0-3,  bits 12-15 - active edge of Markers0-3

   // SCAN_POLARITY sets markers polarity in FIFO32_M mode
   SPC_get_parameter(act_mod, SCAN_POLARITY, &fval);
   scan_polarity = fval;
   SPC_get_parameter(act_mod, ROUTING_MODE, &fval);
   rout_mode = fval;

   // enable all markers
   rout_mode |= (1 << 8 | 1 << 9 | 1 << 10 | 1 << 11);

   // use the same polarity of markers in Fifo_Img and Fifo mode
   rout_mode &= 0xfff8; 
   rout_mode |= scan_polarity & 0x7;

   SPC_get_parameter(act_mod, MODE, &curr_mode);
   if (curr_mode == ROUT_OUT){
      rout_mode |= 0xf00;     // markers 0-3 enabled
      SPC_set_parameter(act_mod, ROUTING_MODE, rout_mode);
   }
   if (curr_mode == FIFO_32M){
      rout_mode |= 0x800;     // additionally enable marker 3
      SPC_set_parameter(act_mod, ROUTING_MODE, rout_mode);
      SPC_set_parameter(act_mod, SCAN_POLARITY, scan_polarity);
   }

   */

   // switch off stop_on_overfl
   SPC_set_parameter(act_mod, STOP_ON_OVFL, 0);
   SPC_set_parameter(act_mod, STOP_ON_TIME, 0);
   /*
   if (fifo_stopt_possible){
      // if Stop on time is possible, the measurement can be stopped automatically 
      //     after Collection time
      /// !!!!!!!!!!!!!!
      //    the mode = FIFO32_M ( fifo_type = FIFO_IMG ) switches off (in hardware) Stop on time
      //        in order to make possible finishing current frame
      //     the measurement will not stop after expiration of collection time
      //     SPC_test_state sets flag SPC_WAIT_FR in the status - software should still read photons 
      //       untill next frame marker appears and then should stop the measurement
      //
      //   to avoid this behaviour - set mode to normal fifo mode = 1 ( ROUT_OUT)
      /// !!!!!!!!!!!!!!
      SPC_set_parameter(act_mod, STOP_ON_TIME, 1);
      SPC_set_parameter(act_mod, COLLECT_TIME, 10.0); // default  - stop after 10 sec
   }
   */

   // there is no need ( and also no way ) to clear FIFO memory before the measurement

   // In FIFO mode the whole SPC memory is a big buffer which is filled with photons.
   // From the user point of view it works like a fifo - 
   //     you can read photon frames untill the fifo is empty 
   //     ( or you reach required number of photons ). 
   //  If your photon's rate is too high or you don't read photons fast enough, 
   //    FIFO overrun can happen, 
   //  it means that photons which were not read before are overwritten with the new ones.
   //  - macro time information after the overrun is not consistent
   // The photon's rate border at which overruns can appear depends on:
   //   - module type ( fifo size ),
   //   - your experiment ( photon's rate ), 
   //   - your computer's speed, hard disk, disk cache, operating memory size
   //   - number of tasks running in the same time
   //  You can experiment using our measurement software to decide 
   //       how big memory buffer to use to read photons and when to write to hard disk
   //  To increase the border :
   //     - close all unnecessary applications
   //     - do not write to the hard disk very big amount of data in one piece - 
   //           it can slow down your measurement loop

   // buffer must be allocated for predefined max number of photons to read in one call
   // max number of photons to read in one call - hier max_ph_to_read variable
   // max_ph_to_read should also be defined carefully depending on the same aspects as 
   //     overrun considerations above
   // if it is too big - you can block (slow down) your system during reading fifo
   //    ( for high photons rates)
   // if it is too small - you can decrease your photon' rate at which overrun occurs
   //    ( by big overhead for calling DLL function)
   // user can experiment with max_ph_to_read value to get the best performance of your
   //    system

   if (module_type == M_SPC830)
      max_ph_to_read = 2000000; // big fifo, fast DMA readout
   else
      max_ph_to_read = 200000;

   if (fifo_type == FIFO_48)
      max_words_in_buf = 3 * max_ph_to_read;
   else
      max_words_in_buf = 2 * max_ph_to_read;

   if (fifo_type == FIFO_48)
      words_per_phot = 3;
   else
      words_per_phot = 2;

}
