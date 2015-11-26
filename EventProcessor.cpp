#include "EventProcessor.h"
#include <QStandardPaths>
#include <QFileDialog>

void EventProcessor::start()
{
   if (recording)
      writeFileHeader();

   running = true;
   reader_thread = std::thread(&EventProcessor::readerThread, this);
   processor_thread = std::thread(&EventProcessor::processorThread, this);
}

void EventProcessor::stop()
{
   running = false;
   if (reader_thread.joinable())
      reader_thread.join();

   if (processor_thread.joinable())
      processor_thread.join();

}

void EventProcessor::writeFileHeader()
{
   // Write header
   quint32 spc_header = 0;
   quint32 magic_number = 0xF1F0;
   quint32 header_size = 4;
   quint32 format_version = 1;
   // todo...
   data_stream << magic_number << header_size << format_version; //<< n_x << n_y << spc_header;
}



void EventProcessor::startRecording(const QString& specified_file_name)
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

void EventProcessor::stopRecording()
{
   recording = false;
   data_stream.setDevice(nullptr);
   file.close();
}
