#pragma once

#include "TimeTagger4_interface.h"
#include "ImageSource.h"
#include "FLIMage.h"
#include "FifoTcspc.h"

class Chronologic : public FifoTcspc
{
	Q_OBJECT

public:
   Chronologic(QObject* parent);
   ~Chronologic();

	void Init();
	void SaveSDT(FLIMage& image, const QString& filename);

private:

   void CheckCard();
   void ConfigureCard();

   void StartModule();
   void ConfigureModule();

   void WriteFileHeader();
   void SetSyncThreshold(float threshold);
   float GetSyncThreshold();

   void ReaderThread();

   bool ReadPackets(); // return whether there are more photons to read
   void ReadRemainingPhotonsFromStream();

   timetagger4_device* device;

   PacketBuffer<crono_packet> packet_buffer;
};