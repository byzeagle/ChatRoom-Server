CC=clang++
CFLAGS=-Wall -Wextra -O0 -g -pthread

all: chat_server.exe

rebuild: clean chat_server.exe

chat_server.exe: chat_server.cpp
	$(CC) $(CFLAGS) chat_server.cpp -o chat_server.exe

clean:
	rm chat_server.exe