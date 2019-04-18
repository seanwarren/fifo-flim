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
      FrameMarker = 8,
      ImageMarker = 16
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
      return (micro_time == 0xF); 
   };

   bool isMark() const
   { 
      return (channel() == 0xF); 
   };

   uint8_t mark() const
   {
      return micro_time >> 4;
   };

   void addMark(Mark mark_)
   {
      micro_time |= (mark_ << 4);
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
