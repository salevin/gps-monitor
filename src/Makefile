# Source files
SRCS = gps-monitor.c gps.c display.c logger.c
OBJS = $(SRCS:.c=.o)

# Target binary
TARGET = gps-monitor

# Libraries
LIBS = -lubus -lubox -lblobmsg_json -lncurses

# Build target
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Dependencies
gps-monitor.o: gps-monitor.c common.h gps.h display.h logger.h
gps.o: gps.c gps.h common.h
display.o: display.c display.h gps.h common.h
logger.o: logger.c logger.h gps.h common.h

clean:
	rm -f $(TARGET) $(OBJS)

