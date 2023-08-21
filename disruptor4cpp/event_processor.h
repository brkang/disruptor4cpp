#ifndef DISRUPTOR_EVENTPROCESSER_H_
#define DISRUPTOR_EVENTPROCESSER_H_

#include <functional>
#include "disruptor4cpp/sequence.h"
#include "disruptor4cpp/sequencer.h"
#include "disruptor4cpp/sequence_barrier.h"
#include "disruptor4cpp/wait_strategy.h"
#include "disruptor4cpp/event_handler.h"

/*
每个线程处理一个EventProcessor
*/

namespace disruptor
{
template<typename SequencerType>
class EventProcessor
{
public:
    typedef std::function<void()> NotifyCallBackType;
    typedef typename SequencerType::EventType T;
    typedef typename SequencerType::WaitType W;
public:
    EventProcessor(SequencerType& sequencer, EventHandler<T> event_handler,
                   const std::vector<Sequence*>& dependents = {} ) 
        : sequencer_(sequencer),
        event_handler_(event_handler)
    {
        sequence_barrier_.reset(sequencer_.NewBarrier(dependents));
    };

    EventProcessor(const EventProcessor&) = delete;
    EventProcessor operator=(EventProcessor &&other) = delete;

public:
    Sequence* sequence()
    {
        return &sequence_;
    }

    bool halt()
    {
        sequence_barrier_->set_alerted(true);
        return true;
    }

    void notifyOnStart(NotifyCallBackType on_start) {
        on_start_ = on_start;
    }

    void notifyOnShutdown(NotifyCallBackType on_shutdown) {
        on_shutdown_ = on_shutdown;
    }

    void Run()
    {
        if(on_start_){ on_start_(); }

        auto cached_available_sequence = std::numeric_limits<std::int64_t>::min();
        auto next_sequence = sequence_.sequence() + 1;

        while (true)
        {
            cached_available_sequence = sequence_barrier_->WaitFor(next_sequence);
            if (kAlertedSignal == cached_available_sequence) 
                break;
            if (kTimeoutSignal == cached_available_sequence)
                continue;

            while (next_sequence <= cached_available_sequence) {
                event_handler_.onEvent(sequencer_[next_sequence], next_sequence, next_sequence == cached_available_sequence); //call back
                next_sequence++;
                // sequence_.set_sequence(next_sequence - 1); //处理完一个马上通知下游
            }
            sequence_.set_sequence(next_sequence - 1); //批量处理完通知下游
        }

        if(on_shutdown_){ on_shutdown_(); }
    }

private:
    SequencerType& sequencer_;
    
    Sequence sequence_;
    std::unique_ptr<SequenceBarrier<W>> sequence_barrier_;
    EventHandler<T> event_handler_;

    NotifyCallBackType on_start_;
    NotifyCallBackType on_shutdown_;
};

}

#endif