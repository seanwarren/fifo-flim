#pragma once

#include <QObject>

#include <QFile>
#include <QDataStream>
#include <QDateTime>
#include "LZ4ThreadedStream.h"
#include "TcspcEvent.h"
#include "FifoTcspc.h"

enum FlimMetadataTag
{
   TagDouble    = 0,
   TagUInt64    = 1,
   TagInt64     = 2,
   TagBool      = 4,
   TagString    = 5,
   TagDate      = 6,
   TagEndHeader = 7
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

   void nextImageStarted();
   void imageSequenceFinished();

   void addEvent(const TcspcEvent& evt);

   bool isProcessingEvents() { return recording; }

protected:

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
   void openFile();

   void writeTag(const char* tag, double value);
   void writeTag(const char* tag, int64_t value);
   void writeTag(const char* tag, uint64_t value);
   void writeTag(const char* tag, bool value);
   void writeTag(const char* tag, const QString& value);
   void writeTag(const char* tag, QDateTime value);
   void writeEndTag();
   void writeTag(const char* tag, uint16_t type, const char* data, uint32_t length);

   FifoTcspc* tcspc = nullptr;
   std::vector<uint32_t> buffer;
   int buffer_pos = 0;
   int image_index = 0;
   bool use_compression = false;
};
