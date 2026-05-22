// LLM / maintainer hints:
// - Provide internal concurrency helpers without changing observable order.
// - Parallel work is allowed, but commit/merge phases must stay deterministic.
// - Keep thread-pool ownership and shutdown behavior explicit.
// - Until real task scheduling is wired, provide deterministic serial fallbacks
//   so core passes can depend on one execution contract.

#include <cstddef>
#include <functional>
#include <vector>

namespace olnk {

class DeterministicTaskRunner {
public:
    void add_task(std::function<void()> task)
    {
        if (task) {
            tasks_.push_back(std::move(task));
        }
    }

    // Executes tasks in insertion order to preserve deterministic behavior.
    void run_all_serial()
    {
        for (const std::function<void()>& task : tasks_) {
            task();
        }
    }

    std::size_t task_count() const
    {
        return tasks_.size();
    }

    void clear()
    {
        tasks_.clear();
    }

private:
    std::vector<std::function<void()>> tasks_;
};

} // namespace olnk
