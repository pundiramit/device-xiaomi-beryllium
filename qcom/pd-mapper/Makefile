PD_MAPPER := pd-mapper

CFLAGS := -Wall -g -O2
LDFLAGS := -lqrtr

prefix ?= /usr/local
bindir := $(prefix)/bin
servicedir := $(prefix)/lib/systemd/system

SRCS := pd-mapper.c \
        assoc.c \
        json.c \
	servreg_loc.c

OBJS := $(SRCS:.c=.o)

$(PD_MAPPER): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

pd-mapper.service: pd-mapper.service.in
	@sed 's+PD_MAPPER_PATH+$(bindir)+g' $< > $@

install: $(PD_MAPPER) pd-mapper.service
	@install -D -m 755 $(PD_MAPPER) $(DESTDIR)$(bindir)/$(PD_MAPPER)
	@install -D -m 644 pd-mapper.service $(DESTDIR)$(servicedir)/pd-mapper.service

clean:
	rm -f $(PD_MAPPER) $(OBJS) pd-mapper.service
