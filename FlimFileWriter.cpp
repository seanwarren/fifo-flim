#include "FlimFileWriter.h"
#include <QStandardPaths>
#include <QFileDialog>
#include <QBuffer>

void FlimFileWriter::eventStreamAboutToStart()
{
   if (recording)
      writeFileHeader();

   running = true;
   buffer.resize(1024);
};

void FlimFileWriter::eventStreamFinished()
{
   running = false;
};


void FlimFileWriter::addEvent(const TcspcEvent& evt)
{
   if (recording)
   {
      data_stream.writeRawData(reinterpret_cast<const char*>(&evt), sizeof(evt));
      /*
      buffer[buffer_pos++] = evt;
      if (buffer_pos == buffer.size())
      {
         lz4_stream.write(reinterpret_cast<const char*>(buffer.data()), sizeof(TcspcEvent)*buffer.size());
         buffer_pos = 0;
      }
      */
   }
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
   header_stream.setByteOrder(QDataStream::LittleEndian);

   writeTag("CreationDate", QDateTime::currentDateTime());
   writeTag("TcspcSystem", tcspc->describe());
   writeTag("SyncRate_Hz", tcspc->getSyncRateHz());
   writeTag("NumTimeBins", (int64_t) tcspc->getNumTimebins());
   writeTag("NumChannels", (int64_t) tcspc->getNumChannels());
   writeTag("MicrotimeResolutionUnit_ps", tcspc->getMicroBaseResolutionPs());
   writeTag("MacrotimeResolutionUnit_ps", tcspc->getMacroBaseResolutionPs());
   writeTag("L4ZCompression", use_compression);
   writeTag("L4ZMessageSize", lz4_stream.getMessageSize());
   writeEndTag();

   buffer.close();

   quint32 header_size = header.size();
   data_stream << magic_number << format_version << (header_size + 12); // + 12 for first three numbers 
   data_stream.writeRawData(header.data(), header.size());
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

//   lz4_stream.close();
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

void FlimFileWriter::writeTag(const char* tag, uint64_t value)
{
   writeTag(tag, TagUInt64, reinterpret_cast<const char*>(&value), sizeof(uint64_t));
};

void FlimFileWriter::writeTag(const char* tag, bool value)
{
   writeTag(tag, TagBool, reinterpret_cast<const char*>(&value), sizeof(bool));
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

void FlimFileWriter::writeEndTag()
{
   writeTag("EndHeader", TagEndHeader, nullptr, 0);
}

void FlimFileWriter::writeTag(const char* tag, uint16_t type, const char* data, uint32_t length)
{
   header_stream << tag << type << length;
   if (length > 0)
      header_stream.writeRawData(data, length);
}