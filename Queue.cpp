#include <boost/thread.hpp>
#include <queue>
#include "concurrent_queue.hpp"

typedef int Message;


class Producer
{
public:
    explicit Producer(concurrent_queue<Message>& q) : _queue(q) {}

    void operator()()
    {
        // produce messages
        for (int i = 0; i < 10; i++) {
            Message m(i);
            _queue.push(m);
            std::cout << "producing message: " << m << std::endl;
            boost::this_thread::sleep(boost::posix_time::milliseconds(5));
        }
        _queue.wait_until_empty();
    }

    concurrent_queue<Message>& _queue;
};


class Consumer
{
public:
    explicit Consumer(concurrent_queue<Message>& q, int index) : _queue(q), _index(index) {}

    void operator()()
    {
        // consume messages
        try {           
            while (true) {
                Message m;
                _queue.wait_and_pop(m);
                std::cout << _index << " consuming message: " << m << std::endl;
                boost::this_thread::sleep(boost::posix_time::milliseconds(_index*50));
            }
        }
        catch (concurrent_queue<Message>::Stopped) {
            std::cout << _index << " thread done." << std::endl;            
        }
    }

    concurrent_queue<Message>& _queue;
    int _index;
};


int main() {
    concurrent_queue<Message> q(100);

    boost::thread_group consumer_threads;
    consumer_threads.create_thread(Consumer(q, 1));
    consumer_threads.create_thread(Consumer(q, 2));

    Producer p = Producer(q);
    boost::thread producer_thread(p);
    producer_thread.join();    
    q.stop();
 
    consumer_threads.join_all();
}
