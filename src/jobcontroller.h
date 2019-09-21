#ifndef JOBCONTROLLER_H
#define JOBCONTROLLER_H

#include <future>
#include <thread>
#include <vector>


///
/// \brief The JobController class is a super simple threaded scoped job
/// scheduler.
///
/// Usage: Create an instance, and add work. Work will be distributed to a new
/// thread. Number of threads should be bounded by the number of cores.
///
/// \todo Make this a thread pool
///
class JobController {
    size_t                         m_max_threads;
    std::vector<std::future<void>> m_active_jobs;

    ///
    /// \brief Flush work. Wait on all futures and clear work vector.
    ///
    void flush();

public:
    JobController();

    ~JobController();

    template <class Function>
    void add_job(Function f) {
        if (m_active_jobs.size() == m_max_threads) {
            flush();
        }

        m_active_jobs.push_back(std::async(std::launch::async, f));
    }
};

#endif // JOBCONTROLLER_H
