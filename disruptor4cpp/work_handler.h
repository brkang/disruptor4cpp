#ifndef DISRUPTOR_WORKHANDLER_H_
#define DISRUPTOR_WORKHANDLER_H_

#include <functional>

namespace disruptor
{

template <class T>
class WorkHandler
{
public:
  typedef std::function<void(T&)> HandleCallBack;

public:
    WorkHandler(HandleCallBack& handle)
      : handler_(handle)
      {}

    /**
      * Called when a publisher has committed an event to the RingBuffer<T>
      * 
      * \param data Data committed to the RingBuffer<T>
      */
    void onEvent(T& data)
    {
        handler_(data);
    }

private:
  HandleCallBack handler_;
};

}

#endif