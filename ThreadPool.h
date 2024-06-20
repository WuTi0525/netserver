#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <queue>
#include <sys/syscall.h>
#include <mutex>
#include <unistd.h>
#include <thread>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>

class ThreadPool   
{
private:
	std::vector<std::thread> threads_;                          // 线程池中的线程。
	std::queue<std::function<void()>> taskqueue_;    // 任务队列。
	std::mutex mutex_;                                                  // 任务队列同步的互斥锁。
	std::condition_variable condition_;                         // 任务队列同步的条件变量。
	std::atomic_bool stop_;                                           // 在析构函数中，把stop_的值设置为true，全部的线程将退出。
	const std::string threadtype_;                                 // 线程种类："IO"、"WORKS"
public:
    // 在构造函数中将启动threadnum个线程，
	ThreadPool(size_t threadnum,const std::string& threadtype);

    // 把任务添加到队列中。
    void addtask(std::function<void()> task);   

	// 获取线程池的大小。
	size_t size();

	// 停止线程。
	void stop();

    // 在析构函数中将停止线程。
	~ThreadPool();
};