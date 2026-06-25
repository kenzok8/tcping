#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>

#include "tcp.h"

/* Connect to one already-resolved address. Uses a non-blocking socket with
 * poll() so the timeout is honoured reliably on every kernel (a blocking
 * connect() ignores SO_SNDTIMEO on many platforms), and measures the handshake
 * with CLOCK_MONOTONIC so the RTT is immune to wall-clock / NTP steps.
 *
 * Returns 0 on a completed TCP handshake (rtt_ms filled in, socket closed),
 * or -1 with errno set on error or timeout. */
int connect_to(const struct addrinfo *ai, int timeout_ms, double *rtt_ms)
{
	struct timespec t0, t1;
	struct pollfd pfd;
	int fd, flags, err = 0, e;
	socklen_t errlen = sizeof(err);

	fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (fd == -1)
		return -1;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
		goto fail;

	clock_gettime(CLOCK_MONOTONIC, &t0);

	if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0)
		goto done;			/* connected immediately (e.g. loopback) */
	if (errno != EINPROGRESS)
		goto fail;

	pfd.fd = fd;
	pfd.events = POLLOUT;
	e = poll(&pfd, 1, timeout_ms);
	if (e == -1)
		goto fail;
	if (e == 0) {
		errno = ETIMEDOUT;
		goto fail;
	}

	/* poll() reported the socket writable: read the real connect result */
	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen) == -1)
		goto fail;
	if (err != 0) {
		errno = err;
		goto fail;
	}

done:
	clock_gettime(CLOCK_MONOTONIC, &t1);
	if (rtt_ms)
		*rtt_ms = (t1.tv_sec - t0.tv_sec) * 1000.0 +
			  (t1.tv_nsec - t0.tv_nsec) / 1000000.0;
	close(fd);
	return 0;

fail:
	e = errno;			/* preserve errno across close() */
	close(fd);
	errno = e;
	return -1;
}
