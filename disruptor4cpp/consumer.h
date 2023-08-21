#ifndef DISRUPTOR_CONSUMER_WORKER_H_
#define DISRUPTOR_CONSUMER_WORKER_H_

#include <atomic>
#include <vector>
#include <memory>
#include "disruptor4cpp/sequence.h"
#include "disruptor4cpp/sequencer.h"
#include "disruptor4cpp/sequence_barrier.h"
#include "disruptor4cpp/wait_strategy.h"
#include "disruptor4cpp/event_processor.h"
#include "disruptor4cpp/event_handler.h"
#include "disruptor4cpp/consumer_interface.h"

namespace disruptor
{
template<typename SequencerType>
class Worker : public IConsumer
{
public:
    typedef typename SequencerType::EventType T;
    typedef typename SequencerType::WaitType W;
    typedef EventProcessor<SequencerType> EventProcessorType;

public:
    Worker(SequencerType& sequencer, const char* name, const EventHandler<T>& handle_call_back, const std::vector<Sequence*>& dependents = {} )
        :sequencer_(sequencer), name_(name)
    {
        event_processor_.reset(new EventProcessorType(sequencer_, handle_call_back, dependents));
    };

    Worker(const Worker&) = delete;
    Worker operator=(Worker &&other) = delete;

    ~Worker() 
    {
        if (running_)
            Stop();
    };

    virtual bool Start() override
    {
        running_ = true;
        started_.store(false, std::memory_order_seq_cst);
        event_processor_->notifyOnStart([=](){ 
            this->started_.store(true, std::memory_order_seq_cst); 
        });

        threads_.reset(new std::thread( std::bind(&EventProcessorType::Run, event_processor_.get()) ));
        while(!started_.load(std::memory_order_seq_cst)){
            std::this_thread::yield();
        }

        return true;
    }

    virtual bool DrainAndStop() override
    {
        while (sequencer_.GetCursor() > event_processor_->sequence()->sequence() ){
            std::this_thread::yield();
        }
        return Stop();
    }

    virtual bool Stop() override
    {
        if(running_)
        {
            running_ = false;
            event_processor_->halt();
            if(threads_->joinable())
                threads_->join();
        }
        return true;
    }

    virtual std::vector<Sequence*> GetSequences() override
    {
        return { event_processor_->sequence() };
    }

    virtual std::string GetName() override
    {
        return name_;
    }

    virtual bool IsEndOfChain() const override { return end_of_chain_; }
    virtual void UnMarkAsUsedInBarrier() override { end_of_chain_ = false; };

private:
    SequencerType& sequencer_;
    std::string name_;
    std::atomic<bool> started_ = false;

    std::atomic<bool> running_ = false;
    std::unique_ptr<EventProcessorType> event_processor_;
    std::unique_ptr<std::thread> threads_;

    bool end_of_chain_ = true;
};

}

#endif