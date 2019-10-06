#ifndef JOBCONTROLLER_H
#define JOBCONTROLLER_H

#include <future>
#include <queue>
#include <thread>
#include <vector>

///
/// \brief The Executor class is a super simple thread pool
///
/// Refactor to use jthread in c++20.
///
class Executor {
    std::vector<std::thread>          m_workers;
    std::queue<std::function<void()>> m_tasks;

    std::mutex              m_queue_mutex;
    std::condition_variable m_condition;
    std::atomic<bool>       m_stop;

public:
    Executor(size_t num_threads = 0);
    ~Executor();

    ///
    /// \brief Get the number of workers
    size_t size() const { return m_workers.size(); }

    ///
    /// \brief Add a task to the executor
    ///
    /// It is VITAL that you use the returned object failing to do so will cause
    /// your code to block from the implicit destruction of the future
    template <class F, class... Args>
    [[nodiscard]] auto enqueue(F&& f, Args&&... args) {
        using ReturnType = typename std::result_of_t<F(Args...)>;

        // no way to get around using shared ptr for now
        // the packaged tasks are move only, and move only
        // things eventually work their way to function,
        // which doesnt handle those nicely
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        auto res = task->get_future();
        {
            // Critical zone
            std::unique_lock<std::mutex> lock(m_queue_mutex);

            if (m_stop.load(std::memory_order_acquire)) {
                // todo: shove off to its own thread, as the executor is dead.
                return res;
            }

            // add the task to the queue
            m_tasks.emplace([task]() { (*task)(); });
        }

        // let a task know we have work
        m_condition.notify_one();
        return res;
    }
};

// =============================================================================

///
/// \brief The JobController class is a super simple threaded scoped job
/// scheduler.
///
/// Usage: Create an instance, and add work. Work will be distributed to a new
/// thread. Number of threads should be bounded by the number of cores.
///
class JobController {
    size_t   m_max_threads;
    Executor m_executor;

    std::queue<std::future<void>> m_active_jobs;

    ///
    /// \brief Flush wait on the front future of the queue so we can clear it
    ///
    void flush();

public:
    JobController();

    /// \brief Destroy the controller. Note that this flushes work, so you
    /// should watch your scopes to make sure you don't proceed before all jobs
    /// are completed.
    ~JobController();

    size_t num_threads() const;

    ///
    /// \brief Add a job to the controller. It'll launch the function in another
    /// thread, and will block to make sure not too many jobs are in flight.
    ///
    template <class Function>
    void add_job(Function f) {

        if (m_active_jobs.size() == m_max_threads) {
            flush();
        }

        auto future = m_executor.enqueue(f);

        m_active_jobs.push(std::move(future));
    }
};

#endif // JOBCONTROLLER_H
