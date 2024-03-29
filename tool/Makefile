
PRODUCTS= write_address
all: $(PRODUCTS)
LDLIBS = -lts01  -lpthread


install: write_address
	install	$< /usr/local/bin

clean:
	rm -f $(PRODUCTS) *.o  *~


