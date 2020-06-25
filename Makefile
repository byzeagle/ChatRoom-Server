CC=clang++
CFLAGS=-Wall -Wextra -O3 -pthread
DFLAGS=-Wall -Wextra -O0 -g -pthread
OPENSSL=-lcrypto -lssl

all: chat_server.exe

debug: chat_server.cpp
	$(CC) $(DFLAGS) chat_server.cpp -o chat_server.exe $(OPENSSL)

chat_server.exe: chat_server.cpp
	$(CC) $(CFLAGS) chat_server.cpp -o chat_server.exe

clean:
	rm chat_server.exe