// Thread pool C++17 implementation
// Reference:
// https://github.com/progschj/ThreadPool
// https://github.com/jhasse/ThreadPool
#pragma once

#include <functional>
#include <future>
#include <queue>

class ThreadPool {
public:
	// the constructor just launches some amount of workers
	ThreadPool() {
	}
	void init(size_t thread_count) {
		for (size_t i = 0; i < thread_count; ++i)
			workers.emplace_back([this] {
				for (;;) {
					std::packaged_task<void()> task;
					{
						std::unique_lock<std::mutex> lock(this->queue_mutex);
						this->condition.wait(lock,
						                     [this] { return this->stop || !this->tasks.empty(); });
						if (this->stop && this->tasks.empty()) return;
						task = std::move(this->tasks.front());
						this->tasks.pop();
					}
					task();
				}
			});
	}
	~ThreadPool() {
		{
			std::unique_lock<std::mutex> lock(queue_mutex);
			stop = true;
		}
		condition.notify_all();
		for (std::thread& worker : workers) {
			worker.join();
		}
	}
	// add new work item to the pool
	template <class F, class... Args> decltype(auto) enqueue(F&& f, Args&&... args) {
		using return_type = std::invoke_result_t<F, Args...>;

		std::packaged_task<return_type()> task(
		    std::bind(std::forward<F>(f), std::forward<Args>(args)...));

		std::future<return_type> res = task.get_future();
		{
			std::unique_lock<std::mutex> lock(queue_mutex);

			// don't allow enqueueing after stopping the pool
			if (stop) throw std::runtime_error("enqueue on stopped ThreadPool");

			tasks.emplace(std::move(task));
		}
		condition.notify_one();
		return res;
	}
	size_t backlog() {
		std::lock_guard<std::mutex> lock(this->queue_mutex);
		return tasks.size();
	}

private:
	// need to keep track of threads so we can join them
	std::vector<std::thread> workers;
	// the task queue
	std::queue<std::packaged_task<void()>> tasks;

	// synchronization
	std::mutex queue_mutex;
	std::condition_variable condition;
	bool stop = false;
};
