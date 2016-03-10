#pragma once
#include <cstdint>

class TcspcEvent
{
public:

   enum Mark {
      Photon = 0,
      PixelMarker = 1,
      LineStartMarker = 2,
      LineEndMarker = 4,
      FrameMarker = 8
   };

   uint16_t macro_time;
   uint16_t micro_time;

   uint8_t channel() const
   {
      return micro_time & 0xF;
   }

   uint16_t microTime() const
   {
      return micro_time >> 4;
   }

   bool isMacroTimeRollover() const 
   { 
      return (channel() == 0xF) && (macro_time == 0); 
   };

   bool isMark() const
   { 
      return (channel() == 0xF) && (macro_time != 0); 
   };

   uint8_t mark() const
   {
      return micro_time >> 4;
   };

protected:
  
   template<class T>
   T readBits(T& p, long n_bits)
   {
      T mask = (1LL << n_bits) - 1;
      T value = p & mask;
      p = p >> n_bits;

      return value;
   }

};

class TcspcEventConsumer
{
public:

   virtual void eventStreamAboutToStart() {};
   virtual void eventStreamFinished() {};
   virtual void nextImageStarted() {};
   virtual void imageSequenceFinished() {};
   virtual void addEvent(const TcspcEvent& evt) = 0;
   virtual bool isProcessingEvents() { return true; };

};
