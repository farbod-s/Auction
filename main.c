#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <netinet/in.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

// constant variables
#define TRUE   				1
#define FALSE  				0

#define STANDARD_INPUT		0
#define STANDARD_OUTPUT		1
#define STANDARD_ERROR		2

#define SERVER_CMD			"server"
#define CONNECT_CMD			"connect"
#define CLIENTS_LIST_CMD	"get_clients_list"
#define AUCTION_CMD			"auction"
#define OFFER_CMD			"offer"
#define END_AUCTION_CMD		"end_auction"
#define SYNC_CMD			"sync"

#define max_auctions 		100
#define max_clients 		10
#define max_nodes			100
#define max_input_char 		100
#define max_char 			100
#define port_len 			6
#define ip_len 				16
#define buffer_size 		1025
#define SUCCESS_INIT_MSG 	"Node is init successfully!\n"

// global variables
int client_socket[3];
int opt = TRUE;

// forward declaration
void init(int, int *, struct sockaddr_in *);
int sendConnectionRequest(char *, int, int *);

// structs
struct Node {
	char ip_address[ip_len];
	int port_number;
	char alias_name[max_char];
};

struct Auction {
	int id;
	char item_name[max_char];
	int min_price;
	char auctioner[max_char];
	char winner[max_char];
};

struct Offer {
	char item_name[max_char];
	int price;
	char offerer[max_char];
	int auction_id;
};

