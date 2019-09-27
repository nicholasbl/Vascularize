#include "jobcontroller.h"

#include <fmt/printf.h>

Executor::Executor(size_t num_threads) {
    m_stop = false;

    for (size_t i = 0; i < num_threads; ++i) {
        m_workers.emplace_back([this] {
            // Each thread is a loop

            while (true) {

                // pull a task off the list

                std::function<void()> task;

                {
                    // Critcal zone. lock the queue mutex
                    std::unique_lock<std::mutex> lock(this->m_queue_mutex);

                    // We want to wait until there is something in the queue, or
                    // the stop token is issued.
                    this->m_condition.wait(lock, [this] {
                        return this->m_stop.load(std::memory_order_acquire) or
                               !this->m_tasks.empty();
                    });

                    // If stop is issued, It's not going to switch back. bail if
                    // so.
                    if (this->m_stop) {
                        return;
                    }

                    // Take the next task.
                    task = std::move(this->m_tasks.front());
                    this->m_tasks.pop();
                }

                // Execute the task
                task();
            }
        });
    }
}

Executor::~Executor() {

    // Issue stop to all workers
    m_stop.store(true, std::memory_order_release);

    // Wake them up
    m_condition.notify_all();

    // Join all threads; they should be done
    for (std::thread& worker : m_workers) {
        worker.join();
    }
}

// =============================================================================

void JobController::flush() {
    auto& b = m_active_jobs.front();

    b.get();

    m_active_jobs.pop();
}

static size_t get_number_of_threads() {
    size_t threads = std::thread::hardware_concurrency();

    // Some implementations don't provide the thread count, and return a 0
    if (threads < 1) {

        threads = 4;
    }

    fmt::print("Using {} threads\n", threads);

    return threads;
}

JobController::JobController()
    : m_max_threads(get_number_of_threads() * 2), m_executor(m_max_threads) {}

JobController::~JobController() {
    while (m_active_jobs.size()) {
        flush();
    }
}
