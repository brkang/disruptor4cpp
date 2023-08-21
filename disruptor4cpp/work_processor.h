#ifndef DISRUPTOR_WORKPROCESSER_H_
#define DISRUPTOR_WORKPROCESSER_H_

#include <functional>
#include "disruptor4cpp/sequence.h"
#include "disruptor4cpp/sequencer.h"
#include "disruptor4cpp/sequence_barrier.h"
#include "disruptor4cpp/wait_strategy.h"
#include "disruptor4cpp/work_handler.h"
#include "disruptor4cpp/event_handler.h"

namespace disruptor
{
template<typename T, typename W, typename SequencerType>
class WorkProcessor
{
public:
    typedef std::function<void()> NotifyCallBackType;

public:
    WorkProcessor(Sequence& work_sequence, 
                  SequencerType& sequencer, 
                  SequenceBarrier<W>& sequence_barrier, 
                  const EventHandler<T>& handle_call_back) 
        : sequencer_(sequencer), 
        work_sequence_(work_sequence),
        sequence_barrier_(sequence_barrier),
        sequence_(),
        work_handler_(handle_call_back)
    {
        sequence_.set_sequence(work_sequence_.sequence());
    };

public:
    Sequence* sequence()
    {
        return &sequence_;
    }

    void notifyOnStart(NotifyCallBackType on_start) {
        on_start_ = on_start;
    }

    void notifyOnShutdown(NotifyCallBackType on_shutdown) {
        on_shutdown_ = on_shutdown;
    }

private:
    SequencerType& sequencer_;
    Sequence& work_sequence_; //work不能超过生产者
    SequenceBarrier<W>& sequence_barrier_;  //多个消费者共享一个Barrier，唤醒notify_all

    Sequence sequence_; //添加到生产者gate中，生产者依赖进度最慢消费者
    EventHandler<T> work_handler_;

    NotifyCallBackType on_start_;
    NotifyCallBackType on_shutdown_;

public:
    void Run()
    {
        if(on_start_){ on_start_(); }

        //生产者较快时减少WaitFor操作
        auto cached_available_sequence = std::numeric_limits<std::int64_t>::min();
        auto next_sequence = sequence_.sequence();
        //WaitFor被唤醒后需要先handle_call_back_继续处理
        auto processed_sequence = true; 

        while (true)
        {
            if(processed_sequence)
            {
                processed_sequence = false;
                // do {
                //     next_sequence = work_sequence_.sequence() + 1L;
                //     sequence_.set_sequence(next_sequence - 1L);
                // } while (!work_sequence_.CompareAndSet(next_sequence - 1L, next_sequence));
                next_sequence = work_sequence_.IncrementAndGet();
                sequence_.set_sequence(next_sequence - 1L);
            }
            if (cached_available_sequence >= next_sequence)
            {
                work_handler_.onEvent(sequencer_[next_sequence], next_sequence, next_sequence == cached_available_sequence); //call back
                processed_sequence = true;
            }
            else
            {
                cached_available_sequence = sequence_barrier_.WaitFor(next_sequence);
                if(cached_available_sequence < 0)
                    break;
            }
        }

        if(on_shutdown_){ on_shutdown_(); }
    }
};
}

#endif
