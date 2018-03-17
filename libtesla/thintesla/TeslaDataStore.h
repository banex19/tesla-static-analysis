#pragma once
#include "TeslaAssertion.h"

class TeslaSiteStore
{
  public:
    TeslaSiteStore(size_t numArgs)
        : numArgs(numArgs)
    {
    }

    virtual bool Match(void *args) = 0;

  protected:
    size_t numArgs;
};

class TeslaSitePerfectStore : public TeslaSiteStore
{
  public:
    TeslaSitePerfectStore(size_t numArgs)
        : TeslaSiteStore(numArgs)
    {
    }
};

class TeslaDataStore
{
  public:
    TeslaDataStore(TeslaSite maxSite);

    bool Match(TeslaSite site);

  private:
};