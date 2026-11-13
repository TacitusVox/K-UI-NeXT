#include "sci_stream.h"
char stream_state_bytes[sizeof(struct kui_sci_stream_state)];
char stream_stats_at[offsetof(struct kui_sci_stream_state, stats)];
char stream_stats_bytes[sizeof(struct kui_sci_stream_stats)];
char stream_wire_at[offsetof(struct kui_sci_stream_state, wire)];
