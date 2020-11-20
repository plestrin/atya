CC := clang
CFLAGS := -Wall -Wextra -O3
LDFLAGS :=

all: build atya

build:
	mkdir -p build

atya: build/main.o build/gory_sewer.o build/utile.o build/fast_index.o build/simple_index.o build/last_index.o build/hash.o build/abs_storage.o build/skim.o build/simple_set.o build/llist.o
	$(CC) $(LDFLAGS) -o $@ $^

.PHONY: test
test: build test_last_index

test_last_index: build/test_last_index.o build/last_index.o build/hash.o
	$(CC) $(LDFLAGS) -o $@ $^

build/%.o: src/%.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/%.o: test/%.c
	$(CC) $(CFLAGS) -o $@ -c $< -Isrc

.PHONY: clean
clean:
	@ rm -rf build
	@ rm -f atya
	@ rm -f test_last_index