// main function
int main(int argc, char *argv[]) {
	// check input
	if (argc != 3) {
		// TODO print error
	} else {
		// TODO type checking
	}

	char *alias_name = argv[1];
	// TODO check alias_name
	int port_number = atoi(argv[2]);
	// TODO check port_number

	int master_socket;
	int addrlen;
	int new_socket;
	int activity, loop, loop2, valread;
	int connection_socket;
	char buffer[buffer_size];    /* data buffer of 1K */
	struct sockaddr_in address;
	fd_set readfds;

	char server_ip[ip_len], item_name[max_char];
	int server_port, price;

	char input[max_input_char];

	// client list
	struct Node nodes[max_nodes];
	int last_node = 0;

	// server
	struct Node server;

	// auction list
	struct Auction auctions[max_auctions];

	// last offer
	struct Offer best_offer;

	// self initialization
	init(port_number, &master_socket, &address);

	while (TRUE) {
		// setup which sockets to listen on
		FD_ZERO(&readfds);

		FD_SET(STANDARD_INPUT, &readfds);
		FD_SET(master_socket, &readfds);
		FD_SET(connection_socket, &readfds);

		// wait for connection, forever if we have to
		activity = select(FD_SETSIZE, &readfds, NULL, NULL, NULL);
		if ((activity < 0) && (errno != EINTR)) {
			// there was an error with select()
		}
		
		if (FD_ISSET(master_socket, &readfds)) {
			// open the new socket as 'new_socket'
			addrlen = sizeof(address);
			if ((new_socket = accept(master_socket, (struct sockaddr *)&address, &addrlen)) < 0 )
			{
				write(STANDARD_ERROR,"accept",6);
				exit(EXIT_FAILURE);
			}

			// transmit alias_name to new connection
			char *msg = malloc(strlen(SERVER_CMD) + 1 + strlen(alias_name) + (last_node) * (1 + ip_len + 1 + port_len + 1 + max_char));
			strcpy(msg, SERVER_CMD);
			strcat(msg, " ");
			strcat(msg, alias_name);
			for (loop = 0; loop < last_node; loop++) {
				char *port = malloc(port_len);
				sprintf(port, "%u", nodes[loop].port_number);

				strcat(msg, ";");
				strcat(msg, nodes[loop].ip_address);
				strcat(msg, " ");
				strcat(msg, port);
				strcat(msg, " ");
				strcat(msg, nodes[loop].alias_name);
			}

			if (send(new_socket, msg, strlen(msg), 0) != strlen(msg)) {
				write(STANDARD_ERROR, "send", 4);
			}

			// add new socket to list of sockets
			for (loop = 0; loop < max_clients; loop++) {
				if (client_socket[loop] == 0) {
					client_socket[loop] = new_socket;
					FD_SET(client_socket[loop], &readfds);
					loop = max_clients;
				}
			}
		}
 
		for (loop = 0; loop < max_clients; loop++) {
			if ((FD_ISSET(client_socket[loop], &readfds)) && (client_socket[loop] != 0)) {
				if ((valread = read(client_socket[loop], buffer, 1024)) < 0) {
					close(client_socket[loop]);
					client_socket[loop] = 0;
				} else {
					buffer[valread] = 0;

					char *temp_buffer = malloc(max_char);
					strcpy(temp_buffer, buffer);
					char *client_first_token = malloc(max_char);
					client_first_token = strtok(buffer, " ");

					// check client message
					if (strcmp(client_first_token, CONNECT_CMD) == 0) {

						char *ip = malloc(ip_len);
						int port;
						char *name = malloc(max_char);
						client_first_token = strtok(NULL, " ");
						sscanf(client_first_token, "%s", ip);
						client_first_token = strtok(NULL, " ");
						sscanf(client_first_token, "%d", &port);
						client_first_token = strtok(NULL, " ");
						sscanf(client_first_token, "%s", name);
						
						strcpy(nodes[last_node].alias_name, name);
						strcpy(nodes[last_node].ip_address, ip);
						nodes[last_node].port_number = port;
						last_node++;

						char *temp_str = malloc(max_char);
						sprintf(temp_str, "New Node: Name=%s, IP=%s, Port=%d\n",
							name, ip, port);
						write(STANDARD_OUTPUT, temp_str, max_char);

						// send new client data to other clients
						for (loop2 = 0; loop2 < max_clients; loop2++) {
							if ((loop2 != loop) && (client_socket[loop2] != 0)) {
								if (send(client_socket[loop2], temp_buffer, strlen(temp_buffer), 0) != strlen(temp_buffer)) {
									write(STANDARD_ERROR, "send", 4);
								}

							}
						}

						// send new client data to server
						if (connection_socket > 0) {
							if (send(connection_socket, temp_buffer, strlen(temp_buffer), 0) != strlen(temp_buffer)) {
								write(STANDARD_ERROR, "send", 4);
							}

						}
					}
				}
			}
		}

		if ((FD_ISSET(connection_socket, &readfds)) && (connection_socket != 0)) {

			if ((valread = read(connection_socket, buffer, 1024)) < 0) {
				close(connection_socket);
				connection_socket = 0;
			} else {
				buffer[valread] = 0;

				char *name = malloc(max_char);
				char *server_first_token = malloc(max_char);
				char *temp_buffer = malloc(max_char);
				strcpy(temp_buffer,buffer);

				server_first_token = strtok(buffer, " ");

				// check server message
				if (strcmp(server_first_token, CONNECT_CMD) == 0) {
					char *ip = malloc(ip_len);
					int port;
					server_first_token = strtok(NULL, " ");
					sscanf(server_first_token, "%s", ip);
					server_first_token = strtok(NULL, " ");
					sscanf(server_first_token, "%d", &port);
					server_first_token = strtok(NULL, " ");
					sscanf(server_first_token, "%s", name);
					
					strcpy(nodes[last_node].alias_name, name);
					strcpy(nodes[last_node].ip_address, ip);
					nodes[last_node].port_number = port;
					last_node++;

					char *temp_str = malloc(max_char);
					sprintf(temp_str, "New Node: Name=%s, IP=%s, Port=%d\n",
						name, ip, port);
					write(STANDARD_OUTPUT, temp_str, max_char);
					
					// send new client data to clients
					for (loop = 0; loop < max_clients; loop++) {
						send(client_socket[loop], buffer, strlen(buffer), 0);
					}
				} else if (strcmp(server_first_token, SERVER_CMD) == 0) {
					server_first_token = strtok(NULL, ";");
					sscanf(server_first_token, "%s", name);
					strcpy(nodes[last_node].alias_name, name);
					last_node++;

					char *remote_ip = malloc(ip_len);
					int remote_port;

					server_first_token = strtok(temp_buffer, ";");	//ignore first token

					server_first_token = strtok(NULL, ";");
					while (server_first_token != NULL) {
						
						sscanf(server_first_token, "%s %d %s", remote_ip, &remote_port, name);
						
						strcpy(nodes[last_node].alias_name, name);
						strcpy(nodes[last_node].ip_address, remote_ip);
						nodes[last_node].port_number = remote_port;
						last_node++;

						server_first_token = strtok(NULL, ";");
					}

					// send selfie data to server!
					char *ip = inet_ntoa(address.sin_addr);
					char *port = malloc(port_len);
					sprintf(port, "%u", port_number);
					char *msg = malloc(strlen(CONNECT_CMD) + 1 + strlen(ip) + 1 + strlen(port) + 1 + strlen(alias_name) + 1);
					strcpy(msg, CONNECT_CMD);
					strcat(msg, " ");
					strcat(msg, ip);
					strcat(msg, " ");
					strcat(msg, port);
					strcat(msg, " ");
					strcat(msg, alias_name);
					if (send(connection_socket, msg, strlen(msg), 0) != strlen(msg)) {
						write(STANDARD_ERROR, "send", 4);
					}

				}
			}
		}
		
		// check user input
		if (FD_ISSET(STANDARD_INPUT, &readfds)) {
			if ((valread = read(STANDARD_INPUT, input, max_input_char)) > 0) {
				input[valread] = 0;

				if (strcmp(input, CLIENTS_LIST_CMD) >= 0) {
					for (loop = 0; loop < last_node; loop++) {
						char *temp_str = malloc(max_char);
						sprintf(temp_str,"Node: Name=%s, IP=%s, Port=%d\n",
							nodes[loop].alias_name,
							nodes[loop].ip_address,
							nodes[loop].port_number);
						write (STANDARD_OUTPUT, temp_str, max_char);
					}
				} else if (strcmp(input, SYNC_CMD) == 0) {
					// TODO
				} else {
					char *input_first_token = malloc(max_char);
					input_first_token = strtok(input, " ");
					
					// check commands
					if (strcmp(input_first_token, CONNECT_CMD) == 0) {
						input_first_token = strtok(NULL, " ");
						sscanf(input_first_token, "%s", server_ip);
						input_first_token = strtok(NULL, " ");
						sscanf(input_first_token, "%d", &server_port);
						
						if (sendConnectionRequest(server_ip, server_port, &connection_socket) == 0) {
							strcpy(nodes[last_node].ip_address, server_ip);
							nodes[last_node].port_number = server_port;
						}
					} else if (strcmp(input_first_token, AUCTION_CMD) == 0) {
						sscanf(input, "%*s %s %d", item_name, &price);
						// TODO
					} else if (strcmp(input_first_token, OFFER_CMD) == 0) {
						sscanf(input, "%*s %s %d", item_name, &price);
						// TODO
					} else if (strcmp(input_first_token, END_AUCTION_CMD) == 0) {
						sscanf(input, "%*s %s", item_name);
						// TODO
					}
				}
			}
		}
		
	}

	return 0;
}

