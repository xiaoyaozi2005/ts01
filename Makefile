
PRODUCTS=libts01.a
all: $(PRODUCTS)

libts01.a : driver.o crc.o sim.o data.o
	ar cr $@ $< crc.o sim.o data.o

install: libts01.a
	install -m 644 libts01.h /usr/local/include
	install -d /usr/local/include/ts01
	install -m 644 data.h /usr/local/include/ts01
	install -m 644 driver.h /usr/local/include/ts01
	install -m 644 sim.h /usr/local/include/ts01
	install -m 644 libts01.a /usr/local/lib

uninstall:
	rm -f /usr/local/include/libts01.h
	rm -f /usr/local/include/ts01/data.h
	rm -f /usr/local/include/ts01/driver.h 
	rm -f /usr/local/include/ts01/sim.h 
	rm -f /usr/local/lib/libts01.a
	rm -r /usr/local/include/ts01


clean:
	rm -f $(PRODUCTS) *.o  *~


