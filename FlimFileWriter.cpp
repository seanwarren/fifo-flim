#include "FlimFileWriter.h"
#include <QStandardPaths>
#include <QFileDialog>
#include <QBuffer>
#include <QMessageBox>

void FlimFileWriter::eventStreamAboutToStart()
{
   running = true;
   buffer.resize(1024); // buffer size must be multiple of 2!
};

void FlimFileWriter::eventStreamFinished()
{
   running = false;
};

void FlimFileWriter::nextImageStarted()
{
   image_index++;
   openFile();
}

void FlimFileWriter::imageSequenceFinished()
{
   if (file.isOpen())
      file.close();

   recording = false;
   file_name = "";
}

void FlimFileWriter::addEvent(const TcspcEvent& evt)
{
   if (recording && (image_index > 0))
   {
      data_stream.writeRawData(reinterpret_cast<const char*>(&evt.macro_time), sizeof(evt.macro_time));
      data_stream.writeRawData(reinterpret_cast<const char*>(&evt.micro_time), sizeof(evt.micro_time));
   }
} 


void FlimFileWriter::writeFileHeader()
{
   // Write header
   quint32 magic_number = 0xF1F0;
   quint32 format_version = 2;
   
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
   writeTag("UsingPixelMarkers", tcspc->usingPixelMarkers());

   for(auto&& m : metadata)
      writeTag(m.first, m.second);
   
   writeEndTag();

   buffer.close();

   quint32 header_size = header.size();
   data_stream << magic_number << format_version << (header_size + 12); // + 12 for first three numbers 
   data_stream.writeRawData(header.data(), header.size());
}



void FlimFileWriter::startRecording(const QString& specified_file_name)
{
   if (recording)
      return;

   file_name = specified_file_name;

   if (file_name == "")
   {
      QString folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
      folder.append("/FLIM data.ffd");
      file_name = QFileDialog::getSaveFileName(nullptr, "Choose a file name", folder, "FFD file (*.ffd)");
   }

   recording = true;
   image_index = 0;
}

void FlimFileWriter::openFile()
{
   if (file.isOpen())
      file.close();

   QString new_ext = QString("_%1.ffd").arg(image_index, 3, 10, QChar('0'));
   QString file_name_with_number = file_name;
   file_name_with_number.replace(".ffd", new_ext);

   std::string fn = file_name_with_number.toStdString();
   
   file.setFileName(file_name_with_number);
   bool success = file.open(QIODevice::WriteOnly);

   if (!success)
      emit error("Could not open file!");

   data_stream.setDevice(&file);
   data_stream.setByteOrder(QDataStream::LittleEndian);

   writeFileHeader();
}



void FlimFileWriter::stopRecording()
{
   recording = false;

   data_stream.setDevice(nullptr);

   if (file.size() == 0)
      emit error("Written file is empty");
   
   file.close();
}

void FlimFileWriter::writeTag(const QString& tag_string, const QVariant& value)
{
   const char* tag = tag_string.toLatin1();

   QVariant::Type type = value.type();

   if (type == QVariant::Type::Double)
      writeTag(tag, value.toDouble());
   else if (type == QVariant::Type::Int)
      writeTag(tag, (int64_t) value.toLongLong());
   else if (type == QVariant::Type::UInt)
      writeTag(tag, (uint64_t) value.toULongLong());
   else if (type == QVariant::Type::Bool)
      writeTag(tag, value.toBool());
   else if (type == QVariant::Type::String)
      writeTag(tag, value.toString());
   else if (type == QVariant::Type::DateTime || type == QVariant::Type::Time)
      writeTag(tag, value.toDateTime());
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
