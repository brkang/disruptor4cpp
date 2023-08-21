#ifndef DISRUPTOR_EVENT_PROCESSOR_GROUP_H_
#define DISRUPTOR_EVENT_PROCESSOR_GROUP_H_

#include <vector>
#include "disruptor4cpp/consumer.h"
#include "disruptor4cpp/consumer_interface.h"
#include "disruptor4cpp/sequence.h"
#include "disruptor4cpp/disruptor.h"

namespace disruptor
{

// 临时性辅助类 构造调用关系时使用
template<typename DisruptorType>
class EventProcessorGroup
{
public:
    typedef typename DisruptorType::SequencerType SequencerType;
    typedef EventProcessorGroup<DisruptorType> EventProcessorGroupType;
    typedef typename DisruptorType::EventType EventType;

public:
    EventProcessorGroup(DisruptorType& disruptor, std::vector<std::unique_ptr<IConsumer>>& event_processor_repository, const std::vector<IConsumer*>& consumers)
        : disruptor_(disruptor), 
          event_processor_repository_(event_processor_repository),
          consumers_(consumers)
        {
        }

    EventProcessorGroupType handleEventsWith(const std::vector< std::pair<std::string, EventHandler<EventType> > > & handles)
    {
        return disruptor_.createEventProcessors(consumers_, handles);
    }

    EventProcessorGroupType handleEventsWith(const char* name, const std::vector<EventHandler<EventType>>& workHandlers)
    {
        return disruptor_.createWorkerPool(consumers_, name, workHandlers);
    }

    EventProcessorGroupType And(const EventProcessorGroupType& otherHandlerGroup)
    {
        std::vector<IConsumer*> barrier_consumers(consumers_);
        barrier_consumers.insert(barrier_consumers.end(), otherHandlerGroup.getConsumers().begin(), otherHandlerGroup.getConsumers().end());
        return EventProcessorGroupType(disruptor_, event_processor_repository_, barrier_consumers);
    }

    EventProcessorGroupType then(const std::vector< std::pair<std::string, EventHandler<EventType> > >& handles)
    {
        return handleEventsWith(handles);
    }

    EventProcessorGroupType then(const char* name, const std::vector<EventHandler<EventType>>& workHandlers)
    {
        return handleEventsWith(name, workHandlers);
    }

    const std::vector<IConsumer*>& getConsumers() const
    {
        return consumers_;
    }

private:
    DisruptorType& disruptor_;
    std::vector<std::unique_ptr<IConsumer>>& event_processor_repository_;
    std::vector<IConsumer*> consumers_;  //当前组的Consumers_
};

}

#endif