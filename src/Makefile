# Source files
SRCS = gps-monitor.c gps.c display.c logger.c spi_display.c
OBJS = $(SRCS:.c=.o)

# U8g2 source files (statically linked)
U8G2_SRCS = $(wildcard u8g2/*.c)
U8G2_OBJS = $(U8G2_SRCS:.c=.o)

# Target binary
TARGET = gps-monitor

# Compiler flags - add u8g2 include path
CFLAGS += -Iu8g2

# Libraries (no u8g2 - statically linked)
LIBS = -lubus -lubox -lblobmsg_json -lncurses

# Build target
$(TARGET): $(OBJS) $(U8G2_OBJS)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJS) $(U8G2_OBJS) $(LIBS)

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Dependencies
gps-monitor.o: gps-monitor.c common.h gps.h display.h logger.h spi_display.h
gps.o: gps.c gps.h common.h
display.o: display.c display.h gps.h common.h
logger.o: logger.c logger.h gps.h common.h
spi_display.o: spi_display.c spi_display.h gps.h common.h

clean:
	rm -f $(TARGET) $(OBJS) $(U8G2_OBJS)

