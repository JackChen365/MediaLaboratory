#include <iostream>
#include <condition_variable>
#include <thread>
#include <queue>

int counter=0;

void consume(int id,const std::shared_ptr<std::queue<int>> &queue,
             const std::shared_ptr<std::mutex> &mutex,
             const std::shared_ptr<std::condition_variable> &consume_var,
             const std::shared_ptr<std::condition_variable> &produce_var){
    while(true){
        std::unique_lock l(*mutex);
        consume_var->wait(l,[id,queue](){
            printf("\tconsume:%d wait:%d\n",id,queue->size());
            return 0 < queue->size();
        });
        int v=queue->front();
        queue->pop();
        printf("\tconsume:%d use:%d\n",id,v);
        l.unlock();
        produce_var->notify_one();
    }
}

void produce(int id ,const std::shared_ptr<std::mutex> &mutex,
             const std::shared_ptr<std::condition_variable> &consume_var,
             const std::shared_ptr<std::condition_variable> &produce_var){
    //Each queue
    const auto queue1 = std::make_shared<std::queue<int>>();
    const auto queue2 = std::make_shared<std::queue<int>>();
    std::thread t1(consume,1,queue1,mutex,consume_var,produce_var);
    std::thread t2(consume,2,queue2,mutex,consume_var,produce_var);
    t1.detach();
    t2.detach();
    while(true){
        std::unique_lock l(*mutex);
        counter++;
        if(0 == counter %2){
            queue1->push(counter);
        } else {
            queue2->push(counter);
        }
        printf("produce:%d num:%d--------------\n",id,counter);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        printf("notify all.\n");
        consume_var->notify_all();
        produce_var->wait(l);
    }
}

int main(){
    const auto mutex=std::make_shared<std::mutex>();
    const auto consume_var=std::make_shared<std::condition_variable>();
    const auto produce_var=std::make_shared<std::condition_variable>();
    std::thread t1(produce,0,mutex,consume_var,produce_var);
    t1.join();
    return 0;
}