#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#define PORTA 5000
#define PASTA_ARQUIVOS "arquivos/"

#define TAM_PAYLOAD 1024
#define TAM_CAB_DADOS 9
#define TAM_MAX_PACOTE (TAM_CAB_DADOS + TAM_PAYLOAD)
#define TAM_BUFFER 1500

#define TIMEOUT_SERVIDOR_MS 200
#define MAX_TENTATIVAS 5
#define TIMEOUT_REQ_CLIENTE_S 1
#define TIMEOUT_TRANSF_CLIENTE_S 5

enum TipoMensagem
{
    MSG_REQ = 1,
    MSG_INFO = 2,
    MSG_DADOS = 3,
    MSG_ACK = 4,
    MSG_ERRO = 5
};

enum CodigoErro
{
    ERRO_NAO_ENCONTRADO = 1,
    ERRO_NOME_INVALIDO = 2
};

#endif