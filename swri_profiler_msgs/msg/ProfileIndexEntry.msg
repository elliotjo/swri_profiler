# An entry in the ProfileIndex message.  See that message definition
# for more details.

uint32 id
# A unique unsigned integer that identifies a label.  Ids are always
# even numbers (due to the encoding scheme of the ProfileEvent
# message).

string label
# The text label for the identifier.
