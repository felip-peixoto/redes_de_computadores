#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "protocolo.h"
#include <cstdint> 
#include <string_view>

int main(int argc, char **argv)
{
    char buffer[TAM_BUFFER];
    uint8_t pacote[TAM_BUFFER];
    const char *nome = "teste.txt";

    pacote[0] = MSG_REQ;
    memcpy(&pacote[1], nome, strlen(nome));
    int tam = 1 + strlen(nome);


    int sockfd;
    struct sockaddr_in servaddr;

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORTA);
    servaddr.sin_family = AF_INET;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return 1;
    }

    sendto(sockfd, pacote, tam, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));

    int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, NULL, NULL);
    if (n < 0)
    {
        perror("recvfrom");
        close(sockfd);
        return 1;
    }
    buffer[n] = '\0';
    puts(buffer);

    close(sockfd);
    return 0;
}