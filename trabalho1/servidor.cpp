#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <unistd.h>
#include "protocolo.h"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <utility>

enum ResultadoAck
{
    ACK_OK,
    ACK_TIMEOUT,
    ACK_FALHA
};

std::string rotulo(const struct sockaddr_in &c)
{
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &c.sin_addr, ip, sizeof(ip));
    return "[" + std::string(ip) + ":" + std::to_string(ntohs(c.sin_port)) + "] ";
}

ResultadoAck esperar_ack(int fd, const struct sockaddr_in &cli, uint32_t seq_esperado)
{
    char buffer[TAM_BUFFER];

    while(true){
        struct sockaddr_in origem;
        socklen_t len_origem = sizeof(origem);
        int r = recvfrom(fd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&origem, &len_origem);
        if(r < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                return ACK_TIMEOUT;
            }
            perror("recvfrom");
            return ACK_FALHA;
        }
        if(origem.sin_addr.s_addr != cli.sin_addr.s_addr || origem.sin_port != cli.sin_port){
            continue;
        }
        if(r != TAM_ACK || buffer[0] != MSG_ACK){
            continue;
        }
        uint32_t ack_rede;
        memcpy(&ack_rede, &buffer[1], 4);
        if(ntohl(ack_rede) != seq_esperado){
            continue;
        }
        return ACK_OK;
    }
}

bool enviar_e_confirmar(int fd, const struct sockaddr_in &cli, socklen_t len, const char *pacote, int tam, uint32_t seq)
{
    for(int tentativa = 1; tentativa <= MAX_TENTATIVAS; tentativa++){
        if(tentativa > 1){
            std::cout << rotulo(cli) << "Retransmitindo pacote " << seq << " (tentativa " << tentativa << " de " << MAX_TENTATIVAS << ")\n";
        }
        sendto(fd, pacote, tam, 0, (struct sockaddr *)&cli, len);

        ResultadoAck r = esperar_ack(fd, cli, seq);
        if(r == ACK_OK){
            return true;
        }
        if(r == ACK_FALHA){
            return false;
        }
        std::cout << rotulo(cli) << "Timeout aguardando ACK " << seq << "\n";
    }
    return false;
}

void transferir(int fd, const struct sockaddr_in &cli, socklen_t len, std::ifstream &arquivo, uint64_t tamanho)
{
    char mensagem[TAM_MAX_PACOTE];
    uint32_t crc_total = crc_arquivo(arquivo);
    uint32_t crc_rede = htonl(crc_total);
    arquivo.clear();
    arquivo.seekg(0);

    mensagem[0] = MSG_INFO;
    for(int i = 0; i < 8; i++){
        mensagem[1 + i] = (tamanho >> (56 - 8 * i)) & 0xFF;
    }
    memcpy(&mensagem[9], &crc_rede, 4);
    std::cout << rotulo(cli) << "Enviando INFO, arquivo com " << tamanho << " bytes\n";
    printf("%sCRC32 do arquivo: %08x\n", rotulo(cli).c_str(), crc_total);

    if(!enviar_e_confirmar(fd, cli, len, mensagem, TAM_INFO, 0)){
        std::cout << rotulo(cli) << "Sem ACK 0, transferencia abortada\n";
        return;
    }

    uint32_t total = (tamanho + TAM_PAYLOAD - 1) / TAM_PAYLOAD;
    std::cout << rotulo(cli) << "ACK 0 recebido, enviando " << total << " blocos\n";

    for(uint32_t seq = 1; seq <= total; seq++){
        uint32_t seq_rede = htonl(seq);

        bzero(mensagem, TAM_CAB_DADOS);
        mensagem[0] = MSG_DADOS;
        memcpy(&mensagem[1], &seq_rede, 4);

        arquivo.read(&mensagem[TAM_CAB_DADOS], TAM_PAYLOAD);
        int lidos = arquivo.gcount();

        uint32_t crc_bloco_rede = htonl(crc_bloco(mensagem, lidos));
        memcpy(&mensagem[5], &crc_bloco_rede, 4);

        if(!enviar_e_confirmar(fd, cli, len, mensagem, TAM_CAB_DADOS + lidos, seq)){
            if(seq == total){
                std::cout << rotulo(cli) << "Sem confirmacao do ultimo bloco, transferencia encerrada\n";
            }
            else{
                std::cout << rotulo(cli) << "Cliente parou de responder no bloco " << seq << ", transferencia abortada\n";
            }
            return;
        }

        if(seq % 1000 == 0 || seq == total){
            std::cout << rotulo(cli) << "Bloco " << seq << " de " << total << " confirmado\n";
        }
    }

    std::cout << rotulo(cli) << "Transferencia concluida\n";
}

