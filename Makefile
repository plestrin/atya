CC := clang
CFLAGS := -Wall -Wextra -O3
LDFLAGS :=

all: build create exclude

build:
	mkdir -p build

create: build/main_create.o build/gory_sewer.o build/utile.o build/fast_index.o build/simple_index.o build/hash.o build/abs_storage.o build/skim.o build/simple_set.o build/llist.o build/abs_index.o
	$(CC) $(LDFLAGS) -o $@ $^

exclude: build/main_exclude.o build/gory_sewer.o build/utile.o build/last_index.o build/hash.o
	$(CC) $(LDFLAGS) -o $@ $^

build/%.o: src/%.c
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean
clean:
	@ rm -rf build
	@ rm -f create exclude
