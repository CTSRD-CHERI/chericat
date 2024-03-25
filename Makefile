BIN=./bin
SRC=./src
DEPS=./includes

CC?=cc
CFLAGS+=-g -O0
CFLAGS+=-Wall -Wcheri

INC=-I/usr/include -I/usr/local/include -I$(DEPS) -I/usr/src/libexec/rtld-elf -I/usr/src/libexec/rtld-elf/aarch64
LDFLAGS=-L/usr/lib -L/usr/local/lib 

.PHONY: all
.PHONY: make_dir
.PHONY: clean

TARGET=$(BIN)/chericat
all: make_dir $(TARGET)

make_dir: $(BIN)/

$(BIN)/:
	mkdir -p $@

$(TARGET): $(SRC)/chericat.c $(BIN)/common.o $(BIN)/mem_scan.o $(BIN)/db_process.o $(BIN)/cap_capture.o $(BIN)/elf_utils.o $(BIN)/ptrace_utils.o $(BIN)/vm_caps_view.o $(BIN)/caps_syms_view.o $(BIN)/rtld_linkmap_scan.o
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) -DSQLITE_MEMDEBUG -lelf -lprocstat -lsqlite3 -lxo -o $(TARGET) $(BIN)/common.o $(BIN)/mem_scan.o $(BIN)/elf_utils.o $(BIN)/ptrace_utils.o $(BIN)/cap_capture.o $(BIN)/db_process.o $(BIN)/vm_caps_view.o $(BIN)/caps_syms_view.o $(BIN)/rtld_linkmap_scan.o $(SRC)/chericat.c

$(BIN)/common.o: $(SRC)/common.c
	$(CC) $(CFLAGS) $(INC) -c -o $(BIN)/common.o $(SRC)/common.c
$(BIN)/mem_scan.o: $(SRC)/mem_scan.c
	$(CC) $(CFLAGS) $(INC) -c -o $(BIN)/mem_scan.o $(SRC)/mem_scan.c

$(BIN)/ptrace_utils.o: $(SRC)/ptrace_utils.c
	$(CC) $(CFLAGS) $(INC) -c -o $(BIN)/ptrace_utils.o $(SRC)/ptrace_utils.c

$(BIN)/elf_utils.o: $(SRC)/elf_utils.c
	$(CC) $(CFLAGS) $(INC) -c -o $(BIN)/elf_utils.o $(SRC)/elf_utils.c

$(BIN)/db_process.o: $(SRC)/db_process.c $(DEPS)/db_process.h
	$(CC) $(CFLAGS) $(INC) -DSQLITE_MEMDEBUG -c -o $(BIN)/db_process.o $(SRC)/db_process.c

$(BIN)/cap_capture.o: $(SRC)/cap_capture.c $(DEPS)/cap_capture.h
	$(CC) $(CFLAGS) $(INC) -DSQLITE_MEMDEBUG -c -o $(BIN)/cap_capture.o $(SRC)/cap_capture.c

$(BIN)/vm_caps_view.o: $(SRC)/vm_caps_view.c $(DEPS)/vm_caps_view.h
	$(CC) $(CFLAGS) $(INC) -c -o $(BIN)/vm_caps_view.o $(SRC)/vm_caps_view.c

$(BIN)/caps_syms_view.o: $(SRC)/caps_syms_view.c $(DEPS)/caps_syms_view.h
	$(CC) $(CFLAGS) $(INC) -c -o $(BIN)/caps_syms_view.o $(SRC)/caps_syms_view.c

$(BIN)/rtld_linkmap_scan.o: $(SRC)/rtld_linkmap_scan.c
	$(CC) $(CFLAGS) $(INC) -c -o $(BIN)/rtld_linkmap_scan.o $(SRC)/rtld_linkmap_scan.c

TEST=./tests

test: $(TEST)/db_process_test.c $(BIN)/common.o
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) -DSQLITE_MEMDEBUG -lsqlite3 -o $(TARGET) $(BIN)/common.o $(TEST)/db_process_test.c -o $(BIN)/db_process_test

clean:
	rm -rf $(BIN)/*.o $(BIN)/chericat
