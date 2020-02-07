TQFTPSERV := tqftpserv

CFLAGS := -Wall -g -O2
LDFLAGS := -lqrtr

prefix ?= /usr/local
bindir := $(prefix)/bin
servicedir := $(prefix)/lib/systemd/system

SRCS := tqftpserv.c translate.c

OBJS := $(SRCS:.c=.o)

$(TQFTPSERV): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

tqftpserv.service: tqftpserv.service.in
	@sed 's+TQFTPSERV_PATH+$(bindir)+g' $< > $@

install: $(TQFTPSERV) tqftpserv.service
	@install -D -m 755 $(TQFTPSERV) $(DESTDIR)$(bindir)/$(TQFTPSERV)
	@install -D -m 644 tqftpserv.service $(DESTDIR)$(servicedir)/tqftpserv.service

clean:
	rm -f $(TQFTPSERV) $(OBJS) tqftpserv.service
