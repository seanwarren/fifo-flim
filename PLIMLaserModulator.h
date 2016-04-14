#pragma once

#include "AbstractArduinoDevice.h"
#include <QString>

class PLIMLaserModulator : public AbstractArduinoDevice
{
   Q_OBJECT

   enum Messages {
      MSG_SET_MODULATION = 0x01,
      MSG_SET_NUM_PIXELS = 0x02
   };

public:

   PLIMLaserModulator(QObject *parent = nullptr, QThread* thread = nullptr) :
      AbstractArduinoDevice(parent, thread)

   {
      startThread();
   }

   void setModulation(bool modulation_active_)
   {
      modulation_active = modulation_active_;
      sendMessage(MSG_SET_MODULATION, (uint) modulation_active);
   }

   void setNumPixels(uint n_pixels_)
   {
      if (n_pixels_ == 0)
         return;

      n_pixels = n_pixels_;
      sendMessage(MSG_SET_NUM_PIXELS, n_pixels);
   }

   bool getModulation() { return modulation_active; }
   int getNumPixels() { return n_pixels; }

private:
   
   void setupAfterConnection() 
   {
      setNumPixels(n_pixels);
      setModulation(modulation_active);
   }

   void processMessage(const char message, uint32_t param, QByteArray& payload) {}; // no expected messages
   const QString getExpectedIdentifier() { return "PLIM Laser Modulator"; };


   uint n_pixels = 128;
   bool modulation_active = false;
};