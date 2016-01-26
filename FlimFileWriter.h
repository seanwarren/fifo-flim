#pragma once

#include <QObject>

#include <QFile>
#include <QDataStream>
#include <QDateTime>
#include "LZ4Stream.h"
#include "TcspcEvent.h"
#include "FifoTcspc.h"

enum FlimMetadataTag
{
   TagDouble,
   TagInt64,
   TagString,
   TagDate
};

class FlimFileWriter : public QObject, public TcspcEventConsumer
{
   Q_OBJECT

public:

   FlimFileWriter(QObject* parent = nullptr) {}

   void setFifoTcspc(FifoTcspc* tcspc_) { tcspc = tcspc_; }

   void startRecording(const QString& filename = "");
   void stopRecording();
   bool isRecording() { return recording; }

   void eventStreamAboutToStart();
   void eventStreamFinished();

   void addEvent(const TcspcEvent& evt);

   bool isProcessingEvents() { return recording; }

public:

   QString folder;
   QString file_name;
   QFile file;
   QDataStream data_stream;
   QByteArray header;
   QDataStream header_stream;
   LZ4Stream lz4_stream;

   bool recording = false;
   bool running = false;

   void writeFileHeader();

   void writeTag(const char* tag, double value);
   void writeTag(const char* tag, int64_t value);
   void writeTag(const char* tag, const QString& value);
   void writeTag(const char* tag, QDateTime value);
   void writeTag(const char* tag, uint16_t type, const char* data, uint32_t length);

   FifoTcspc* tcspc = nullptr;
};
