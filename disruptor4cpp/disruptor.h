
#ifndef DISRUPTOR_DISRUPTOR_H_ // NOLINT
#define DISRUPTOR_DISRUPTOR_H_ // NOLINT

#include <array>
#include <vector>
#include <string>
#include <unordered_map>

#include "disruptor4cpp/sequencer.h"
#include "disruptor4cpp/wait_strategy.h"
#include "disruptor4cpp/claim_strategy.h"
#include "disruptor4cpp/event_processor.h"
#include "disruptor4cpp/work_processor.h"
#include "disruptor4cpp/consumer.h"
#include "disruptor4cpp/consumer_work_pool.h"
#include "disruptor4cpp/ring_buffer.h"
#include "disruptor4cpp/event_handler.h"
#include "disruptor4cpp/work_handler.h"
#include "disruptor4cpp/event_processor_group.h"

namespace disruptor
{
    template <typename T, size_t N = kDefaultRingBufferSize,
              typename C = kDefaultClaimStrategy, typename W = kDefaultWaitStrategy>
    class Disruptor
    {
    public:
        typedef Sequencer<T, N, C, W> SequencerType;
        typedef EventProcessorGroup<Disruptor<T, N, C, W>> EventProcessorGroupType;
        typedef typename SequencerType::EventType EventType;

    public:
        Disruptor(/*const std::array<T, N> &events*/)
            : sequencer_(/*events*/)
        {
        }

        // 添加并行消费者，每一个EventHandler都是独立的消费者。  这些消费者是并行关系，彼此无依赖的。
        // 消息到来时最先处理的消费者
        EventProcessorGroupType handlesWith(const std::vector< std::pair<std::string, EventHandler<T> > > & handles)
        {
            std::vector<IConsumer*> barrierConsumers;
            return createEventProcessors(barrierConsumers, handles);
        }

        EventProcessorGroupType handlesWith(const char* name, const std::vector<EventHandler<T>>& workHandlers)
        {
            std::vector<IConsumer*> barrierConsumers;
            return createWorkerPool(barrierConsumers, name, workHandlers);
        }

        // P1 -> RB -> C1 -> C2 C2为最慢的消费者 gating_sequences为此即可
        EventProcessorGroupType after(const std::vector<std::string>& handle_names)
        {
            std::vector<IConsumer *> barrierConsumers;
            for (auto &name : handle_names)
            {
                auto iter = cosumer_name_map_.find(name);
                if (iter != cosumer_name_map_.end())
                {
                    barrierConsumers.push_back(iter->second);
                }
                else
                    throw "this handle not exists!";
            }
            return EventProcessorGroupType(*this, event_processor_repository_, barrierConsumers);
        }

        //FIXME 批量接口
        void publishEvents(const T& event)
        {
            if(stop_)
                return;

            int64_t seq = sequencer_.Claim(1);
            sequencer_[seq] = event;
            sequencer_.Publish(seq, 1);
        }

        void start()
        {
            // 设置gate 启动消费者
            std::vector<Sequence *> dependents;
            for (auto &event_processor : event_processor_repository_)
            {
                if (event_processor->IsEndOfChain())
                {
                    auto sequences = event_processor->GetSequences();
                    dependents.insert(dependents.end(), sequences.begin(), sequences.end());
                    // dependents.push_back(event_processor->GetSequences());
                }
            }
            sequencer_.set_gating_sequences(dependents);

            for (size_t i = 0; i < event_processor_repository_.size(); i++)
            {
                event_processor_repository_[i]->Start();
            }
            stop_ = false;
        }

        void stop()
        {
            stop_ = true;
            for (size_t i = 0; i < event_processor_repository_.size(); i++)
            {
                event_processor_repository_[i]->Stop();
            }
        }

        void drainAndStop()
        {
            stop_ = true;
            for (size_t i = 0; i < event_processor_repository_.size(); i++)
            {
                event_processor_repository_[i]->DrainAndStop();
            }
        }
    
    private:
        EventProcessorGroupType createEventProcessors(const std::vector<IConsumer*>& barrierConsumers,
                                                      const std::vector< std::pair<std::string, EventHandler<T> > > & eventHandlers)
        {
            std::vector<IConsumer *> processorConsumers;
            processorConsumers.reserve(eventHandlers.size());

            std::vector<Sequence*> barrierSequences;  //IConsumer装换为Sequence 统一work与work_pool
            for (auto consumerPtr : barrierConsumers)
            {
                auto sequences = consumerPtr->GetSequences();
                barrierSequences.insert(barrierSequences.end(), sequences.begin(), sequences.end());
            }

            for (auto &eventHandler : eventHandlers)
            {
                IConsumer* new_consumer = new Worker<SequencerType>(sequencer_, eventHandler.first.c_str(), eventHandler.second, barrierSequences);
                event_processor_repository_.emplace_back(new_consumer);
                auto &eventProcessor = event_processor_repository_.back();

                //handle名称保存下 用于after操作
                cosumer_name_map_[eventHandler.first] = eventProcessor.get();
                processorConsumers.push_back(eventProcessor.get());
            }

            if (!processorConsumers.empty())  // barrierConsumers标记为不是调用链的末尾 用于set_gating
            {
                for (auto consumerPtr : barrierConsumers)
                {
                    consumerPtr->UnMarkAsUsedInBarrier();
                }
            }
            return EventProcessorGroupType(*this, event_processor_repository_, processorConsumers);
        }

        EventProcessorGroupType createWorkerPool(const std::vector<IConsumer*>& barrierConsumers,
                                                 const std::string name,
                                                 const std::vector<EventHandler<T>>& workHandlers)
        {
            std::vector<Sequence*> barrierSequences;  //IConsumer装换为Sequence 统一work与work_pool
            for (auto consumerPtr : barrierConsumers)
            {
                auto sequences = consumerPtr->GetSequences();
                barrierSequences.insert(barrierSequences.end(), sequences.begin(), sequences.end());
            }
            //创建WorkerPool
            IConsumer* new_consumer = new WorkerPool<SequencerType>(sequencer_, name.c_str(), workHandlers, barrierSequences);
            event_processor_repository_.emplace_back(new_consumer);
            auto &workPool = event_processor_repository_.back();

            //handle名称保存下 用于after操作
            cosumer_name_map_[name] = workPool.get();

            std::vector<IConsumer *> processorConsumers;
            processorConsumers.push_back(workPool.get());

            if (!processorConsumers.empty())  // barrierConsumers标记为不是调用链的末尾 用于set_gating
            {
                for (auto consumerPtr : barrierConsumers)
                {
                    consumerPtr->UnMarkAsUsedInBarrier();
                }
            }

            return EventProcessorGroupType(*this, event_processor_repository_, processorConsumers);
        }

    private:
        SequencerType sequencer_;
        //保存所有消费者
        std::vector<std::unique_ptr<IConsumer>> event_processor_repository_;
        //名称与消费者对应关系 由于任务顺序组装
        std::unordered_map<std::string, IConsumer*> cosumer_name_map_;

        friend class EventProcessorGroup<Disruptor<T, N, C, W>>;

        std::atomic<bool> stop_ = false;
    };

} // namespace disruptor

#endif
/*

handles->Group1
handles->Group2
Group1->after()

P1 RB1 C1、C2 -> C3
P1 RB1 C3 -> C1、C2
P1 RB1 C1 -> C3
       C2
name -> consumer
*/