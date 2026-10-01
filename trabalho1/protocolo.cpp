#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <string.h>
#include <array>
#include "protocolo.h"

void definir_timeout(int fd, int ms)
{
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

static const uint32_t *tabela_crc()
{
    static const std::array<uint32_t, 256> tabela = []{
        std::array<uint32_t, 256> t;
        for(uint32_t i = 0; i < 256; i++){
            uint32_t c = i;
            for(int j = 0; j < 8; j++){
                if(c & 1){
                    c = 0xEDB88320u ^ (c >> 1);
                }
                else{
                    c = c >> 1;
                }
            }
            t[i] = c;
        }
        return t;
    }();
    return tabela.data();
}

uint32_t crc_atualizar(uint32_t crc, const char *dados, size_t tam)
{
    const uint32_t *tabela = tabela_crc();

    crc = ~crc;
    for(size_t i = 0; i < tam; i++){
        crc = tabela[(crc ^ (uint8_t)dados[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

uint32_t crc_bloco(const char *pacote, int tam_dados)
{
    uint32_t crc = crc_atualizar(0, pacote, 5);
    return crc_atualizar(crc, pacote + TAM_CAB_DADOS, tam_dados);
}

bool bloco_integro(const char *pacote, int n)
{
    if(n < TAM_CAB_DADOS){
        return false;
    }
    uint32_t crc_rede;
    memcpy(&crc_rede, &pacote[5], 4);
    return ntohl(crc_rede) == crc_bloco(pacote, n - TAM_CAB_DADOS);
}

uint32_t crc_arquivo(std::istream &entrada)
{
    char parte[65536];
    uint32_t crc = 0;

    while(entrada.read(parte, sizeof(parte)) || entrada.gcount() > 0){
        crc = crc_atualizar(crc, parte, entrada.gcount());
    }
    return crc;
}
