// Disruptor.cpp
#include <chrono>
#include <vector>
#include <atomic>
#include <array>

#include "disruptor4cpp/disruptor.h"

using namespace disruptor;

#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
pid_t gettid()
{
	return syscall(SYS_gettid);
}

#define RING_BUFFER_SIZE 1024 * 256
#define BATCH_DELTA 64
#define NUMBER_OF_EVENTS_TO_PUBLISH  BATCH_DELTA * 1024 * 256
#define YieldCount 10
// typedef BusySpinStrategy WaitStrategy;
// typedef SleepingStrategy<YieldCount> WaitStrategy;
// typedef BlockingStrategy WaitStrategy;
typedef YieldingStrategy<YieldCount> WaitStrategy;

struct Event
{
    int64_t event;
};

typedef Disruptor<Event, RING_BUFFER_SIZE, SingleThreadedStrategy<RING_BUFFER_SIZE>,
        WaitStrategy> DisruptorType;

struct DisruptorFixture {
    DisruptorFixture() : disruptor(/*initArray()*/) {}

    size_t f(const size_t i) { return i + 1; }

    // std::array<Event, RING_BUFFER_SIZE> initArray() {
    //     std::array<Event, RING_BUFFER_SIZE> tmp;
    //     for (size_t i = 0; i < RING_BUFFER_SIZE; i++) tmp[i].event = f(i);
    //     return tmp;
    // }

    DisruptorType disruptor;
};

class Timer
{
public:
    void begin()
    {
        begin_ = std::chrono::steady_clock::now();
    }
    void end()
    {
        end_ = std::chrono::steady_clock::now();
    }
    int milliseconds()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(end_ - begin_).count();
    }
    std::chrono::steady_clock::time_point begin_;
    std::chrono::steady_clock::time_point end_;
};

int main(int argc, char* argv[])
{
    DisruptorFixture fixture;

    EventHandler<Event> handle1([](Event& event, int64_t sequence, bool end){
        // std::this_thread::sleep_for(std::chrono::microseconds(20 * 1000));
        printf("C1 event=%ld tid=%d\n", event.event, gettid());
    });
    EventHandler<Event> handle2([](Event& event, int64_t sequence, bool end){
        std::this_thread::sleep_for(std::chrono::microseconds(10 * 1000));
        printf("C2 event=%ld tid=%d\n", event.event, gettid());
    });
    EventHandler<Event> handle3([](Event& event, int64_t sequence, bool end){
        // std::this_thread::sleep_for(std::chrono::microseconds(1));
        printf("C3 event=%ld tid=%d\n", event.event, gettid());
    });

    // C1\C2
    // fixture.disruptor.handlesWith({std::make_pair("C1", handle1), std::make_pair("C2", handle2)});

    //PipeLine C1\C2 -> C3
    // fixture.disruptor.handlesWith({std::make_pair("C1", handle1), std::make_pair("C2", handle2)});
    // fixture.disruptor.after({"C1", "C2"}).handleEventsWith({std::make_pair("C3", handle3)});

    //PipeLine C1\C2 -> C3
    // auto group1 = fixture.disruptor.handlesWith({std::make_pair("C1", handle1)});
    // auto group2 = fixture.disruptor.handlesWith({std::make_pair("C2", handle2)});
    // auto group3 = group1.And(group2);
    // group3.handleEventsWith({std::make_pair("C3", handle3)});

    // PipeLine C1 -> C2 -> C3
    fixture.disruptor.handlesWith({std::make_pair("C3", handle3)})
                    .handleEventsWith({std::make_pair("C2", handle2)})
                    .handleEventsWith({std::make_pair("C1", handle1)});
    
    // work_pool
    // fixture.disruptor.handlesWith("workpool", { handle1, handle2 }).handleEventsWith({std::make_pair("C3", handle3)});

    fixture.disruptor.start();

    for(int i = 0; i < 100; i++)
    {
        Event event{ i };
        fixture.disruptor.publishEvents(event);
    }

    //std::this_thread::sleep_for(std::chrono::microseconds(5000 * 1000));

    fixture.disruptor.drainAndStop();
    return 0;
}

