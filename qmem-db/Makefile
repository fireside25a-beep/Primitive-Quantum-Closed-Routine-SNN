CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Wno-misleading-indentation -Wno-deprecated-declarations -Werror
INCS = -Isrc -I../include
.PHONY: all test clean sanitize
all: bin/qmem bin/qmem-probe
bin/qmem: src/main.c src/qmem.c src/qmem.h ../core/primitiveq.c
	mkdir -p bin
	$(CC) $(CFLAGS) $(INCS) src/main.c src/qmem.c ../core/primitiveq.c -lsqlite3 -lcrypto -o $@
bin/qmem-probe: tests/probe.c src/qmem.c src/qmem.h
	mkdir -p bin
	$(CC) $(CFLAGS) $(INCS) tests/probe.c src/qmem.c -lcrypto -o $@
test: all
	python3 tests/hash_crc.py
	python3 tests/check.py
	python3 tests/security_io.py
sanitize:
	mkdir -p bin
	$(CC) -std=c11 -g -O1 -Wall -Wextra -Wpedantic -Wno-misleading-indentation -Wno-deprecated-declarations -Werror -fno-omit-frame-pointer -fsanitize=address,undefined $(INCS) src/main.c src/qmem.c ../core/primitiveq.c -lsqlite3 -lcrypto -o bin/qmem-sanitize
	ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 python3 tests/check.py --binary bin/qmem-sanitize
clean:
	$(RM) bin/qmem bin/qmem-sanitize bin/qmem-probe
