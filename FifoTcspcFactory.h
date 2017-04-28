#pragma once

#include "FifoTcspc.h"
#include "SimTcspc.h"
#include <QString>

#ifdef USE_CRONOLOGIC
#include "cronologic.h"
#endif
#ifdef USE_BECKERHICKL
#include "bh.h"
#endif

class FifoTcspcFactory
{
public:
   static FifoTcspc* create(const QString& type, QObject* parent = nullptr)
   {
      if (type.toLower() == "sim")
      {
         return new SimTcspc(parent);
      }

      if (type.toLower() == "cronologic")
      {
#ifdef USE_CRONOLOGIC
         return new Cronologic(parent);
#endif
         throw(std::runtime_error("Cronologic support was not compiled"));
      }
      if (type.toLower() == "bh")
      {
#ifdef USE_BECKERHICKL
         return new BH(parent);
#endif
         throw(std::runtime_error("Becker&Hickl support was not compiled"));
      }

      throw(std::runtime_error("Unsupported TCSPC type requested"));
   }
};

