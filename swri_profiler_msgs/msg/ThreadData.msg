# This message is part of the the NodeData message.  See that message
# definition for more details.

# This message contains a list of events that have occurred in a
# single thread between now and the previous events message.  

uint32 id
# A unique integer identifier for this thread.  A given thread will
# have the same identifier while a node is running.  Identifiers are
# not consistent accross runs.

Event[] events
# A list of the events that occured, ordered according to time.
