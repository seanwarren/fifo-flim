#pragma once

#include <QObject>
#include <vector>
#include <list>
#include <memory>
#include <opencv2/core.hpp>
#include <mutex>
class FlimDataSourceWatcher;

class FlimDataSource : public QObject
{
   Q_OBJECT

public:

   FlimDataSource(QObject* parent = nullptr);

   virtual int getNumChannels() { return 1; }
   virtual double getTimeResolution() = 0;

   virtual cv::Mat getIntensity() = 0;
   virtual cv::Mat getMeanArrivalTime() = 0;

//   virtual std::list<std::vector<quint16>>& getHistogramData() = 0;
   virtual std::vector<uint>& getCurrentDecay(int channel) = 0;
   virtual std::vector<double>& getCountRates() = 0;
   virtual std::vector<double>& getMaxInstantCountRates() = 0;

   void registerWatcher(FlimDataSourceWatcher* watcher);
   void unregisterWatcher(FlimDataSourceWatcher* watcher);
   void requestDelete();

signals:
   void decayUpdated();
   void countRatesUpdated();
   void readComplete();

private:

   std::list<FlimDataSourceWatcher*> watchers;
   std::mutex m;
};

class FlimDataSourceWatcher
{
public:

   virtual ~FlimDataSourceWatcher()
   {
      if (source != nullptr)
         source->unregisterWatcher(this);
   }

   void setFlimDataSource(std::shared_ptr<FlimDataSource> source_)
   {
      source = source_;
      source->registerWatcher(this);
   }

   virtual void sourceDeleteRequested() = 0;

protected:
   std::shared_ptr<FlimDataSource> source;
};
