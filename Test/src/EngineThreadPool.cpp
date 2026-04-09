#include "EngineThreadPool.hpp"

EngineThreadPool::EngineThreadPool(size_t numThreads) : stop(false), activeTasks(0) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queueMutex);
                    this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                    
                    if (this->stop && this->tasks.empty()) return;
                    
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                    this->activeTasks++;
                }
                
                task(); // 작업 실행!
                
                {
                    std::unique_lock<std::mutex> lock(this->queueMutex);
                    this->activeTasks--;
                }
                this->waitCondition.notify_one();
            }
        });
    }
}

EngineThreadPool::~EngineThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
    condition.notify_all();
    for (std::thread &worker : workers) {
        worker.join();
    }
}

void EngineThreadPool::waitAll() {
    std::unique_lock<std::mutex> lock(queueMutex);
    waitCondition.wait(lock, [this] { return this->tasks.empty() && this->activeTasks == 0; });
}