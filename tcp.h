#include <netdb.h>

int connect_to(const struct addrinfo *ai, int timeout_ms, double *rtt_ms);
