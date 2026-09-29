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
    char mensagem[TAM_MAX_PACOTE];
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

    if(n > 1 && buffer[0] == MSG_REQ){
        std::string nome_arquivo(buffer + 1, n - 1);

        std::string caminho = std::string(PASTA_ARQUIVOS) + nome_arquivo;

        std::ifstream arquivo(caminho, std::ios::binary);

        int tam;

        if(arquivo.is_open()){
            uint32_t seq = 1;
            uint32_t seq_rede = htonl(seq);

            bzero(mensagem, TAM_CAB_DADOS);
            mensagem[0] = MSG_DADOS;
            memcpy(&mensagem[1], &seq_rede, 4);

            arquivo.read(&mensagem[TAM_CAB_DADOS], TAM_PAYLOAD);
            int lidos = arquivo.gcount();

            tam = TAM_CAB_DADOS + lidos;
            std::cout << "Enviando bloco " << seq << " com " << lidos << " bytes\n";
        }
        else{
            const char *texto = "Arquivo não encontrado";
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