CC=gcc
buildFolder=build
ifeq ($(OS),Windows_NT)
OUTPUTBIN = vlang.exe
else
OUTPUTBIN = vlang
endif

all:
ifeq ($(OS),Windows_NT)
	rmdir .\$(buildFolder)\ /s /q
else
	rm -rf $(buildFolder)
endif
	mkdir $(buildFolder)
	$(CC) -c -g interpret.c -o $(buildFolder)/interpret.o
	$(CC) -c -g parser.c -o $(buildFolder)/parser.o
	$(CC) -c -g libs/removeCharFromString.c -o $(buildFolder)/removeCharFromString.o
	$(CC) -c -g libs/startswith.c -o $(buildFolder)/startswith.o
	$(CC) -c -g libs/isInt.c -o $(buildFolder)/isInt.o
	$(CC) -c -g libs/isCharContainedInStr.c -o $(buildFolder)/isCharContainedInStr.o
	$(CC) -c -g main.c -o $(buildFolder)/main.o
	$(CC) -o $(OUTPUTBIN) $(buildFolder)/main.o $(buildFolder)/interpret.o $(buildFolder)/removeCharFromString.o $(buildFolder)/startswith.o $(buildFolder)/parser.o $(buildFolder)/isInt.o $(buildFolder)/isCharContainedInStr.o
ifeq ($(OS),Windows_NT)
	rmdir .\$(buildFolder)\ /s /q
else
	rm -rf $(buildFolder)
endif



install:
	cp vlang /usr/bin/vlang
run:
	./vlang test.vlang -d --llvm
clean:
	rm -rf $(buildFolder) vlang
