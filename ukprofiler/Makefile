CC = gcc
TARGET = ukprofiler
SRCS = ukprofiler.c
OBJS = $(SRCS:.c=.o)
LIBS = -lvmi
INCLUDES = 
LIBDIRS = 
CFLAGS = -Wall -Wextra -O2 $(INCLUDES)
LDFLAGS = $(LIBDIRS) $(LIBS)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: install
install: $(TARGET)
	install -m 0755 $(TARGET) /usr/local/bin/

.PHONY: uninstall
uninstall:
	rm -f /usr/local/bin/$(TARGET)
