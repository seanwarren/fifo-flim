#include "FlimDataSource.h"

FlimDataSource::FlimDataSource(QObject* parent) :
   QObject(parent)
{

}

void FlimDataSource::registerWatcher(FlimDataSourceWatcher* watcher)
{
   std::lock_guard<std::mutex> lk(m);
   watchers.push_back(watcher);
}

void FlimDataSource::unregisterWatcher(FlimDataSourceWatcher* watcher)
{
   std::lock_guard<std::mutex> lk(m);
   watchers.remove(watcher);
}

void FlimDataSource::requestDelete()
{
   std::lock_guard<std::mutex> lk(m);
   for (auto& watcher : watchers)
      watcher->sourceDeleteRequested();
}
