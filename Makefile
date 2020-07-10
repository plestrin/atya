CC := clang
CFLAGS := -Wall -Wextra -O3
LDFLAGS :=

all: atya

atya: main.o gory_sewer.o utile.o index.o simple_index.o hash.o
	$(CC) $(LDFLAGS) -o $@ $^

test: test_last_index

test_last_index: test_last_index.o last_index.o hash.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	@ rm -f *.o
	@ rm -f atya
	@ rm -f test_last_index
