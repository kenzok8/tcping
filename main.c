#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "tcp.h"

static volatile sig_atomic_t stop = 0;

void usage(void)
{
	fprintf(stderr, "Usage\n");
	fprintf(stderr, "tcping [options] <destination>\n\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  <destination>      dns name or ip address (IPv4/IPv6)\n");
	fprintf(stderr, "  -4                 force IPv4\n");
	fprintf(stderr, "  -6                 force IPv6\n");
	fprintf(stderr, "  -c <count>         count how many times to connect\n");
	fprintf(stderr, "  -f                 flood connect (no delays)\n");
	fprintf(stderr, "  -h                 print help and exit\n");
	fprintf(stderr, "  -i <interval>      interval delay between each connect (e.g. 1)\n");
	fprintf(stderr, "  -p <port>          portnr portnumber (e.g. 80)\n");
	fprintf(stderr, "  -q                 quiet, only returncode\n");
	fprintf(stderr, "  -t <timeout>       time to wait for response (e.g. 3)\n\n");
}

void handler(int sig)
{
	(void)sig;
	stop = 1;
}

int main(int argc, char *argv[])
{
	char *hostname = NULL;
	int portnr = 80;
	int c;
	int timeout = 3;
	int count = -1, curncount = 0;
	int wait = 1, quiet = 0;
	int ok = 0, err = 0;
	int family = AF_UNSPEC;
	double min = 0.0, avg = 0.0, max = 0.0;
	char portstr[16];
	struct addrinfo hints, *res = NULL;
	int gai;

	while((c = getopt(argc, argv, "p:c:i:t:46fqh?")) != -1)
	{
		switch(c)
		{
		case 'p':
			portnr = atoi(optarg);
			break;

		case 'c':
			count = atoi(optarg);
			break;

		case 'i':
			wait = atoi(optarg);
			break;

		case 't':
			timeout = atoi(optarg);
			break;

		case '4':
			family = AF_INET;
			break;

		case '6':
			family = AF_INET6;
			break;

		case 'f':
			wait = 0;
			break;

		case 'q':
			quiet = 1;
			break;

		case 'h':
		case '?':
		default:
			usage();
			return 0;
		}
	}

	if (optind >= argc) {
		usage();
		return 3;
	}
	hostname = argv[optind];

	snprintf(portstr, sizeof(portstr), "%d", portnr);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_ADDRCONFIG;

	gai = getaddrinfo(hostname, portstr, &hints, &res);
	if (gai != 0)
	{
		fprintf(stderr, "%s: %s\n", hostname, gai_strerror(gai));
		return 2;
	}

	if (!quiet)
		printf("tcping %s:%d\n", hostname, portnr);

	signal(SIGINT, handler);
	signal(SIGTERM, handler);

	while((curncount < count || count == -1) && stop == 0)
	{
		double rtt = 0.0;
		int connected = 0;
		struct addrinfo *ai;
		char ipstr[INET6_ADDRSTRLEN] = "";

		/* try the resolved addresses in order until one connects */
		for(ai = res; ai != NULL && stop == 0; ai = ai->ai_next)
		{
			if (connect_to(ai, timeout * 1000, &rtt) == 0)
			{
				connected = 1;
				getnameinfo(ai->ai_addr, ai->ai_addrlen, ipstr,
					    sizeof(ipstr), NULL, 0, NI_NUMERICHOST);
				break;
			}
		}

		if (connected)
		{
			if (ok == 0 || rtt < min)
				min = rtt;
			if (rtt > max)
				max = rtt;
			avg += rtt;
			ok++;
			printf("connected to %s:%d (%s), seq=%d time=%.2f ms\n",
			       hostname, portnr, ipstr, curncount, rtt);
		}
		else
		{
			err++;
			printf("error connecting to host: %s\n", strerror(errno));
		}

		curncount++;

		if (curncount != count && stop == 0)
			sleep(wait);
	}

	freeaddrinfo(res);

	if (!quiet)
	{
		double failed = 0.0;
		double avg_ms = 0.0;
		if (curncount > 0)
			failed = ((double)err / (double)curncount) * 100.0;
		if (ok > 0)
			avg_ms = avg / (double)ok;
		printf("--- %s:%d ping statistics ---\n", hostname, portnr);
		printf("%d connects, %d ok, %3.2f%% failed\n", curncount, ok, failed);
		printf("round-trip min/avg/max = %.1f/%.1f/%.1f ms\n", min, avg_ms, max);
	}

	if (ok)
		return 0;
	else
		return 127;
}
