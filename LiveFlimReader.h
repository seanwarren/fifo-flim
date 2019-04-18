#pragma once

#include <QObject>
#include <QTime>
#include <QTimer>
#include <opencv2/core.hpp>
#include <iostream>
#include <fstream>
#include <mutex>
#include <queue>

#include "TcspcEvent.h"
#include "AbstractFifoReader.h"
#include "FifoTcspc.h"

class LiveEventReader : public AbstractEventReader
{
public:

   LiveEventReader();
   void addEvent(const TcspcEvent& evt);

   // Event reader functions
   double getProgress();
   bool hasMoreData();
   std::tuple<FifoEvent, uint64_t> getRawEvent();

   void setEventStreamAboutToStart();
   void setEventStreamFinished();

protected:

   bool has_more_data = true;
   std::vector<char> block;
   std::vector<char>::iterator writer_block_it;
};


class LiveFlimReader : public QObject, public TcspcEventConsumer, public AbstractFifoReader
{
   Q_OBJECT

public:

   LiveFlimReader(const TcspcAcquisitionParameters& params, QObject* parent = 0);

   //void setFrameAccumulation(int frame_accumulation_) { frame_accumulation = frame_accumulation_; }


   // TCSPC event consumer functions
   void addEvent(const TcspcEvent& evt);
   void eventStreamAboutToStart() { live_event_reader->setEventStreamAboutToStart(); };
   void eventStreamFinished() { live_event_reader->setEventStreamFinished(); };
   void nextImageStarted() {};
   void imageSequenceFinished() {};
   bool isProcessingEvents() { return true; };

   void setImageSize(int n_x, int n_y);
   
protected:

   std::shared_ptr<LiveEventReader> live_event_reader;
   TcspcAcquisitionParameters params;

   bool active = false;

   std::vector<quint16> cur_histogram;
   std::list<std::vector<uint16_t>> image_histograms;

   std::vector<std::vector<uint>> decay;
   std::vector<std::vector<uint>> next_decay;

   const int refresh_time_ms = 1000;

   bool using_pixel_markers = false;
   bool bidirectional = false;

   int cur_dir = +1;

   std::mutex cv_mutex;
   std::mutex decay_mutex;

   QTimer* timer;
};