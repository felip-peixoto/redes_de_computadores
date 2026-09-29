#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "protocolo.h"
#include <iostream>
#include <fstream>

int main(int argc, char **argv)
{
    char buffer[TAM_BUFFER];
    char mensagem[TAM_BUFFER];
    int listenfd;
    socklen_t len;
    struct sockaddr_in servaddr, cliaddr;

    bzero(&servaddr, sizeof(servaddr));
    bzero(&cliaddr, sizeof(cliaddr));

    listenfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listenfd < 0)
    {
        perror("socket");
        return 1;
    }

    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORTA);
    servaddr.sin_family = AF_INET;

    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("bind");
        close(listenfd);
        return 1;
    }

    len = sizeof(cliaddr);
    int n = recvfrom(listenfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&cliaddr, &len);
    if (n < 0)
    {
        perror("recvfrom");
        close(listenfd);
        return 1;
    }

    if(buffer[0] == MSG_REQ){
        std::string nome_arquivo(buffer + 1, n - 1);

        std::string caminho = std::string(PASTA_ARQUIVOS) + nome_arquivo;

        std::ifstream arquivo(caminho);

        const char *texto;
        int tam; 

        if(arquivo.is_open()){
            texto = "Arquivo existe"; 
            std::cout << texto << "\n";
            mensagem[0] = MSG_INFO;
            memcpy(&mensagem[1], texto, strlen(texto));
            tam = 1 + strlen(texto);
        }
        else{
            texto = "Arquivo não encontrado";
            std::cout << texto << "\n";
            mensagem[0] = MSG_ERRO;
            mensagem[1] = ERRO_NAO_ENCONTRADO;
            memcpy(&mensagem[2], texto, strlen(texto));
            tam = 2 + strlen(texto);
        }
        arquivo.close();
        sendto(listenfd, mensagem, tam, 0, (struct sockaddr *)&cliaddr, len);

    }
    else{
        return 0;
    }

    

    close(listenfd);
    return 0;
}