int main(int argc, char **argv)
{
    char buffer[TAM_BUFFER];
    char mensagem[TAM_MAX_PACOTE];
    int listenfd;
    socklen_t len;
    struct sockaddr_in servaddr, cliaddr;
    std::map<pid_t, std::pair<uint32_t, uint16_t>> ativos;

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

    while(true){
        len = sizeof(cliaddr);
        int n = recvfrom(listenfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&cliaddr, &len);
        if (n < 0)
        {
            perror("recvfrom");
            close(listenfd);
            return 1;
        }

        pid_t terminado;
        while((terminado = waitpid(-1, NULL, WNOHANG)) > 0){
            ativos.erase(terminado);
        }

        if(n >= 1 && buffer[0] == MSG_REQ){

            std::pair<uint32_t, uint16_t> chave(cliaddr.sin_addr.s_addr, cliaddr.sin_port);
            bool repetido = false;
            for(const auto &a : ativos){
                if(a.second == chave){
                    repetido = true;
                }
            }
            if(repetido){
                std::cout << rotulo(cliaddr) << "REQ repetido, transferencia em andamento, ignorado\n";
                continue;
            }

            std::string nome_arquivo(buffer + 1, n - 1);

            int tam = 0;
            std::string caminho = std::string(PASTA_ARQUIVOS) + nome_arquivo;

            struct stat info;

            if(nome_arquivo.empty() || nome_arquivo.size() > 255 || nome_arquivo.find('\0') != std::string::npos || nome_arquivo.find_first_of("/\\")  != std::string::npos|| nome_arquivo.find("..") != std::string::npos){
                const char *texto = "Nome de arquivo recebido inválido";
                std::cout << rotulo(cliaddr) << texto << "\n";
                mensagem[0] = MSG_ERRO;
                mensagem[1] = ERRO_NOME_INVALIDO;
                memcpy(&mensagem[2], texto, strlen(texto));
                tam = 2 + strlen(texto);
            }

            else if(stat(caminho.c_str(), &info) == 0 && S_ISREG(info.st_mode)){
                std::ifstream arquivo(caminho, std::ios::binary);
                if(arquivo.is_open()){
                    fflush(stdout);
                    pid_t pid = fork();
                    if(pid < 0){
                        perror("fork");
                    }
                    else if(pid == 0){
                        close(listenfd);
                        int fd = socket(AF_INET, SOCK_DGRAM, 0);
                        if(fd < 0){
                            perror("socket");
                            exit(1);
                        }
                        definir_timeout(fd, TIMEOUT_SERVIDOR_MS);
                        transferir(fd, cliaddr, len, arquivo, info.st_size);
                        close(fd);
                        exit(0);
                    }
                    else{
                        ativos[pid] = chave;
                        std::cout << rotulo(cliaddr) << "Transferencia de " << nome_arquivo << " iniciada\n";
                    }
                }
                else{
                    const char *texto = "Arquivo nao encontrado";
                    std::cout << rotulo(cliaddr) << texto << "\n";
                    mensagem[0] = MSG_ERRO;
                    mensagem[1] = ERRO_NAO_ENCONTRADO;
                    memcpy(&mensagem[2], texto, strlen(texto));
                    tam = 2 + strlen(texto);
                }
                arquivo.close();
            }
            else{
                const char *texto = "Arquivo nao encontrado";
                std::cout << rotulo(cliaddr) << texto << "\n";
                mensagem[0] = MSG_ERRO;
                mensagem[1] = ERRO_NAO_ENCONTRADO;
                memcpy(&mensagem[2], texto, strlen(texto));
                tam = 2 + strlen(texto);
            }

            if(tam > 0){
                sendto(listenfd, mensagem, tam, 0, (struct sockaddr *)&cliaddr, len);
            }
        }
    }
    close(listenfd);
    return 0;
}
