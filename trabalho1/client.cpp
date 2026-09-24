#include <stdio.h>
#include <string.h>
#include <strings.h> //pra usar o bzero
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>

#define PORT 5000
#define MAXLINE 1000

int main(int argc, char **argv)
{
    char buffer[100];
    const char *message = "Hello Server";
    int sockfd, n;
    struct sockaddr_in servaddr;

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        printf("\n Error : Connect Failed \n");
        exit(0);
    }

    sendto(sockfd, message, MAXLINE, 0, (struct sockaddr *)NULL, sizeof(servaddr));
    int end = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)NULL, NULL);
    buffer[end] = '\0';
    puts(buffer);
    close(sockfd);
}