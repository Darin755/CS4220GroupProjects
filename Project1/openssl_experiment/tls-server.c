// Jacob Hartt, Derin Rawson, Lea Karsanbhai
// Serena Sullivan
// CS4220.002
// 04/17/2025

#include <stdio.h>


// #include <openssl/bio.h>
#include <arpa/inet.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
// #include <openssl/err.h>


#include "tls-server.h"
#include "tls-utils.h"


int main() {
	// // Initialize SSL with no options, no settings
	// if (!OPENSSL_init_ssl(0, NULL)) {
    //     fprintf(stderr, "Failed to initialize OpenSSL\n");
    //     return 1;
    // } else {
	// 	printf("Successfully initialized OpenSSL\n");
	// }

	char ip_str[INET_ADDRSTRLEN];
	int port = 55555;

	// Setup IPv4 streaming socket with default protocol
	int serv_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (serv_sock < 0)
    	die("socket failed to initialize");
	else
		printf("socket initialized\n");

	// Force socket to allow reuse of local addresses at the socket level, but not network level
	int true_var = 1;
	if (setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &true_var, sizeof(true_var)) < 0)
		die("failed to set socket options");
	else 
		printf("socket configured with SO_REUSEADDR\n");
	
	// Configure server address port
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr)); // Set everything in the structure to 0
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // host to network long
	serv_addr.sin_port = htons(port); // host to network short

	if (bind(serv_sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		die("failed to bind socket");
	else
		printf("successfully bound socket to address\n");

	// Start listening to server socket with a backlog of 128
	if (listen(serv_sock, 128) < 0)
		die("failed to begin listening to socket");
	else
		printf("started listening to socket\n");

	int client_sock;
	struct sockaddr_in peer_addr;
	socklen_t peer_addr_len = sizeof(peer_addr);

	struct pollfd poll_set[2]; // make a set of two polls
	memset(&poll_set, 0, sizeof(poll_set)); // init fd_set to 0

	poll_set[0].fd = STDIN_FILENO; // Set first poll to stdin
	poll_set[0].events = POLLIN; // configure to read data

	// if (ssl_init("server.crt", "server.key") < 0)
	// 	die("failed to init ssl");
	// else
	// 	printf("initialized ssl");
	ssl_init("tls/server.crt", "tls/server.key");

	while (1) {
		printf("waiting for next connction on port %d\n", port);

		// Awaits connection
		client_sock = accept(serv_sock, (struct sockaddr *) &peer_addr, &peer_addr_len);
		if (client_sock < 0)
			die("failed to accept connection on socket");
		else
			printf("accepted connection on client socket\n");

		ssl_client_init(&client, client_sock, SSLMODE_SERVER);

		// Wrtie peer address into ip_str and print it out
		inet_ntop(peer_addr.sin_family, &peer_addr.sin_addr, ip_str, INET_ADDRSTRLEN);		
		printf("new connection from %s:%d\n", ip_str, ntohs(peer_addr.sin_port));

		// set second poll to client_sock
		poll_set[1].fd = client_sock;

		// === event loop ===
		// configure events for all things
    poll_set[1].events = POLLERR | POLLHUP | POLLNVAL | POLLIN;
#ifdef POLLRDHUP
    fd_set[1].events |= POLLRDHUP;
#endif

		while (1) { // temp holding pattern to test connection with client
			poll_set[1].events &= ~POLLOUT; // Remove pollout from the events
			poll_set[1].events |= (ssl_client_wants_write(&client) ? POLLOUT : 0); // if clients wants a write, add pollout back

			// specify a poll to wait for data on the first socket, with two overall sockets, with no timeout
			int n_ready = poll(&poll_set[0], 2, -1);

			if (n_ready == 0)
				continue; // no sockets are giving info
			
			// get the returned events from client and test to if (make sure the events and data line up
			int revents = poll_set[1].revents;
			if (revents & POLLIN)
				if (enc_sock_read() == -1)
					break;
			if (revents & POLLOUT)
				if (enc_sock_write() == -1)
					break;
			if (revents & (POLLERR | POLLHUP | POLLNVAL))
				break;
#ifdef POLLRDHUP
			if (revents & POLLRDHUP)
				break;
#endif
			if (poll_set[0].revents & POLLIN)
				stdin_read();
			if (client.encrypt_len > 0)
				encrypt();
		}

		close(poll_set[1].fd);
		ssl_client_cleanup(&client);
	}

	return 0;
}


void ssl_client_cleanup(struct ssl_client *p)
{
  SSL_free(p->ssl); // free the ssl object
  free(p->write_buf); // free the write buffer
  free(p->encrypt_buf); // free the encrypt buffer
}
