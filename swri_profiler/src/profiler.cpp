#include <ros/this_node.h>
#include <swri_profiler/profiler.h>
#include <ros/publisher.h>

#include <swri_profiler_msgs/ProfileIndex.h>
#include <swri_profiler_msgs/ProfileData.h>

namespace spm = swri_profiler_msgs;

namespace swri_profiler
{
// Define/initialize static member variables for the Profiler class.
boost::thread_specific_ptr<Profiler::TLS> Profiler::tls_;

// These are super-private variables for the Profiler class, only
// visible in this source file.
static SpinLock g_master_lock_;
static ThreadData g_all_threads_;
static int g_next_thread_id = 0;

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
    g_profiler_index_pub_ = nh.advertise<spm::ProfileIndex>("/profiler/index", 1, true);
    g_profiler_data_pub_ = nh.advertise<spm::ProfileData>("/profiler/data", 100, false);
    g_profiler_thread_ = boost::thread(profilerMain);   
    g_profiler_initialized_ = true;
  }
  
  ROS_INFO("Initializing profiler data for new thread.");
  tls_.reset(new TLS());
  ThreadData *last = &g_all_threads_;
  while (last->next != NULL) {
    last = last->next;
  }
  
  ThreadData *new_thread = new ThreadData();
  new_thread->id = g_next_thread_id++;
  new_thread->next = NULL;
  last->next = new_thread;
  tls_->data = new_thread;
}
  
static void profilerMain()
{
  ROS_DEBUG("swri_profiler thread started.");
  while (ros::ok()) {
    // Align updates to approximately every second.  This is probably
    // not necessary now that we've changed to the publishing actual
    // events instead of processed statistics.
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
  size_t thread_count = 0;
  for (ThreadData *thread = g_all_threads_.next; thread != NULL; thread = thread->next) {
    SpinLockGuard guard(thread->lock);
    thread->events_swap.swap(thread->events);
    thread_count++;
  }

  // At this point, we've swapped all of the thread data so that
  // threads are happily storing new events in the "events" member
  // while we are free to process the "events_swap" member which
  // contains data for the previous time segment.  We continue to hold
  // the g_master_lock to so that the g_all_threads linked list is in a fixed state.
  spm::ProfileDataPtr data = boost::make_shared<spm::ProfileData>();
  data->header.stamp = timeFromWall(now);
  data->header.frame_id = ros::this_node::getName();
  data->threads.reserve(thread_count);

  bool keys_added = false;
  for (ThreadData *thread = g_all_threads_.next; thread != NULL; thread = thread->next) {
    data->threads.emplace_back();
    data->threads.back().id = thread->id;
    data->threads.back().events.resize(thread->events_swap.size());

    for (size_t j = 0; j < thread->events_swap.size(); j++) {
      const Event &src_event = thread->events_swap[j];

      uint32_t block_id = g_name_keys_[src_event.name];
      if (block_id == 0) {
        block_id = g_name_keys_.size()*2;
        g_name_keys_[src_event.name] = block_id;
        keys_added = true;
      }

      // For open events, key = base_key + 0. For close events, key = base_key + 1.
      uint32_t event_id = block_id;
      if (!src_event.status) {
        event_id += 1;
      }

      data->threads.back().events[j].event_id = event_id;
      data->threads.back().events[j].stamp = timeFromWall(src_event.stamp);
    }
    thread->events_swap.clear();
  }

  if (keys_added) {
    spm::ProfileIndex index_msg;
    index_msg.header.stamp = timeFromWall(now);
    index_msg.header.frame_id = ros::this_node::getName();
    index_msg.index.reserve(g_name_keys_.size());
    
    for (auto const &pair : g_name_keys_) {
      index_msg.index.emplace_back();
      index_msg.index.back().id = pair.second;
      index_msg.index.back().label = pair.first;
    }        
    g_profiler_index_pub_.publish(index_msg);
  }

  g_profiler_data_pub_.publish(data);
  }
}  // namespace swri_profiler
