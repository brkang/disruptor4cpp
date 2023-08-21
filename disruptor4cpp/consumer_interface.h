#ifndef DISRUPTOR_CONSUMER_INTERFACE_H_
#define DISRUPTOR_CONSUMER_INTERFACE_H_

#include <vector>
#include "disruptor4cpp/sequencer.h"

namespace disruptor
{
class IConsumer
{
public:
    virtual ~IConsumer() {};
    virtual bool Start() = 0;
    virtual bool DrainAndStop() = 0;
    virtual bool Stop() = 0;
    virtual std::vector<Sequence*> GetSequences() = 0;
    virtual std::string GetName() = 0;
    virtual bool IsEndOfChain() const = 0;
    virtual void UnMarkAsUsedInBarrier() = 0;
};

}

#endif