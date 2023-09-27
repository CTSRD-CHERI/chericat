BIN=./bin
SRC=./src
DEPS=./includes

CC=cc
CFLAGS=-g -Wall -Wcheri -O0

INC=-I/usr/include -I/usr/local/include -I/home/psjm3/util_libs/libelf/include/elf -I$(DEPS)
LDFLAGS=-L/usr/lib -L/usr/local/lib -L/home/psjm3/releng/22.12/lib/libelf

.PHONY: all
.PHONY: make_dir
.PHONY: clean

TARGET=$(BIN)/chericat
all: make_dir $(TARGET)

make_dir: $(BIN)/

$(BIN)/:
	mkdir -p $@

$(TARGET): $(SRC)/chericat.c $(BIN)/mem_scan.o $(BIN)/db_process.o $(BIN)/cap_capture.o $(BIN)/elf_utils.o $(BIN)/ptrace_utils.o $(BIN)/data_presentation.o
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) -DSQLITE_MEMDEBUG -lelf -lprocstat -lsqlite3 -lxo -o $(TARGET) $(BIN)/mem_scan.o $(BIN)/elf_utils.o $(BIN)/ptrace_utils.o $(BIN)/cap_capture.o $(BIN)/db_process.o $(BIN)/data_presentation.o $(SRC)/chericat.c

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

$(BIN)/data_presentation.o: $(SRC)/data_presentation.c $(DEPS)/data_presentation.h
	$(CC) $(CFLAGS) $(INC) -c -o $(BIN)/data_presentation.o $(SRC)/data_presentation.c

clean:
	rm -rf $(BIN)/*.o $(BIN)/chericat
