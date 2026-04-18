#pragma once

#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>

class EngineThreadPool {
public:
    EngineThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    ~EngineThreadPool();

    EngineThreadPool(const EngineThreadPool&) = delete;
    EngineThreadPool& operator=(const EngineThreadPool&) = delete;

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>;

    void waitAll();

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queueMutex;
    std::condition_variable condition;
    std::condition_variable waitCondition;
    
    bool stop;
    int activeTasks;
};

template<class F, class... Args>
auto EngineThreadPool::enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {
    using return_type = typename std::invoke_result<F, Args...>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        if (stop) throw std::runtime_error("종료된 스레드 풀에 작업을 추가할 수 없습니다.");
        
        tasks.emplace([task]() { (*task)(); });
    }
    condition.notify_one();
    return res;
}