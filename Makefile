LDFLAGS+=
CFLAGS+=-O3 -Wall

VERSION=0.6

OBJS=main.o tcp.o

.PHONY: all install clean package

all: tcping

tcping: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o tcping
	$(if $(STRIP),$(STRIP) tcping,strip tcping)

install: tcping
	mkdir -p $(DESTDIR)/usr/bin
	cp tcping $(DESTDIR)/usr/bin

clean:
	rm -f $(OBJS) core tcping

package: clean
	# source package
	rm -rf tcping-$(VERSION)*
	mkdir tcping-$(VERSION)
	cp *.c *.h Makefile readme.txt license.txt tcping-$(VERSION)
	tar czf tcping-$(VERSION).tgz tcping-$(VERSION)
	rm -rf tcping-$(VERSION)
