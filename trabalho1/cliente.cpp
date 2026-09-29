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
#include <iostream>
#include <fstream>
#include <string>

int main(int argc, char **argv)
{
    char buffer[TAM_BUFFER];
    uint8_t pacote[TAM_BUFFER];

    std::string entrada;
    std::getline(std::cin, entrada);

    pacote[0] = MSG_REQ;

    int tam = 1 + entrada.size();

    if((tam > TAM_BUFFER) || entrada.empty()){
        return 1;
    }
    memcpy(&pacote[1], entrada.c_str(), entrada.size());

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

    if (n < 1){
        std::cout << "Resposta Vazia" << "\n";
    }
    else if(buffer[0] == MSG_ERRO && n >= 2){
        std::string erro(buffer + 2, n - 2);
        std::cout << erro << "\n";
    }
    else if(buffer[0] == MSG_DADOS && n >= TAM_CAB_DADOS){
        uint32_t seq_rede;
        memcpy(&seq_rede, &buffer[1], 4);
        uint32_t seq = ntohl(seq_rede);

        int tam_dados = n - TAM_CAB_DADOS;
        std::cout << "Bloco " << seq << " recebido com " << tam_dados << " bytes\n";

        std::ofstream saida("recebido.bin", std::ios::binary);
        saida.write(&buffer[TAM_CAB_DADOS], tam_dados);
        saida.close();
    }
    else{
        std::cout << "Resposta inesperada" << "\n";
    }

    close(sockfd);
    return 0;
}