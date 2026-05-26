#include "simple_scheduler.hpp"

SimpleScheduler::SimpleScheduler(const std::string& name) 
    : schedulerName_(name), running_(false) {
    Logger::log(LogLevel::INFO, "SCHEDULER", "Created scheduler: " + name);
}

SimpleScheduler::~SimpleScheduler() {
    stop();
    Logger::log(LogLevel::INFO, "SCHEDULER", "Destroyed scheduler: " + schedulerName_);
}

bool SimpleScheduler::addTask(const std::string& name,
                              TaskFunction function,
                              TaskPriority priority,
                              std::chrono::milliseconds period) {
    std::lock_guard<std::mutex> lock(schedulerMutex_);
    
    if (tasks_.size() >= MAX_TASKS) {
        Logger::log(LogLevel::ERROR, "SCHEDULER", "Maximum tasks reached");
        return false;
    }
    
    // Check if task already exists
    if (findTask(name)) {
        Logger::log(LogLevel::WARNING, "SCHEDULER", 
                   "Task already exists: " + name);
        return false;
    }
    
    auto task = std::make_shared<Task>();
    task->name = name;
    task->function = function;
    task->priority = priority;
    task->state = TaskState::READY;
    task->executionCount = 0;
    task->period = period;
    task->executionTime = std::chrono::milliseconds(0);
    task->nextRunTime = std::chrono::system_clock::now();
    task->lastRunTime = std::chrono::system_clock::now();
    task->totalExecutionTime = std::chrono::milliseconds(0);
    
    tasks_.push_back(task);
    
    Logger::log(LogLevel::INFO, "SCHEDULER", 
               "Added task: " + name + 
               " with priority: " + std::to_string(static_cast<int>(priority)));
    
    return true;
}

bool SimpleScheduler::removeTask(const std::string& name) {
    std::lock_guard<std::mutex> lock(schedulerMutex_);
    
    auto it = std::find_if(tasks_.begin(), tasks_.end(),
        [&name](const std::shared_ptr<Task>& task) {
            return task->name == name;
        });
    
    if (it != tasks_.end()) {
        (*it)->state = TaskState::TERMINATED;
        tasks_.erase(it);
        Logger::log(LogLevel::INFO, "SCHEDULER", "Removed task: " + name);
        return true;
    }
    
    return false;
}

bool SimpleScheduler::suspendTask(const std::string& name) {
    std::lock_guard<std::mutex> lock(schedulerMutex_);
    
    auto task = findTask(name);
    if (task) {
        task->state = TaskState::SUSPENDED;
        Logger::log(LogLevel::INFO, "SCHEDULER", "Suspended task: " + name);
        return true;
    }
    
    return false;
}

bool SimpleScheduler::resumeTask(const std::string& name) {
    std::lock_guard<std::mutex> lock(schedulerMutex_);
    
    auto task = findTask(name);
    if (task) {
        task->state = TaskState::READY;
        Logger::log(LogLevel::INFO, "SCHEDULER", "Resumed task: " + name);
        return true;
    }
    
    return false;
}

void SimpleScheduler::start() {
    if (running_) {
        Logger::log(LogLevel::WARNING, "SCHEDULER", "Scheduler already running");
        return;
    }
    
    running_ = true;
    schedulerThread_ = std::make_unique<std::thread>(&SimpleScheduler::schedulerLoop, this);
    Logger::log(LogLevel::INFO, "SCHEDULER", "Started scheduler: " + schedulerName_);
}

void SimpleScheduler::stop() {
    running_ = false;
    
    if (schedulerThread_ && schedulerThread_->joinable()) {
        schedulerThread_->join();
    }
    
    Logger::log(LogLevel::INFO, "SCHEDULER", "Stopped scheduler: " + schedulerName_);
}

