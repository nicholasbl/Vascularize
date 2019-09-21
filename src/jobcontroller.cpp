#include "jobcontroller.h"

void JobController::flush() {
    for (auto& f : m_active_jobs) {
        f.get();
    }
    m_active_jobs.clear();
}

JobController::JobController() {
    m_max_threads = std::thread::hardware_concurrency();

    // Some implementations don't provide the thread count
    if (m_max_threads < 1) m_max_threads = 4;
}

JobController::~JobController() { flush(); }
