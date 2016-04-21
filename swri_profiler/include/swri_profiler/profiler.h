#ifndef SWRI_PROFILER_PROFILER_H_
#define SWRI_PROFILER_PROFILER_H_

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <atomic>

#include <ros/time.h>
#include <ros/console.h>
#include <diagnostic_updater/diagnostic_updater.h>

namespace swri_profiler
{
class SpinLock
{
  std::atomic_flag locked_;
 public:
  SpinLock() : locked_(ATOMIC_FLAG_INIT) {}
    
  void acquire()
  { 
    while (locked_.test_and_set(std::memory_order_acquire)) { ; }
  }

  void release()
  {
    locked_.clear(std::memory_order_release);
  }
};

class SpinLockGuard
{
  SpinLock &lock_;
  
 public:
  SpinLockGuard(SpinLock &lock) : lock_(lock) { lock_.acquire(); }
  ~SpinLockGuard() { lock_.release(); }
};

struct Event
{
  std::string name;
  bool status;  // True if this is an open event, false for a close event.
  ros::WallTime stamp;
  Event() {};
  Event(const std::string &name, bool status, const ros::WallTime &stamp) :
    name(name), status(status), stamp(stamp) {}
};
  
struct ThreadData
{
  SpinLock lock;
  uint32_t id;
  std::vector<Event> events;
  std::vector<Event> events_swap;
  struct ThreadData *next;
};

class Profiler
{
  // We support multithreading by storing events in separate lists for
  // each thread.  The data is stored indirectly because we don't want
  // it to be deleted if a thread is closed (We currently don't
  // support removing threads once they are created).
  struct TLS
  {    
    ThreadData *data;
  };
  static boost::thread_specific_ptr<TLS> tls_;

  // Other static methods implemented in profiler.cpp
  static void initializeTLS();

  void addEvent(const std::string &name, bool status, const ros::WallTime &stamp)
  {
    SpinLockGuard guard(tls_->data->lock);
    tls_->data->events.emplace_back(name, status, stamp);
  }
  
 private:
  std::string name_;
  
 public:
  Profiler(const std::string &name)
    :
    name_(name)
  {
    if (!tls_.get()) { initializeTLS(); }
    addEvent(name_, true, ros::WallTime::now());
  }
        
  ~Profiler()
  {
    addEvent(name_, false, ros::WallTime::now());
  }
};
}  // namespace swri_profiler

// Macros for string concatenation that work with built in macros.
#define SWRI_PROFILER_CONCAT_DIRECT(s1,s2) s1##s2
#define SWRI_PROFILER_CONCAT(s1, s2) SWRI_PROFILER_CONCAT_DIRECT(s1,s2)

#define SWRI_PROFILER_IMP(block_var, name)             \
  swri_profiler::Profiler block_var(name);             \

#ifndef DISABLE_SWRI_PROFILER
#define SWRI_PROFILE(name) SWRI_PROFILER_IMP(      \
    SWRI_PROFILER_CONCAT(prof_block_, __LINE__),   \
    name)
#else // ndef DISABLE_SWRI_PROFILER
#define SWRI_PROFILE(name)
#endif // def DISABLE_SWRI_PROFILER

#endif  // SWRI_PROFILER_PROFILER_H_