void SimpleScheduler::runOnce() {
    auto task = selectNextTask();
    
    if (task) {
        task->state = TaskState::RUNNING;
        task->lastRunTime = std::chrono::system_clock::now();
        
        Logger::log(LogLevel::DEBUG, "SCHEDULER", 
                   "Executing task: " + task->name);
        
        try {
            task->function();
            task->executionCount++;
            
            auto endTime = std::chrono::system_clock::now();
            auto executionDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                endTime - task->lastRunTime);
            
            task->totalExecutionTime += executionDuration;
            task->executionTime = executionDuration;
            
        } catch (const std::exception& e) {
            Logger::log(LogLevel::ERROR, "SCHEDULER", 
                       "Task exception: " + task->name + " - " + e.what());
            task->state = TaskState::TERMINATED;
        }
        
        if (task->state != TaskState::TERMINATED) {
            task->state = TaskState::READY;
            if (task->period.count() > 0) {
                task->nextRunTime = std::chrono::system_clock::now() + task->period;
            }
        }
    }
}

bool SimpleScheduler::isRunning() const {
    return running_;
}

std::vector<SimpleScheduler::TaskInfo> SimpleScheduler::getTaskList() const {
    std::lock_guard<std::mutex> lock(schedulerMutex_);
    
    std::vector<TaskInfo> taskList;
    for (const auto& task : tasks_) {
        TaskInfo info;
        info.name = task->name;
        info.priority = task->priority;
        info.state = task->state;
        info.executionCount = task->executionCount;
        info.totalExecutionTime = task->totalExecutionTime;
        info.lastRunTime = task->lastRunTime;
        taskList.push_back(info);
    }
    
    return taskList;
}

SimpleScheduler::TaskInfo SimpleScheduler::getTaskInfo(const std::string& name) const {
    std::lock_guard<std::mutex> lock(schedulerMutex_);
    
    auto task = findTask(name);
    if (!task) {
        throw std::runtime_error("Task not found: " + name);
    }
    
    TaskInfo info;
    info.name = task->name;
    info.priority = task->priority;
    info.state = task->state;
    info.executionCount = task->executionCount;
    info.totalExecutionTime = task->totalExecutionTime;
    info.lastRunTime = task->lastRunTime;
    
    return info;
}

size_t SimpleScheduler::getTotalTasks() const {
    std::lock_guard<std::mutex> lock(schedulerMutex_);
    return tasks_.size();
}

size_t SimpleScheduler::getActiveTasks() const {
    std::lock_guard<std::mutex> lock(schedulerMutex_);
    return std::count_if(tasks_.begin(), tasks_.end(),
        [](const std::shared_ptr<Task>& task) {
            return task->state == TaskState::READY || task->state == TaskState::RUNNING;
        });
}

void SimpleScheduler::schedulerLoop() {
    Logger::log(LogLevel::INFO, "SCHEDULER", "Scheduler loop started");
    
    while (running_) {
        runOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    Logger::log(LogLevel::INFO, "SCHEDULER", "Scheduler loop ended");
}

std::shared_ptr<SimpleScheduler::Task> SimpleScheduler::findTask(const std::string& name) {
    auto it = std::find_if(tasks_.begin(), tasks_.end(),
        [&name](const std::shared_ptr<Task>& task) {
            return task->name == name;
        });
    
    return (it != tasks_.end()) ? *it : nullptr;
}

std::shared_ptr<SimpleScheduler::Task> SimpleScheduler::selectNextTask() {
    std::lock_guard<std::mutex> lock(schedulerMutex_);
    
    std::shared_ptr<Task> selectedTask = nullptr;
    auto now = std::chrono::system_clock::now();
    
    for (auto& task : tasks_) {
        if (task->state != TaskState::READY) continue;
        
        // Check if periodic task is due
        if (task->period.count() > 0 && now < task->nextRunTime) continue;
        
        // Select task with highest priority
        if (!selectedTask || task->priority > selectedTask->priority) {
            selectedTask = task;
        }
        // If same priority, select the one that hasn't run recently
        else if (task->priority == selectedTask->priority) {
            if (task->lastRunTime < selectedTask->lastRunTime) {
                selectedTask = task;
            }
        }
    }
    
    return selectedTask;
}