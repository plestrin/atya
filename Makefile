CC := clang
CFLAGS := -Wall -Wextra -O3
LDFLAGS :=

atya: main.o gory_sewer.o utile.o index.o simple_index.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	@ rm -f *.o
	@ rm -f atya
