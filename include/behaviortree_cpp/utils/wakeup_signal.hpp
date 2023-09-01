#ifndef BEHAVIORTREECORE_WAKEUP_SIGNAL_HPP
#define BEHAVIORTREECORE_WAKEUP_SIGNAL_HPP

#include <chrono>
#include <mutex>
#include <condition_variable>

namespace BT
{

class WakeUpSignal
{
public:
    /// Return true if the timeout was NOT reached and the
    /// signal was received.
    bool waitFor(std::chrono::microseconds usec)
    {
      if(usec.count() > 0) {
        std::unique_lock<std::mutex> lk(mutex_);
        auto res = cv_.wait_for(lk, usec, [this]{
          return ready_;
        });
        ready_ = false;
        return res;
      }
      return ready_;
    }

    void emitSignal()
    {
       {
           std::lock_guard<std::mutex> lk(mutex_);
           ready_ = true;
       }
       cv_.notify_all();
    }

private:

    std::mutex mutex_;
    std::condition_variable cv_;
    bool ready_ = false;
};

}

#endif // BEHAVIORTREECORE_WAKEUP_SIGNAL_HPP
