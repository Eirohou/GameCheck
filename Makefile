CC ?= gcc
CFLAGS ?= -Wall -Wextra -Wno-format-truncation -O2 -std=c99 -D_GNU_SOURCE
LDFLAGS ?= $(shell pkg-config --libs libcurl 2>/dev/null || echo "-lcurl")
CURL_CFLAGS ?= $(shell pkg-config --cflags libcurl 2>/dev/null)

TARGET = gamecomp
SRC_DIR = src
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/sysinfo.c \
       $(SRC_DIR)/req_parser.c \
       $(SRC_DIR)/steam_api.c \
       $(SRC_DIR)/checker.c \
       $(SRC_DIR)/utils.c \
       $(SRC_DIR)/cJSON.c

OBJS = $(SRCS:.c=.o)
PREFIX ?= /usr/local

.PHONY: all clean strip install uninstall test

all: $(TARGET) strip

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(CURL_CFLAGS) -c $< -o $@

strip: $(TARGET)
	strip --strip-all $(TARGET) 2>/dev/null || true

debug: CFLAGS = -Wall -Wextra -g -O0 -std=c99 -D_GNU_SOURCE
debug: clean $(TARGET)

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
