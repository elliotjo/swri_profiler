# This message is published by the swri_profiler periodically to
# profile code in a node.  Each message contains all of the events
# that were measured since the previous message was published.

# This message is published at 1Hz and the profiler tries to align the
# messages with integer seconds so that messages are approximately
# synchonized between nodes.

# Events are captured on a per-thread basis.  To reduce message size
# (and added load from serialization), events are identified using an
# encoding that identified the block and event type using an integer.
# See the Event.msg and ProfileIndex.msg definitions for more details
# of the encoding.

Header header
# The header contains the ros::WallTime() from when this message was
# created and the name of the node in the frame_id.

ThreadData[] threads
# threads contains the data collected for each thread.  The ordering
# of this list is undefined and the number of entries may change
# between messages depending on if events were collected.
