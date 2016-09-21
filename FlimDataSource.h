#pragma once

#include <QObject>
#include <vector>
#include <list>
#include <opencv2/core.hpp>

class FlimDataSource : public QObject
{
   Q_OBJECT

public:

   FlimDataSource(QObject* parent) : 
      QObject(parent)
   {

   }

   virtual int getNumChannels() { return 1; }
   virtual double getTimeResolution() = 0;

   virtual cv::Mat getIntensity() = 0;
   virtual cv::Mat getMeanArrivalTime() = 0;

//   virtual std::list<std::vector<quint16>>& getHistogramData() = 0;
   virtual std::vector<uint>& getCurrentDecay(int channel) = 0;
   virtual std::vector<double>& getCountRates() = 0;
   virtual std::vector<double>& getMaxInstantCountRates() = 0;


signals:
   void decayUpdated();
   void countRatesUpdated();

};