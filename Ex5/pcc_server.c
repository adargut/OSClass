#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <stdio.h>

#include <stdlib.h>

#include <unistd.h>

#include <errno.h>

#include <string.h>

#include <sys/types.h>

#include <time.h>

#include <assert.h>

#include <ctype.h>

#include <stdbool.h>

#include <sys/signal.h>

#define BUFF_LEN 32

#define RESP_LEN 256

#define UNUSED(x)(void)(x)

static bool busy = false;
static bool cancelled = false;
static unsigned int total_PCC[95] = {0};

void handle_sigint(int signum, siginfo_t* info, void* ptr)
{
    cancelled = true;
}

int main(int argc, char * argv[]) {
    int nread = -1;
    int listenfd = -1;
    int connfd = -1;

    struct sockaddr_in serv_addr;
    struct sockaddr_in my_addr;
    struct sockaddr_in peer_addr;

    UNUSED(peer_addr);
    UNUSED(my_addr);

    socklen_t addrsize = sizeof(struct sockaddr_in);

    char data_buff[BUFF_LEN];

    uint32_t N_buff[1];
    uint32_t serv_resp[RESP_LEN];

    if (argc != 2) {
        printf("Error : Incorrect args count \n");
        return 1;
    }

    // Define signal for SIGINT
    struct sigaction sigint_action;
    memset(&sigint_action, 0, sizeof(sigint_action));
    sigint_action.sa_sigaction = handle_sigint;
    sigint_action.sa_flags = SA_SIGINFO;
    if(0 != sigaction(SIGINT, &sigint_action, NULL))
    {
        fprintf(stderr, "SIGINT handler registration failed: %s\n", strerror(errno));
        return -1;
    }

    // Socket
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror(strerror(errno));
        exit(1);
    }
    // Set socket options
    memset( & serv_addr, 0, addrsize);
    int yes = 1;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, & yes, sizeof(yes));

    // Set binding config
    serv_addr.sin_family = AF_INET;
    // INADDR_ANY = any local machine address
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    // Bind socket
    if (0 != bind(listenfd,
                  (struct sockaddr * ) & serv_addr,
                  addrsize)) {
        printf("\n Error : Bind Failed. %s \n", strerror(errno));
        return 1;
    }

    // Listen on port
    if (0 != listen(listenfd, 10)) {
        printf("\n Error : Listen Failed. %s \n", strerror(errno));
        return 1;
    }

    while (!cancelled) {
        // Accept a connection.
        connfd = accept(listenfd,
                        (struct sockaddr * ) & peer_addr, & addrsize);
        // Server now handling connection
        busy = true;
        // Do not accept new connections after signal
        if (cancelled) break;
        if (connfd < 0) {
            printf("\n Error : Accept Failed. %s \n", strerror(errno));
            return 1;
        }

        // Read N value from client
        nread = read(connfd,
                     N_buff,
                     sizeof(int));
        uint32_t printable = 0;

        // Read file from client
        int total_read = 0;
        while (total_read < N_buff[0]) {
            nread = read(connfd,
                         data_buff,
                         BUFF_LEN);
            total_read += nread;
            if (nread <= 0)
                break;
            data_buff[nread] = '\0';
            for (int i = 0; i < nread; i++) {
                // Update printable histogram
                if (data_buff[i] <= 126 && data_buff[i] >= 32) {
                    total_PCC[data_buff[i] - 32]++;
                    printable++;
                }
            }
            // Check if we finished reading client request
            if (total_read == N_buff[0]) {
                break;
            }
        }

        // Set response
        serv_resp[0] = printable;
        // Write response to client
        nread = write(connfd,
                      serv_resp,
                      sizeof(serv_resp) - 1);
        // Set to non busy
        busy = false;
        // Close connection
        close(connfd);

        if (cancelled) {
            break;
        }
    }
    // Print histogram before exiting server
    for (int i = 0; i < 95; i++) {
        char c = i + 32;
        printf("char '%c' : %u times\n", c, total_PCC[i]);
    }
    return 0;
}