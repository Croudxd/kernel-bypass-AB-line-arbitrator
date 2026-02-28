#pragma once
#include <bits/stdc++.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "../include/binary_struct.hpp"

#define PORT     8080
#define MAXLINE  1024

class udp
{
    private:
        int sockfd;
        struct sockaddr_in servaddr;

    public:
        udp() = default;
        void connect ()
        {
            sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            if (sockfd < 0) 
            {
                perror("socket creation failed");
                exit(EXIT_FAILURE);
            }

            memset(&servaddr, 0, sizeof(servaddr));

            servaddr.sin_family = AF_INET;              
            servaddr.sin_port   = htons(PORT);         
            servaddr.sin_addr.s_addr = inet_addr("224.0.0.1");

        }
        void send(binary_struct* bs) 
        {
            socklen_t len = sizeof(servaddr);
            sendto(sockfd, bs, sizeof(binary_struct), 0, (const struct sockaddr *)&servaddr, sizeof(servaddr));
        }

        void udpclose()
        {
            close(sockfd);
        }
};
