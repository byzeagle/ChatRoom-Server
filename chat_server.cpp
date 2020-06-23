
/*
*	Copyright 2020
*
*	Author: 			Ugur Buyukdurak
*	Description: 		Simple Chatroom written in C++
*	Version:			1.0
*
*/

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <unistd.h> 
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <thread>
#include <atomic>
#include <algorithm>
#include <mutex>

#include <boost/tokenizer.hpp>

constexpr int PORT = 8080;
constexpr int BACKLOG = 10;
constexpr int BUFFERSIZE = 2048;
constexpr int MAX_CLIENTS = 100;

struct Client {
	struct sockaddr_in addr;
	int connfd;
	int userid;
	std::string name;

	bool operator == (const Client & cli){
		return this->userid == cli.userid ? true : false;
	}

	bool operator != (const Client & cli) const{
		return this->userid != cli.userid ? true : false;
	}
};

std::mutex mtx;

static std::atomic<int> client_count {0};

static int userid = 1;

static std::vector<Client> clients;

static const char * error_msg = "<< Unkown command\n";

// Socket Initialization
int initialize_socket(int port){
	int sockfd;
	struct sockaddr_in servaddr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0){
		perror("socket");
		exit(EXIT_FAILURE);
	}

	bzero(&servaddr, sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	if(bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
		perror("bind");
		exit(EXIT_FAILURE);
	}

	if(listen(sockfd, BACKLOG)){
		perror("listen");
		exit(EXIT_FAILURE);
	}

	return sockfd;
}

using namespace std;

// Terminate the server
void __SYSTEM_EXIT__(){
	for(const auto & client : clients)
		close(client.connfd);
	exit(EXIT_SUCCESS);
}

// Send message to everybody
void broadcast_message(const string & message){
	mtx.lock();
	for(const auto & client : clients){
		if(write(client.connfd, message.c_str(), strlen(message.c_str())) < 0){
			perror("Write descriptor failed");
			break;
		}
	}
	mtx.unlock();
}

// Send message to everybody except the client
void send_message_to_others(const string & message, const Client & client){
	mtx.lock();
	for(const auto & cli : clients){
		if(cli != client){
			if(write(cli.connfd, message.c_str(), strlen(message.c_str())) < 0){
				perror("send_message_to_others failed");
				break;
			}
		}
	}
	mtx.unlock();
}

// Send message to the client
void send_message_to_self(const string & message, const Client client){
	if (write(client.connfd, message.c_str(), strlen(message.c_str())) < 0) {
        perror("Self message failed");
        exit(EXIT_FAILURE);
    }
}

// Return list of clients logged in to the system
void send_list_of_clients(const Client & client){
	mtx.lock();
	for(const auto & cli : clients){
		send_message_to_self("<< [" + std::to_string(cli.userid) + "] " + cli.name + "\r\n", client);
	}
	mtx.unlock();
}

// Send message to a specific client
void send_to_specific_client(const string & message, int userid){
	mtx.lock();
	for(const auto & cli : clients){
		if(cli.userid == userid){
			if(write(cli.connfd, message.c_str(), strlen(message.c_str())) < 0){
				perror("Send to specific client");
				break;
			}
		}
	}
	mtx.unlock();
}

// Main function to handle the incoming connections
void handle_client(Client client){
	char buffer[BUFFERSIZE] = {'\0'};

	client_count++;

	// For displaying IPs
	char ip_address[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(client.addr.sin_addr), ip_address, INET_ADDRSTRLEN);

	cout << "<< client accepted referenced by " << client.userid << ", ";
	cout << "ip: " << ip_address << endl;

	broadcast_message("<< " + std::to_string(client.userid) + " has joined in\r\n");
	send_message_to_self("<< see /help for help\r\n", client);

	// Main loop to read the incoming messages.
	while(read(client.connfd, buffer, sizeof(buffer)) > 0){
		string incoming_message(buffer);
		string outgoing_message("[" + client.name + "] " + incoming_message);

		if((strlen(buffer) == 0))
			continue;

		if(incoming_message[0] == '/'){
			if(incoming_message.find("/quit") != std::string::npos){
				break;
			}
			else if (incoming_message.find("/ping") != std::string::npos){
				send_message_to_self("<< pong\r\n", client);
			}
			else if (incoming_message.find("/nick") != std::string::npos){
				using namespace boost;
				tokenizer<>tok(incoming_message);
				auto beg = tok.begin();
				client.name = *(++beg);
				auto pos = std::find(clients.begin(), clients.end(), client);
				(*pos).name = client.name;

				broadcast_message("<< " + std::to_string((*pos).userid) + " is known as " + (*pos).name + "\r\n");
			}
			else if (incoming_message.find("/msg") != std::string::npos){
				using namespace boost;
				tokenizer<>tok(incoming_message);
				string message = "";
				int userid = -1;
				for(auto beg = tok.begin(); beg != tok.end(); ++beg){
					if(std::distance(tok.begin(), beg) > 1){
						message += *beg;
						message += " ";
					}
					if(std::distance(tok.begin(), beg) == 1){
						userid = std::stoi(*beg);
					}
				}

				message = "[PM] " + string("[") + client.name + "] " + message + "\r\n";

				send_to_specific_client(message, userid);

			}
			else if(incoming_message.find("/list") != std::string::npos){
				string list = "<< clients " + std::to_string(client_count) + "\r\n";
				send_message_to_self(list, client);
				send_list_of_clients(client);
			}
			else if(incoming_message.find("/help") != std::string::npos){
				string help( "<< /quit	Quit chatroom\r\n"
							 "<< /ping	Server Test\r\n"
							 "<< /nick	Change nick\r\n"
							 "<< /msg 	Send private message\r\n"
							 "<< /list 	Show clients in the chat\r\n"
							 "<< /help 	Show help\r\n");

				send_message_to_self(help, client);
			}
			else if(incoming_message.find("/SERVER_EXIT") != std::string::npos){
				__SYSTEM_EXIT__();
			}
			else{
				send_message_to_self(error_msg, client);
			}
		}else{
			send_message_to_others(outgoing_message, client);
		}
		bzero(buffer, sizeof(buffer));
	}

	cout << "<< quit referenced by " << client.userid << ", ";
	cout << "ip: " << ip_address << endl;
	client_count--;
	clients.erase(std::remove(clients.begin(), clients.end(), client), clients.end());

	close(client.connfd);
}

int main(void) {

	/*	Socket initialization */
	int sockfd = initialize_socket(PORT);
	struct sockaddr_in cli_addr;
	int connfd;

	cout << "[Server Started]" << endl;

	while(true){
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

		if(connfd < 0){
			perror("accept");
			exit(EXIT_FAILURE);
		}

		if((client_count + 1) == MAX_CLIENTS) {
			cout << "<< Max clients reached" << endl;
			cout << "<< Rejected " << endl;
			close(connfd);
			continue;
		}

		Client client;
		client.addr = cli_addr;
		client.connfd = connfd;
		client.userid = userid++;
		client.name = to_string(client.userid);
		clients.push_back(client);

		thread (handle_client, client).detach();

		sleep(1);
	}

    return 0; 
}