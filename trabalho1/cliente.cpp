#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
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
#include <set>
#include <sstream>

void enviar_ack(int fd, const struct sockaddr_in &destino, socklen_t len, uint32_t seq)
{
    uint8_t ack[TAM_ACK];
    uint32_t seq_rede = htonl(seq);

    ack[0] = MSG_ACK;
    memcpy(&ack[1], &seq_rede, 4);
    sendto(fd, ack, TAM_ACK, 0, (struct sockaddr *)&destino, len);
}

int main(int argc, char **argv)
{
    char buffer[TAM_BUFFER];
    uint8_t pacote[TAM_BUFFER];
    int codigo = 0;

    std::string entrada;
    std::cout << "Endereco (@IP:Porta/arquivo): ";
    std::getline(std::cin, entrada);

    std::string lista;
    std::cout << "Blocos a descartar: ";
    std::getline(std::cin, lista);

    std::set<uint32_t> descartar;
    std::stringstream partes(lista);
    std::string parte;
    while(std::getline(partes, parte, ',')){
        int bloco;
        try{
            bloco = std::stoi(parte);
        }
        catch(...){
            std::cout << "Lista de blocos inválida" << "\n";
            return 1;
        }
        if(bloco < 1){
            std::cout << "Lista de blocos inválida" << "\n";
            return 1;
        }
        descartar.insert(bloco);
    }
    if(!descartar.empty()){
        std::cout << "Blocos que serao descartados:";
        for(uint32_t bloco : descartar){
            std::cout << " " << bloco;
        }
        std::cout << "\n";
    }

    size_t p_arroba = entrada.find("@");
    if (p_arroba == std::string::npos){
        std::cout << "Entrada inválida" << "\n";
        return 1;
    }
    size_t p_dois_pontos = entrada.find(":");
    if (p_dois_pontos == std::string::npos){
        std::cout << "Entrada inválida" << "\n";
        return 1;
    }
    size_t p_barra = entrada.find("/");
    if (p_barra == std::string::npos){
        std::cout << "Entrada inválida" << "\n";
        return 1;
    }
    if(p_arroba > p_dois_pontos || p_dois_pontos > p_barra){
        std::cout << "Entrada inválida" << "\n";
        return 1;
    }
    std::string ip = entrada.substr(p_arroba + 1, p_dois_pontos - p_arroba - 1);

    int porta_entrada;
    try{
        porta_entrada = std::stoi(entrada.substr(p_dois_pontos + 1, p_barra - p_dois_pontos - 1));
    }
    catch(...){
        std::cout << "Porta inválida" << "\n";
        return 1;
    }
    if(porta_entrada < 1024 || porta_entrada > 65535){
        std::cout << "Porta inválida" << "\n";
        return 1;
    }

    std::string arquivo =  entrada.substr(p_barra + 1);
    std::string destino = "recebido_" + arquivo;
    pacote[0] = MSG_REQ;

    int tam = 1 + arquivo.size();

    if(tam > TAM_BUFFER){
        std::cout << "Nome de arquivo muito longo" << "\n";
        return 1;
    }
    memcpy(&pacote[1], arquivo.c_str(), arquivo.size());

    int sockfd;
    struct sockaddr_in servaddr;

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_port = htons(porta_entrada);
    if (inet_pton(AF_INET, ip.c_str(), &servaddr.sin_addr) != 1) {
        std::cout << "IP inválido" << "\n";
        return 1;
    }

    servaddr.sin_family = AF_INET;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return 1;
    }

    definir_timeout(sockfd, TIMEOUT_REQ_CLIENTE_S * 1000);

    struct sockaddr_in origem;
    socklen_t len_origem = sizeof(origem);
    int n = -1;

    for(int tentativa = 1; tentativa <= MAX_TENTATIVAS; tentativa++){
        sendto(sockfd, pacote, tam, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));

        len_origem = sizeof(origem);
        n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&origem, &len_origem);
        if(n >= 0){
            break;
        }
        if(errno != EAGAIN && errno != EWOULDBLOCK){
            perror("recvfrom");
            close(sockfd);
            return 1;
        }
        std::cout << "Sem resposta do servidor (tentativa " << tentativa << " de " << MAX_TENTATIVAS << ")\n";
    }

    if (n < 0)
    {
        std::cout << "Servidor nao respondeu\n";
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
    else if(buffer[0] == MSG_INFO && n == TAM_INFO){
        uint64_t tamanho = 0;
        for(int i = 0; i < 8; i++){
            tamanho = (tamanho << 8) | (uint8_t)buffer[1 + i];
        }
        uint32_t total = (tamanho + TAM_PAYLOAD - 1) / TAM_PAYLOAD;
        uint32_t crc_info_rede;
        memcpy(&crc_info_rede, &buffer[9], 4);
        uint32_t crc_info = ntohl(crc_info_rede);
        std::cout << "INFO recebido, arquivo com " << tamanho << " bytes em " << total << " blocos\n";

        enviar_ack(sockfd, origem, len_origem, 0);

        definir_timeout(sockfd, TIMEOUT_TRANSF_CLIENTE_S * 1000);

        std::ofstream saida(destino, std::ios::binary | std::ios::trunc);
        uint32_t esperado = 1;

        while(esperado <= total){
            struct sockaddr_in remetente;
            socklen_t len_remetente = sizeof(remetente);
            n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&remetente, &len_remetente);
            if (n < 0)
            {
                if(errno == EAGAIN || errno == EWOULDBLOCK){
                    std::cout << "Servidor parou de responder, transferencia abortada\n";
                }
                else{
                    perror("recvfrom");
                }
                saida.close();
                close(sockfd);
                return 1;
            }

            if(remetente.sin_addr.s_addr != origem.sin_addr.s_addr || remetente.sin_port != origem.sin_port){
                continue;
            }
            if(n == TAM_INFO && buffer[0] == MSG_INFO){
                enviar_ack(sockfd, origem, len_origem, 0);
                continue;
            }
            if(n < TAM_CAB_DADOS || buffer[0] != MSG_DADOS){
                continue;
            }
            if(!bloco_integro(buffer, n)){
                std::cout << "Bloco corrompido (CRC invalido), descartado sem ACK\n";
                continue;
            }

            uint32_t seq_rede;
            memcpy(&seq_rede, &buffer[1], 4);
            uint32_t seq = ntohl(seq_rede);

            if(seq > esperado){
                continue;
            }
            if(seq < esperado){
                enviar_ack(sockfd, origem, len_origem, seq);
                continue;
            }

            if(descartar.erase(seq) > 0){
                std::cout << "Descartando bloco " << seq << " (simulacao de perda)\n";
                continue;
            }

            saida.seekp((std::streamoff)(seq - 1) * TAM_PAYLOAD);
            saida.write(&buffer[TAM_CAB_DADOS], n - TAM_CAB_DADOS);

            enviar_ack(sockfd, origem, len_origem, seq);

            if(seq % 1000 == 0 || seq == total){
                std::cout << "Bloco " << seq << " de " << total << " recebido\n";
            }
            esperado++;
        }
        saida.close();
        std::cout << "Arquivo salvo em " << destino << "\n";

        std::ifstream recebido(destino, std::ios::binary);
        uint32_t crc_final = crc_arquivo(recebido);
        recebido.close();
        if(crc_final == crc_info){
            printf("Integridade OK: CRC32 %08x confere com o do servidor\n", crc_final);
        }
        else{
            printf("ERRO: CRC32 do arquivo (%08x) difere do informado pelo servidor (%08x)\n", crc_final, crc_info);
            codigo = 1;
        }

        definir_timeout(sockfd, TIMEOUT_FIM_CLIENTE_S * 1000);

        while(true){
            struct sockaddr_in remetente;
            socklen_t len_remetente = sizeof(remetente);
            n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&remetente, &len_remetente);
            if(n < 0){
                break;
            }
            if(remetente.sin_addr.s_addr != origem.sin_addr.s_addr || remetente.sin_port != origem.sin_port){
                continue;
            }
            if(n >= TAM_CAB_DADOS && buffer[0] == MSG_DADOS && bloco_integro(buffer, n)){
                uint32_t seq_rede;
                memcpy(&seq_rede, &buffer[1], 4);
                if(ntohl(seq_rede) == total){
                    enviar_ack(sockfd, origem, len_origem, total);
                }
            }
        }
    }
    else{
        std::cout << "Resposta inesperada" << "\n";
    }

    close(sockfd);
    return codigo;
}
