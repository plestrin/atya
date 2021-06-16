CC := clang
CFLAGS := -Wall -Wextra -O3
LDFLAGS :=

all: build create exclude

.PHONY: test
test: build test1_stree test2_stree test3_stree

build:
	mkdir -p build

create: build/main_create.o build/gory_sewer.o build/utile.o build/simple_index.o build/hash.o build/abs_storage.o build/skim.o build/simple_set.o build/llist.o build/stree.o build/reverse_index.o
	$(CC) $(LDFLAGS) -o $@ $^

exclude: build/main_exclude.o build/gory_sewer.o build/utile.o build/last_index.o build/hash.o
	$(CC) $(LDFLAGS) -o $@ $^

test1_stree: build/test1_stree.o build/stree.o
	$(CC) $(LDFLAGS) -o $@ $^

test2_stree: build/test2_stree.o build/stree.o
	$(CC) $(LDFLAGS) -o $@ $^

test3_stree: build/test3_stree.o build/stree.o build/utile.o build/gory_sewer.o
	$(CC) $(LDFLAGS) -o $@ $^

build/%.o: src/%.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/%.o: test/%.c
	$(CC) $(CFLAGS) -g -o $@ -c $< -I src/

.PHONY: clean
clean:
	@ rm -rf build
	@ rm -f create exclude test1_stree test2_stree test3_stree
