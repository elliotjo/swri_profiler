#include <ros/this_node.h>
#include <swri_profiler/profiler.h>
#include <ros/publisher.h>

#include <swri_profiler_msgs/ProfileIndex.h>
#include <swri_profiler_msgs/ProfileIndexArray.h>
#include <swri_profiler_msgs/ProfileLogArray.h>

namespace spm = swri_profiler_msgs;

namespace swri_profiler
{
// Define/initialize static member variables for the Profiler class.
boost::thread_specific_ptr<Profiler::TLS> Profiler::tls_;

// These are super-private variables for the Profiler class, only
// visible in this source file.
static SpinLock g_master_lock_;
static EventLog g_all_logs_;

static bool g_profiler_initialized_ = false;
static ros::Publisher g_profiler_index_pub_;
static ros::Publisher g_profiler_data_pub_;
static boost::thread g_profiler_thread_;

static std::unordered_map<std::string, int32_t> g_name_keys_;

static void profilerMain();
static void collectAndPublish();

static ros::Time timeFromWall(const ros::WallTime &src)
{
  return ros::Time(src.sec, src.nsec);
}
    
void Profiler::initializeTLS()
{
  if (tls_.get()) {
    ROS_ERROR("Attempt to initialize thread local storage again.");
    return;
  }

  SpinLockGuard guard(g_master_lock_);

  if (!g_profiler_initialized_) {
    ROS_INFO("Initializing swri_profiler...");
    ros::NodeHandle nh;
    g_profiler_index_pub_ = nh.advertise<spm::ProfileIndexArray>("/profiler/index", 1, true);
    g_profiler_data_pub_ = nh.advertise<spm::ProfileLogArray>("/profiler/data", 100, false);
    g_profiler_thread_ = boost::thread(profilerMain);   
    g_profiler_initialized_ = true;
  }
  
  ROS_INFO("Initializing profiler log for new thread.");
  tls_.reset(new TLS());
  EventLog *last = &g_all_logs_;
  while (last->next != NULL) {
    last = last->next;
  }
  
  EventLog *new_log = new EventLog();
  new_log->next = NULL;
  last->next = new_log;
  tls_->log = new_log;
}
  
static void profilerMain()
{
  ROS_DEBUG("swri_profiler thread started.");
  while (ros::ok()) {
    // Align updates to approximately every second.
    ros::WallTime now = ros::WallTime::now();
    ros::WallTime next(now.sec+1,0);
    (next-now).sleep();
    collectAndPublish();
  }
  
  ROS_DEBUG("swri_profiler thread stopped.");
}

static void collectAndPublish()
{
  const ros::WallTime now = ros::WallTime::now();

  // Block new threads during an update.  This should happen rarely
  // and mainly near start up.
  SpinLockGuard guard(g_master_lock_);

  // First we swap every thread's events vector.  This allows us to
  // quickly swap out the storage area so that we minimize the impact
  // on concurrent threads.  We should also get a slight benefit
  // because the vectors should generally be pre-allocated to an
  // appropriate size so that reallocs become rare after running for a
  // while.
  size_t log_count = 0;
  for (EventLog *log = g_all_logs_.next; log != NULL; log = log->next) {
    SpinLockGuard guard(log->lock);
    log->events_swap.swap(log->events);
    log_count++;
  }

  // At this point, we've swapped all of the thread event logs so that
  // threads are happily storing new events in the "events" member
  // while we are free to process the "events_swap" member which
  // contains data for the previous time segment.  We continue to hold
  // the g_master_lock to so that the g_all_logs linked list is in a fixed state.
  spm::ProfileLogArrayPtr data = boost::make_shared<spm::ProfileLogArray>();
  data->header.stamp = timeFromWall(now);
  data->header.frame_id = ros::this_node::getName();
  data->logs.reserve(log_count);

  bool keys_added = false;
  for (EventLog *src_log = g_all_logs_.next; src_log != NULL; src_log = src_log->next) {
    data->logs.emplace_back();
    data->logs.back().thread_index = data->logs.size();
    data->logs.back().events.resize(src_log->events_swap.size());

    for (size_t j = 0; j < src_log->events_swap.size(); j++) {
      const Event &src_event = src_log->events_swap[j];

      uint32_t key = g_name_keys_[src_event.name];
      if (key == 0) {
        key = g_name_keys_.size()*2;
        g_name_keys_[src_event.name] = key;
        keys_added = true;
      }

      // For open events, key = base_key + 0. For close events, key = base_key + 1.
      if (!src_event.status) {
        key += 1;
      }

      data->logs.back().events[j].key = key;
      data->logs.back().events[j].stamp = timeFromWall(src_event.stamp);
    }
    src_log->events_swap.clear();
  }

  if (keys_added) {
    spm::ProfileIndexArray index;
    index.header.stamp = timeFromWall(now);
    index.header.frame_id = ros::this_node::getName();
    index.data.reserve(g_name_keys_.size());
    
    for (auto const &pair : g_name_keys_) {
      index.data.emplace_back();
      index.data.back().key = pair.second;
      index.data.back().label = pair.first;
    }        
    g_profiler_index_pub_.publish(index);
  }

  g_profiler_data_pub_.publish(data);
  }
}  // namespace swri_profiler
