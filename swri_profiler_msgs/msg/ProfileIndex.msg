#  The profiler encodes text names as integers to reduce the size the
#  periodic messages (and reduce overhead needed to serialize larger
#  messages).  The ProfilerIndex message contains the index to convert
#  identifiers back into human readable strings.

# Profiled events are identified with an integer id instead of a
# string to significantly reduce the size of the messages.  The
# ProfileIndexArray message provides a map between these block ids
# (integers) and the corresponding name (string).  
#
# Every profiled node publishes a ProfileIndex message to the latched
# /profiler/index topic.  Each node's index is valid for every thread
# that is profiled in that node.  A new message is published when
# blocks are added to the index.  Blocks are added to the index over
# time, but blocks are never removed or changed once added.
#
# Note that blocks are enumerated based on the order they are
# encountered in the node, so the ids are not guaranteed to be
# consistent between different runs.

# Blocks are defined by a call to the SWRI_PROFILER() and when the the
# C++ scope containing the call ends.

Header header
# The header contains the current ros::WallTime() when the index was
# generated and the name of the node in the frame_id.

ProfileIndexEntry[] index
# This list constains the pairs that that map identifiers to text
# names.  This list may grow over time as new names are encountered,
# but pairs will never be deleted or changed once added.  The order of
# the identifiers list is undefined and arbitrary.
