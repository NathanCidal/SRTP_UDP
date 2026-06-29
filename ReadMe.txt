================================================================================
          README - SIMPLE RELIABLE TRANSPORT PROTOCOL (SRTP) SOBRE UDP
================================================================================

Este projeto implementa o protocolo SRTP (Simple Reliable Transport Protocol),
uma camada de transporte confiavel desenvolvida sobre o protocolo UDP. A 
aplicacao possui suporte a tres modos de controle de fluxo e erros:
Stop-and-Wait (SAW), Go-Back-N (GBN) e Selective Repeat (SR).

A arquitetura baseia-se em um modelo Cliente/Servidor, referenciados no codigo 
como Host (Emissor/Cliente) e Listen (Receptor/Servidor).

--------------------------------------------------------------------------------
1. INSTRUCOES DE COMPILACAO E DEPENDENCIAS
--------------------------------------------------------------------------------

A aplicacao foi desenvolvida em C padrao para sistemas Linux/Unix e utiliza a 
biblioteca zlib para o calculo do checksum CRC32. Certifique-se de possui-la 
instalada (no Ubuntu/Debian: `sudo apt install zlib1g-dev`).

A compilação pode ser feita diretamente pelo Makefile fornecido:

* Compilar o projeto:
  make build

* Limpar executaveis e arquivos temporarios:
  make clean

Nota: Por padrao, a flag -DDEBUG esta ativa no Makefile, exibindo logs detalhados 
no console sobre o fluxo de pacotes, buffers e disparos de ACKs/NACKs.

--------------------------------------------------------------------------------
2. ARGUMENTOS DE LINHA DE COMANDO
--------------------------------------------------------------------------------

O executavel aceita os seguintes parametros gerenciados pelo modulo Parser:

Modos de Execucao (Obrigatorio escolher um):
  --listen          Configura a aplicacao como Receptor (Servidor). Espera conexoes.
  --host <IP>       Configura a aplicacao como Emissor (Cliente). Requer o IP de destino.

Parametros de Conexao e Fluxo:
  --port <numero>   Define a porta base P de comunicacao (Ex: 6000). O Host fixa
                    automaticamente sua origem e escuta em P+1.
  --file <caminho>  Especifica o caminho do arquivo de entrada a ser transmitido
                    (Obrigatorio apenas no modo --host).
  --size <numero>   Define o tamanho maximo da Janela Deslizante desejada para 
                    os modos GBN ou SR. Se omitido, assume 1.

Selecao do Protocolo (Opcional - SAW por padrao se omitido):
  --mode saw        Forca a execucao em Stop-and-Wait.
  --mode gbn        Forca a execucao em Go-Back-N (Janela Deslizante).
  --mode sr         Forca a execucao em Selective Repeat (Janela Deslizante).

--------------------------------------------------------------------------------
3. INSTRUCOES DE EXECUCAOO (PASSO A PASSO COM MAKEFILE)
--------------------------------------------------------------------------------

Sempre inicie primeiro a aplicacao em modo Receptor para que ela fique aguardando 
o Handshake inicial. O receptor salvara o arquivo de saida como "output_file.txt".

Para testar os modos alterando os parametros diretamente pelo terminal:

* Modo Stop-and-Wait (SAW):
  Terminal Server: make server MODE=saw SIZE=1
  Terminal Client: make client MODE=saw SIZE=1

* Modo Go-Back-N (GBN):
  Terminal Server: make server MODE=gbn SIZE=4
  Terminal Client: make client MODE=gbn SIZE=4

* Modo Selective Repeat (SR):
  Terminal Server: make server MODE=sr SIZE=4
  Terminal Client: make client MODE=sr SIZE=4

--------------------------------------------------------------------------------
4. EMULACAO DE CENARIOS DE REDE (LINUX TC / NETEM)
--------------------------------------------------------------------------------

Para simular condicoes adversas de rede localmente na interface loopback (lo), 
execute os comandos abaixo no terminal utilizando privilegios de root (sudo) 
antes de rodar o Client/Server.

* ATENCAO: Sempre limpe as regras anteriores antes de aplicar uma nova:
  sudo tc qdisc del dev lo root

* Cenario de Latencia Elevada (Adiciona 75ms de atraso, gerando ~150ms de RTT):
  sudo tc qdisc add dev lo root netem delay 75ms

* Cenario de Perda Elevada (Descarta aleatoriamente 25% dos pacotes):
  sudo tc qdisc add dev lo root netem loss 25%

* Cenario de Reordenacao de Pacotes (Atrasa pacotes em 10ms, enviando 25% imediatamente):
  sudo tc qdisc add dev lo root netem delay 10ms reorder 25% 50%

* Resetar a interface de rede para o comportamento padrao:
  sudo tc qdisc del dev lo root

--------------------------------------------------------------------------------
5. NOTAS DE INSTRUMENTACAO E METRICAS DO RELATORIO
--------------------------------------------------------------------------------
Ao final de cada transmissao, o receptor exibe um relatorio detalhado em tela contendo:
- Tempo total da recepcao (segundos)
- Contadores de pacotes validos, duplicados e fora de ordem (essencial para o relatorio)
- Volume util de payload gravado em disco (Bytes)
- Volume bruto recebido pela pilha UDP (Bytes)
- Throughput efetivo de escrita util (KB/s)

Para analise de trafego fina, recomenda-se monitorar a interface "loopback" no Wireshark 
utilizando o filtro de exibicao: udp.port == 6000 || udp.port == 6001.
================================================================================