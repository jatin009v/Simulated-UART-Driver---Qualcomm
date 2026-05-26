#ifndef SIMPLE_SCHEDULER_HPP
#define SIMPLE_SCHEDULER_HPP

#include "common.hpp"

class SimpleScheduler {
public:
    enum class TaskPriority {
        LOW = 0,
        NORMAL = 1,
        HIGH = 2,
        CRITICAL = 3
    };
    
    enum class TaskState {
        READY,
        RUNNING,
        BLOCKED,
        SUSPENDED,
        TERMINATED
    };
    
    struct TaskInfo {
        std::string name;
        TaskPriority priority;
        TaskState state;
        size_t executionCount;
        std::chrono::milliseconds totalExecutionTime;
        std::chrono::system_clock::time_point lastRunTime;
    };
    
    using TaskFunction = std::function<void()>;
    
    struct Task {
        std::string name;
        TaskFunction function;
        TaskPriority priority;
        TaskState state;
        size_t executionCount;
        std::chrono::milliseconds period;
        std::chrono::milliseconds executionTime;
        std::chrono::system_clock::time_point nextRunTime;
        std::chrono::system_clock::time_point lastRunTime;
        std::chrono::milliseconds totalExecutionTime;
    };
    
    SimpleScheduler(const std::string& name = "MainScheduler");
    ~SimpleScheduler();
    
    // Task management
    bool addTask(const std::string& name, 
                TaskFunction function,
                TaskPriority priority = TaskPriority::NORMAL,
                std::chrono::milliseconds period = std::chrono::milliseconds(0));
    
    bool removeTask(const std::string& name);
    bool suspendTask(const std::string& name);
    bool resumeTask(const std::string& name);
    
    // Scheduler control
    void start();
    void stop();
    void runOnce();
    bool isRunning() const;
    
    // Task information
    std::vector<TaskInfo> getTaskList() const;
    TaskInfo getTaskInfo(const std::string& name) const;
    
    // Statistics
    size_t getTotalTasks() const;
    size_t getActiveTasks() const;
    
private:
    std::string schedulerName_;
    std::vector<std::shared_ptr<Task>> tasks_;
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> schedulerThread_;
    mutable std::mutex schedulerMutex_;
    
    void schedulerLoop();
    std::shared_ptr<Task> findTask(const std::string& name);
    std::shared_ptr<Task> selectNextTask();
};

#endif // SIMPLE_SCHEDULER_HPP