SO-SSEGADO: Simulador de Sistema Operacional com Gerenciamento de Disco
:fountain_pen: Sobre
SO-SSEGADO é um simulador de sistema operacional que se concentra no gerenciamento de recursos de hardware entre diferentes processos, com um foco especial no gerenciamento de disco usando o algoritmo C-SCAN. O simulador gerencia os seguintes recursos de hardware: CPU, Memória, E/S e Disco.
🎓 Um projeto de universidade
Este projeto de simulador de sistema operacional, SO-SSEGADO, foi desenvolvido como requisito da disciplina de Sistemas Operacionais por:
Leandro Aguiar Mota
Otávio Augusto Teixeira
Mateus Rosolem Baroni

:book: Introdução
SO-SSEGADO oferece um terminal interativo onde os usuários podem executar processos e visualizar sua execução em diferentes recursos de hardware por meio de visualizações.
:question: O que são visualizações?
Visualizações são janelas no terminal do simulador que revelam o funcionamento interno do sistema operacional ao gerenciar um recurso de hardware específico. Cada visualização é dedicada a um recurso de hardware, como o disco ou os semáforos.
:question: Como posso executar meu próprio processo?
Nas próximas seções, explicaremos como escrever um arquivo de programa sintético :spiral_notepad: que define as instruções do processo, o tamanho do segmento, os semáforos utilizados e outras informações relevantes.
:hammer_and_wrench: Compilação
Para começar, siga estas etapas para compilar o simulador:
1. Instalação do gcc
Instale o compilador C, gcc, executando o seguinte comando:
<p align="center"><i>sudo apt install gcc</i></p>
:bell: Nota: Se o gcc já estiver instalado, prossiga para a próxima etapa.
2. Instalação do make
Instale o make para facilitar a compilação do simulador, permitindo a compilação de todo o projeto com um único comando:
<p align="center"><i>sudo apt install make</i></p>
:bell: Nota: Se o make já estiver instalado, prossiga para a próxima etapa.
3. Instalação do ncurses
Instale o ncurses, a biblioteca utilizada pelo simulador para criar as interfaces do terminal:
<p align="center"><i>sudo apt install libncurses-dev</i></p>
:bell: Nota: Se o ncurses já estiver instalado, você está pronto para prosseguir para a próxima seção.
:rocket: Execução
Após seguir as etapas de instalação, execute o simulador de sistema operacional com o seguinte comando (no mesmo diretório do arquivo Makefile):
<p align="center"><i>make run</i></p>
Explore o simulador e experimente alguns dos programas sintéticos fornecidos.
:mag_right: Opções do Menu do Terminal
Ao executar o simulador pela primeira vez, familiarize-se com as opções do menu do terminal:
1. Criar Processo
Permite criar e executar as instruções de um processo. Uma janela será aberta solicitando o caminho para o arquivo do programa sintético.
2. Informações do Processo
Exibe informações detalhadas sobre um processo específico, como nome, ID do segmento, prioridade, tamanho do segmento, semáforos utilizados, instrução atual, status e tempo restante.
3. Informações do Semáforo
Lista todos os semáforos existentes e seus respectivos contadores de referência.
4. Informações da Memória
Mostra a utilização total da memória, incluindo a quantidade de memória utilizada e a porcentagem de utilização.
5. Informações do Disco
Exibe informações sobre o disco, como o bloco atual do cabeçote, o último bloco acessado, a direção do cabeçote e a fila de disco.
0. Sair
Encerra o simulador de sistema operacional.
:clapper: Como posso executar meu próprio processo?
Os processos no simulador são executados a partir de um arquivo de programa sintético com um formato específico.
:scroll: Arquivo de Programa Sintético
O formato do arquivo é o seguinte:
nome do programa
id do segmento
prioridade inicial do processo
tamanho do segmento (em KB)
lista de semáforos (separados por espaço)
<linha em branco>
sequência de instruções (uma por linha)
Use code with caution.
Descrição dos metadados:
nome do programa: Nome do programa (não precisa ser único).
id do segmento: ID único do segmento de memória alocado ao processo.
prioridade inicial do processo: Define a fila do escalonador onde o processo iniciará (0: prioridade mais alta, 1: prioridade mais baixa).
tamanho do segmento: Tamanho do segmento de memória alocado ao processo em kilobytes.
lista de semáforos: Lista de semáforos que o processo pode usar durante a simulação.
Exemplo de arquivo de programa sintético:
  
processo1
50
0
24
s

exec 20
exec 80
P(s)
read 110
print 50
exec 40
write 150
V(s)
exec 30


:bell: Nota 1: O exemplo acima define um processo chamado processo1 com ID de segmento 50, prioridade inicial 0 (prioridade mais alta), tamanho de segmento 24 KB e utiliza o semáforo s.
:bell: Nota 2: O arquivo de programa sintético não requer nenhuma extensão de arquivo ou nome específico.
:question: Quais são as instruções disponíveis?
:desktop_computer: Instruções Disponíveis
As instruções a seguir podem ser incluídas na seção de sequência de instruções do arquivo de programa sintético:
exec k: Executa por k unidades de tempo.
read k: Lê do disco na trilha k.
write k: Escreve no disco na trilha k.
print k: Imprime por k unidades de tempo.
P(s): Acessa a região crítica protegida pelo semáforo s.
V(s): Libera a região crítica protegida pelo semáforo s.
Agora é só usar a criatividade!

Gerenciamento de Disco C-SCAN

O simulador SO-SSEGADO implementa o algoritmo de escalonamento de disco C-SCAN (Circular SCAN) para gerenciar as solicitações de leitura e escrita no disco. O C-SCAN visa reduzir o tempo médio de busca em relação ao algoritmo SCAN, movendo o cabeçote de leitura/gravação do disco em uma única direção até atingir o último cilindro, para então retornar ao início da superfície do disco e continuar atendendo às solicitações na mesma direção.
Considerações Finais

O simulador SO-SSEGADO oferece uma plataforma completa para explorar os conceitos de gerenciamento de recursos de hardware em sistemas operacionais, com foco no algoritmo C-SCAN para o gerenciamento de disco. As diferentes visualizações e a possibilidade de criar e executar programas sintéticos permitem aos usuários entender o funcionamento interno de um sistema operacional e os desafios de gerenciar recursos compartilhados de forma eficiente.