void init(int port_number, int *master_socket, struct sockaddr_in *address) {
	// make non-blocking STANDARD_INPUT
	//int flags = fcntl(STANDARD_INPUT, F_GETFL, 0);
	//fcntl(STANDARD_INPUT, F_SETFL, flags | O_NONBLOCK);

	// initialise all client_socket[] to 0 so not checked
	int loop;
	for (loop = 0; loop < max_clients; loop++) {
		client_socket[loop] = 0;
	}

	// create the master socket and check it worked
	if (((*master_socket) = socket(AF_INET, SOCK_STREAM,0)) == 0) {
		write(STANDARD_ERROR, "socket", 6);
		exit(EXIT_FAILURE);
	}

	// set master socket to allow multiple connections
	if (setsockopt((*master_socket), SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
		write(STANDARD_ERROR, "setsockopt", 10);
		exit(EXIT_FAILURE);
	}

	// type of socket created
	(*address).sin_family = AF_INET;
	(*address).sin_addr.s_addr = INADDR_ANY;
	(*address).sin_port = htons(port_number);
	
	// bind the socket to port
	if (bind((*master_socket), (struct sockaddr *)address, sizeof((*address))) < 0) {
		write(STANDARD_ERROR, "bind", 4);
		exit(EXIT_FAILURE);
	}

	// try to specify maximum of 3 pending connections for the master socket
	if (listen((*master_socket), max_clients) < 0) {
		write(STANDARD_ERROR, "listen", 6);
		exit(EXIT_FAILURE);
	}

	// display success message
	write(STANDARD_OUTPUT, SUCCESS_INIT_MSG, strlen(SUCCESS_INIT_MSG));
}

int sendConnectionRequest(char *remote_ip, int remote_port, int *fd) {
	struct sockaddr_in server_addr;
	(*fd) = socket(AF_INET, SOCK_STREAM, 0);
	if ((*fd) == -1) {
		write(STANDARD_ERROR, "Error: Create Client Socket\n", 21);
		return -1;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(remote_port);
	server_addr.sin_addr.s_addr = inet_addr(remote_ip);

	if (connect((*fd), (struct sockaddr *)&server_addr,sizeof(server_addr)) == -1) {
		write(STANDARD_ERROR, "Error: Client Connection\n", 25);
		return -1;
	}

	return 0;
}
