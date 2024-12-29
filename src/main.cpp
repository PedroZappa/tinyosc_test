/// @file main.cpp
/// @brief OSC Interface main file
/// @Author: Zedro

#include "../inc/ansi.h"
#include "../inc/tinyosc.hpp"
#include <asm-generic/socket.h>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/select.h>
#include <sys/socket.h>

#define VERSION "1.0.0"
#define BUF_SIZE 2048
#define MAX_PORTS 65535
#define PORT 9000

// Volatile allows for external modification
static volatile bool running = true;

static void SIGINT_handler(int sig);

int main(int argc, char **argv) {
	char buf[BUF_SIZE]; // Buffer to read packet data
	int port = PORT;

	std::cout << BGRN "osc_interface " NC << VERSION << std::endl;
	// Process Port parameter
	try {
		if (argc > 1) { // Process Port
			std::cout << "Setting port to: " BYEL << argv[1] << NC << std::endl;
			long porlLong = strtol(argv[1], nullptr, 10);
			char *endptr = nullptr;
			// Check if port is a valid int
			if ((*endptr != '\0') || (argv[1] == endptr)) // if empty
				throw std::runtime_error("Invalid port: not an int");
			// Check if port is within valid range
			if ((porlLong < 0) || (porlLong > MAX_PORTS))
				throw std::runtime_error("Invalid port: out of range");
			// Warn about reserved ranges
			if (porlLong < 1024)
				std::cerr << "Error: Port " << porlLong
						  << " is within a reserved range (0-1023)" << std::endl;
			port = static_cast<int>(porlLong);
		} else if (argc > 2) {
			throw std::runtime_error("Too many arguments");
		} else // Use default port
			std::cout << "Using default port: " << port << std::endl;

		std::cout << BGRN "listening on Port: " << port
				  << std::endl;
	} catch (std::exception &e) { // Handle Error
		std::cerr << e.what() << std::endl;
		return (EXIT_FAILURE);
	}

	// Create Test Message
	// int len = tosc_writeMessage(buf, BUF_SIZE, "/test", "si", "yo whirl!",
	// -42); tosc_printOscBuffer(buf, len);

	// SIGINT handler
	signal(SIGINT, &SIGINT_handler);

	try {
		// UDP Server Setup
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
		// Create Socket address & set values
		struct sockaddr_in socket_in;
		socket_in.sin_family = AF_INET;
		socket_in.sin_port = htons(port); // Bind socket to port
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
			// if (select((fd + 1), &readFds, NULL, NULL, &timeout) > 0) {
			int select_result = select((fd + 1), &readFds, NULL, NULL, &timeout);
			if (select_result > 0) {
				struct sockaddr_in sender_addr;
				socklen_t sender_addr_len = sizeof(sender_addr);
				ssize_t recv_len = recvfrom(fd,
											buf,
											BUF_SIZE,
											0,
											(struct sockaddr *)&sender_addr,
											&sender_addr_len);
				if (recv_len < 0) {
					std::cerr << "Error receiving data!" << std::endl;
					continue; // Skip if there's an error
				}
				// Process Bundles
				tosc_bundle bundle;
				tosc_message osc;

				tosc_parseBundle(&bundle, buf, BUF_SIZE);
				const uint64_t timetag = tosc_getTimetag(&bundle);
				while (tosc_getNextMessage(&bundle, &osc))
					tosc_printMessage(&osc);
			} else if (select_result == 0) {
				// No data available in the timeout period
				continue;
			} else {
				std::cerr << "Error waiting for data!" << std::endl;
				break; // Skip if there's an error
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
