#ifndef DISRUPTOR_EVENTHANDLER_H_
#define DISRUPTOR_EVENTHANDLER_H_

#include <functional>
#include <string>

namespace disruptor
{

template <class T>
class EventHandler
{
public:
    typedef std::function<void(T&, int64_t, bool)> HandleCallBack;

public:
    EventHandler(HandleCallBack handle)
      : handler_(handle)
      {}

    /**
      * Called when a publisher has committed an event to the RingBuffer<T>
      * 
      * \param data Data committed to the RingBuffer<T>
      * \param sequence Sequence number committed to the RingBuffer<T>
      * \param endOfBatch flag to indicate if this is the last event in a batch from the RingBuffer<T>
      */
    void onEvent(T& data, std::int64_t sequence, bool endOfBatch)
    {
        handler_(data, sequence, endOfBatch);
    }

private:
  HandleCallBack handler_;

};

}

#endif