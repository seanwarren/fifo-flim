#include "FlimFileWriter.h"
#include <QStandardPaths>
#include <QFileDialog>
#include <QBuffer>

void FlimFileWriter::eventStreamAboutToStart()
{
   if (recording)
      writeFileHeader();

   running = true;

};

void FlimFileWriter::eventStreamFinished()
{
   running = false;
};


void FlimFileWriter::addEvent(const TcspcEvent& evt)
{
   if (recording)
      lz4_stream.write(reinterpret_cast<const char*>(&evt), sizeof(TcspcEvent));
}


void FlimFileWriter::writeFileHeader()
{
   // Write header
   quint32 magic_number = 0xF1F0;
   quint32 format_version = 1;
   
   header.clear();
   QBuffer buffer(&header);
   buffer.open(QIODevice::WriteOnly);
   header_stream.setDevice(&buffer);

   writeTag("CreationDate", QDateTime::currentDateTime());
   writeTag("TcspcSystem", tcspc->describe());
   writeTag("SyncRate", tcspc->getSyncRateHz());
   writeTag("Microtime_ResolutionUnit_Ps", tcspc->getMicroBaseResolutionPs());
   writeTag("Macrotime_ResolutionUnit_Ps", tcspc->getMacroBaseResolutionPs());

   buffer.close();

   quint32 header_size = header.size();
   data_stream << magic_number << format_version << header_size;
   data_stream.writeBytes(header.data(), header.size());
}



void FlimFileWriter::startRecording(const QString& specified_file_name)
{
   QString file_name = specified_file_name;

   if (recording)
      return;

   if (file_name == "")
   {
      QString folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
      folder.append("/FLIM data.spc");
      file_name = QFileDialog::getSaveFileName(nullptr, "Choose a file name", folder, "SPC file (*.spc)");
   }

   if (!file_name.isEmpty())
   {

      file.setFileName(file_name);
      file.open(QIODevice::WriteOnly);
      data_stream.setDevice(&file);
      data_stream.setByteOrder(QDataStream::LittleEndian);
      lz4_stream.setDevice(&file);

      if (running)
         writeFileHeader();

      recording = true;
   }
}

void FlimFileWriter::stopRecording()
{
   recording = false;
   data_stream.setDevice(nullptr);
   file.close();
}



void FlimFileWriter::writeTag(const char* tag, double value)
{
   writeTag(tag, TagDouble, reinterpret_cast<const char*>(&value), sizeof(double));
};

void FlimFileWriter::writeTag(const char* tag, int64_t value)
{
   writeTag(tag, TagInt64, reinterpret_cast<const char*>(&value), sizeof(int64_t));
};

void FlimFileWriter::writeTag(const char* tag, const QString& value)
{
   QByteArray ba = value.toLatin1();
   writeTag(tag, TagString, ba.data(), ba.size());
};

void FlimFileWriter::writeTag(const char* tag, QDateTime value)
{
   QByteArray ba = value.toString(Qt::ISODate).toLatin1();
   writeTag(tag, TagDate, ba.data(), ba.size());
}

void FlimFileWriter::writeTag(const char* tag, uint16_t type, const char* data, uint32_t length)
{
   header_stream << tag << type << length;
   header_stream.writeBytes(data, length);
}