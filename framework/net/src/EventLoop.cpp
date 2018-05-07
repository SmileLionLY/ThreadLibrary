#include <EventLoop.hpp>
#include <Logging.hpp>
#include <Poller.h>

namespace Net {

__thread EventLoop* t_eventloop_in_thread = NULL;

#define POLLWAITTIME 1000

static int CreateEventFd() {
    int event_fd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (event_fd < 0) {
        LOG_SYSFATAL << "faild to create eventfd.";
    }
    return event_fd;
}

EventLoop::EventLoop() :
    poller_(new Poller(this)),
    wakeup_fd_(CreateEventFd()),
    doing_task_(false),
    loop_(false),
    quit_(false),
    wakeup_channel_(new Channel(this, wakeup_fd_)),
    current_active_channel_(NULL) {
    if (t_eventloop_in_thread) {
        LOG_FATAL << "have already created eventloop in current thread.";
    } else {
        LOG_INFO << "Create EventLoop in current thread.";
        t_eventloop_in_thread = this;
    }
    wakeup_channel_
}

EventLoop::~EventLoop() {
    
}

EventLoop::Loop() {
    if (!IsInLoopThread()) {
        LOG_FATAL << "loop not in loop thread.";
    }
    if (true == loop_) {
        LOG_FATAL << "loop start than one time.";
    }
    loop_ = true;
    quit_ = false;
    while (!quit_) {
        active_channels_.clear();
        poller_->poll(active_channels_);
        for (auto &channel : active_channels_) {
            channel.HandleEvent();
        }
        DoPendingTask();
    }
    loop_ = true;
}

void EventLoop::Quit() {
    quit_ = true;
    WakeUP()
}

void EventLoop::RunInLoop(Task task) {
    if (IsInLoopThread()) {
        task();
    } else {
        QueueInLoop(task);
    }
}

void EventLoop::QueueInLoop(Task task) {
    {
        Base::MutexLockGuard lock(task_mutex_);
        task_.push_back(std::move(task));
    }
    if (!IsInLoopThread() || doing_task_) {
        WakeUP();
    }
}

int EventLoop::QueueSize() {
    Base::MutexLockGuard lock(task_mutex_);
    return static_cast<int>(task_.size());
}

void EventLoop::UpdateChannel(Channel *channel) {
    if (NULL == channel) {
        LOG_FATAL << "input param channel is NULL.";
    }
    if (channel->OwnerLoop() != this) {
        LOG_FATAL << "input channel not belong to this eventloop.";
    }
    if (false == IsInLoopThread()) {
        LOG_FATAL << "can't update channel in other thread.";
    }
    poller_->UpdateChannel(channel);
}

void EventLoop::RemoveChannel(Channel *channel) {
    if (NULL == channel) {
        LOG_FATAL << "input param channel is NULL.";
    }
    if (channel->OwnerLoop() != this) {
        LOG_FATAL << "input channel not belong to this eventloop.";
    }
    if (false == IsInLoopThread()) {
        LOG_FATAL << "can't update channel in other thread.";
    }
    poller_->RemoveChannel(channel);
}

bool EventLoop::HasChannel(Channel *channel) {
    if (NULL == channel) {
        LOG_FATAL << "input param channel is NULL.";
    }
    if (channel->OwnerLoop() != this) {
        LOG_FATAL << "input channel not belong to this eventloop.";
    }
    if (false == IsInLoopThread()) {
        LOG_FATAL << "can't update channel in other thread.";
    }
    poller_->HasChannel(channel);
}

void EventLoop::WakeUP() {
    uint64_t one = 1;
    ssize_t count = ::write(wakeup_fd_, &one, sizeof(one));
    if (count != sizeof(one)) {
        LOG_SYSERR << "write wakeup fd failed.";
    }
}

void EventLoop::HandleRead() {
    uint64_t one = 1;
    ssize_t count = ::read(wakeupFd_, &one, sizeof(one));
    if (count != sizeof(one)) {
        LOG_ERR << "EventLoop::HandleRead() reads " << count << " bytes instead of 8";
    }
}

void EventLoop::DoPendingTask() {
    std::vector<Task> task;
    doing_task_ = true;
    {
        Base::MutexLockGuard lock(task_mutex_);
        task.swap(task_);
    }
    for (size_t i = 0; i < task.size(); i++) {
        task[i]();
    }
    doing_task_ = false;
}

void EventLoop::PrintActiveChannels() {
    for (const auto& channel : active_channels_) {
        LOG_INFO << "{" << channel->reventsToString() << "} ";    
    }    
    
}

EventLoop *GetCurrentThreadEventLoop() {
    return t_eventloop_in_thread;
}
};
