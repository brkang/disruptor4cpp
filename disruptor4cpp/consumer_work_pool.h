#ifndef DISRUPTOR_CONSUMER_WORKPOOL_H_
#define DISRUPTOR_CONSUMER_WORKPOOL_H_

#include <atomic>
#include <vector>
#include <memory>
#include <unordered_map>

#include "disruptor4cpp/sequence.h"
#include "disruptor4cpp/sequencer.h"
#include "disruptor4cpp/sequence_barrier.h"
#include "disruptor4cpp/wait_strategy.h"
#include "disruptor4cpp/work_processor.h"
#include "disruptor4cpp/consumer_interface.h"

namespace disruptor
{
template<typename SequencerType>
class WorkerPool : public IConsumer
{
public:
    typedef typename SequencerType::EventType T;
    typedef typename SequencerType::WaitType W;
    typedef WorkProcessor<T, W, SequencerType> WorkProcessorType;

public:
    WorkerPool(SequencerType& sequencer, const char* name, const std::vector<EventHandler<T>>& handle_call_backs, const std::vector<Sequence*> dependents = {} )
        :sequencer_(sequencer), name_(name)
    {
        work_sequence_.set_sequence(sequencer_.GetCursor());
        sequence_barrier_.reset(sequencer_.NewBarrier(dependents));

        for (size_t i = 0; i < handle_call_backs.size(); i++)
        {
            work_processors_.push_back(std::unique_ptr<WorkProcessorType>( new WorkProcessorType(work_sequence_, sequencer_, *sequence_barrier_, handle_call_backs[i]) ));
        }
    };

    ~WorkerPool() 
    {
        if (running_)
            Stop();
    };

    virtual bool Start() override
    {
        running_ = true;
        for (size_t i = 0; i < work_processors_.size(); i++)
        {
            threads_.push_back(std::unique_ptr<std::thread>(new std::thread( std::bind(&WorkProcessorType::Run, work_processors_[i].get()) )));

            std::atomic<bool> started = false;
            work_processors_[i]->notifyOnStart([&started](){ started = true; });
            while(!started)
                std::this_thread::yield();
        }
        return true;
    }

    virtual bool DrainAndStop() override
    {
        auto works = GetSequences();
        while (sequencer_.GetCursor() > GetMinimumSequence(works)){
            std::this_thread::yield();
        }
        return Stop();
    }

    virtual bool Stop() override
    {
        if(running_)
        {
            running_ = false;
            sequence_barrier_->set_alerted(true);
            for_each(threads_.begin(), threads_.end(), std::bind(&std::thread::join, std::placeholders::_1));
            threads_.clear();
        }
        return true;
    }

    virtual std::vector<Sequence*> GetSequences() override
    {
        std::vector<Sequence*> sequences(work_processors_.size());
        for (auto i = 0u; i < work_processors_.size(); ++i)
        {
            sequences[i] = work_processors_[i]->sequence();
        }
        // sequences永远比work_sequence慢，不需要增加
        // sequences[sequences.size() - 1] = &work_sequence_;
        return sequences;
    }

    //disruptor类使用
    virtual std::string GetName() override
    {
        return name_;
    }

    virtual bool IsEndOfChain() const override { return end_of_chain_; }
    virtual void UnMarkAsUsedInBarrier() override { end_of_chain_ = false; };

private:
    SequencerType& sequencer_;
    std::string name_;

    std::atomic<bool> running_ = false;
    std::vector< std::unique_ptr<WorkProcessorType> > work_processors_;
    std::vector< std::unique_ptr<std::thread> > threads_;

    Sequence work_sequence_;
    std::unique_ptr<SequenceBarrier<W>> sequence_barrier_;

    bool end_of_chain_ = true;
};

}

#endif