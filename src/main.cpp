/// @file main.cpp
/// @brief OSC Interface main file
/// @Author: Zedro

#include "../inc/tinyosc.h"
#include <asm-generic/socket.h>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/select.h>
#include <sys/socket.h>

#define BUF_SIZE 2048
#define PORT 9000

// Volatile allows for external modification
static volatile bool running = true;

static void SIGINT_handler(int sig);

int main(void) {
	// Buffer to read packet data
	char buf[BUF_SIZE];

	// Dummy Test
	char bin[4] = {0x01, 0x11, 0x33, 0x66};
	int len = tosc_writeMessage(
		buf, BUF_SIZE, "/test", "fsibTFNI", 1.0f, "yo whirl!", -42, sizeof(bin), bin);
	tosc_printOscBuffer(buf, len);

	// SIGINT handler
	signal(SIGINT, &SIGINT_handler);

	// UDP Server Setup
	try {
		const int fd = socket(AF_INET, SOCK_DGRAM, 0); // Create socket
		if (fd == -1)
			throw std::runtime_error("Failed to create socket");
		// Set it to non-blocking
		if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
			close(fd);
			throw std::runtime_error("Failed to set socket to non-blocking");
		}
		// Set socket to reuse
		int reuse = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) ==
			-1) {
			close(fd);
			throw std::runtime_error("Failed to set socket to reuse");
		}
		struct sockaddr_in socket_in; // Crea Socket address & set values
		socket_in.sin_family = AF_INET;
		socket_in.sin_port = htons(PORT); // Bind socket to port
		socket_in.sin_addr.s_addr = htonl(INADDR_ANY);
		// Bind socket
		if (bind(fd, (struct sockaddr *)(&socket_in), sizeof(socket_in)) == -1) {
			close(fd);
			throw std::runtime_error("Failed to bind socket");
		}

		// Main Loop
		while (running) {
			fd_set readFds;
			FD_ZERO(&readFds);
			FD_SET(fd, &readFds);
			struct timeval timeout{1, 0};

			// Wait for data
			if (select((fd + 1), &readFds, NULL, NULL, &timeout) > 0) {
				// Process Bundles
				tosc_bundle bundle;
				tosc_parseBundle(&bundle, buf, BUF_SIZE);
				const uint64_t timetag = tosc_getTimetag(&bundle);
				tosc_message osc;
				while (tosc_getNextMessage(&bundle, &osc))
					tosc_printMessage(&osc);
			} else {
				// Process Messages
				tosc_message osc;
				tosc_parseMessage(&osc, buf, BUF_SIZE);
				tosc_printMessage(&osc);
			}
		}
		close(fd);
	} catch (const std::runtime_error &e) {
		std::cerr << e.what() << std::endl;
		return (EXIT_FAILURE);
	}
	return (EXIT_SUCCESS);
}

/// @brief SIGINT handler
/// @param sig Signal number
static void SIGINT_handler(int sig) {
	running = false;
}
