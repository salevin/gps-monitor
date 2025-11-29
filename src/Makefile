all: gps-monitor gps-logger

gps-monitor: gps-monitor.c
	$(CC) $(CFLAGS) -o gps-monitor gps-monitor.c $(LDFLAGS) -lubus -lubox -lblobmsg_json -lncurses

gps-logger: gps-logger.c
	$(CC) $(CFLAGS) -o gps-logger gps-logger.c $(LDFLAGS) -lubus -lubox -lblobmsg_json

clean:
	rm -f gps-monitor gps-logger

