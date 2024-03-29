
PRODUCTS= test as_test sim read write #changeIP
all: $(PRODUCTS)
LDLIBS = -lts01 -lktl -lktlrt -lpthread


install: libts01.a
	install libts01.h /usr/local/include
	install libts01.a /usr/local/lib
	install	$< /usr/local/bin

clean:
	rm -f $(PRODUCTS) *.o  *~


