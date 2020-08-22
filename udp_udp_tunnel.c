#include "shared.c"

int udp_udp_tunnel(int verbose, int obfuscate,
                   struct sockaddr_in localaddr, int localport,
                   struct sockaddr_in remoteaddr, int remoteport)
{
    int res, remotebound = 0;
    int serverfd, remotefd;
    struct pollfd fds[2];
    char buffer[MTU_SIZE];
    struct sockaddr_in clientaddr;
    int clientaddrlen = sizeof(clientaddr), remoteaddrlen = sizeof(remoteaddr);
    
    memset(&clientaddr, 0, sizeof(clientaddr));

    if ((serverfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    { 
        perror("server socket creation failed");
        return EXIT_FAILURE;
    }

    if ((remotefd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    { 
        perror("gateway socket creation failed");
        return EXIT_FAILURE;
    }

    sockets[0] = serverfd;
    sockets[1] = remotefd;

    if (bind(serverfd, (const struct sockaddr *)&localaddr, sizeof(localaddr)) < 0)
    {
        perror("bind failed");
        return EXIT_FAILURE;
    }

    memset(fds, 0 , sizeof(fds));
    fds[0].fd = serverfd;
    fds[0].events = POLLIN;
    fds[1].fd = remotefd;
    fds[1].events = POLLIN;

    if (obfuscate) printf("Header obfuscation enabled.\n");

    while (run)
    {
        if (!remotebound)
        {
            if (verbose) printf("Waiting for first packet from client...\n");

            socklen_t msglen = recvfrom(serverfd, (char*)buffer, MTU_SIZE, MSG_WAITALL, (struct sockaddr*)&clientaddr, (unsigned int*)&clientaddrlen);

            if (msglen == -1)
            {
                if (run)
                {
                    perror("failed to read UDP packet");
                }

                continue;
            }

            char clientaddrstr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(clientaddr.sin_addr), clientaddrstr, INET_ADDRSTRLEN);
            printf("Client connected from %s:%d\n", clientaddrstr, ntohs(clientaddr.sin_port));

            if (verbose) printf("Received %d bytes from client\n", msglen);
            if (obfuscate) obfuscate_message(buffer, msglen);

            res = sendto(remotefd, (char*)buffer, msglen, 0, (const struct sockaddr *)&remoteaddr, remoteaddrlen);

            remotebound = 1;
            continue;
        }

        if (verbose) printf("Polling...\n");

        res = poll(fds, 2, (3 * 60 * 1000));
        
        if (res == 0)
        {
            continue;
        }
        else if (res < 0)
        {
            if (run)
            {
                perror("poll failed");
                return EXIT_FAILURE;
            }
            else
            {
                return EXIT_SUCCESS;
            }
        }

        if (fds[0].revents & POLLIN)
        {
            socklen_t msglen = recvfrom(serverfd, (char*)buffer, MTU_SIZE, MSG_WAITALL, (struct sockaddr*)&clientaddr, (unsigned int*)&clientaddrlen);

            if (msglen == -1)
            {
                if (run)
                {
                    perror("failed to read UDP packet");
                }

                continue;
            }

            if (verbose) printf("Received %d bytes from client\n", msglen);
            if (obfuscate) obfuscate_message(buffer, msglen);

            res = sendto(remotefd, (char*)buffer, msglen, 0, (const struct sockaddr *)&remoteaddr, remoteaddrlen);
        }

        if (fds[1].revents & POLLIN)
        {
            socklen_t msglen = recvfrom(remotefd, (char*)buffer, MTU_SIZE, MSG_WAITALL, (struct sockaddr*)&remoteaddr, (unsigned int*)&remoteaddrlen);

            if (msglen == -1)
            {
                if (run)
                {
                    perror("failed to read UDP packet");
                }

                continue;
            }

            if (verbose) printf("Received %d bytes from remote\n", msglen);
            if (obfuscate) obfuscate_message(buffer, msglen);

            res = sendto(serverfd, (char*)buffer, msglen, 0, (const struct sockaddr *)&clientaddr, clientaddrlen);
        }
    }

    close(serverfd);
    close(remotefd);

    return 0;
}