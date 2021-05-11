CC := clang
CFLAGS := -Wall -Wextra -O3
LDFLAGS :=

all: build intersect exclude

build:
	mkdir -p build

intersect: build/main_intersect.o build/gory_sewer.o build/utile.o build/fast_index.o build/simple_index.o build/hash.o build/abs_storage.o build/skim.o build/simple_set.o build/llist.o build/abs_index.o
	$(CC) $(LDFLAGS) -o $@ $^

exclude: build/main_exclude.o build/gory_sewer.o build/utile.o build/last_index.o build/hash.o
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
	@ rm -f intersect exclude
	@ rm -f test_last_index
