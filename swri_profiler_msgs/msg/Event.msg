# This message is part of the ThreadData message.  See that message
# definition for more details.

# The Event message records the time a profiled event.  An event is
# the beginning or end of a profiled code block.  The identified of
# the block and the type of the event are encoded in the event_id, and
# the time of the event is stored in the stamp.

uint32 event_id
# The event_id is encodes the id of a code block and the type of event
# (begin or end).  An even event_id corresponds to the start of a code
# block (with an id the same value) .  An odd event_id corresponds to
# the end of a block with id = event_id - 1.  In other words, the
# block ids are always even and (event_id = block_id) for begin events
# or (event_id = block_id + 1) for end events.  Ids are converted to
# human readable names according the current profile index.

time stamp
# The time stamp of the event in ros::WallTime.
