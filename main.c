/*
TRABALHO DE SISTEMA OPERACIONAIS
GRUPO:
  - Leandro Aguiar Mota
  - Otávio Augusto Teixeira
  - Mateus Rosolem Baroni
*/

// Escolhemos deixar varias execucções unitárias de cada EXEC para ficar vísivel
// para o usuario

#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Defines
// 1GB memoria Total
#define MAX_MEM_SIZE 1073741824
// Deve ser simulada uma memória com alocação em blocos, com páginas de 8kB
#define PAGE_SIZE 8192
// kbytes;
#define NUMBER_OF_FRAMES (MAX_MEM_SIZE / PAGE_SIZE)

#define QUANTUM 5000
#define FILE_EXT ".prog"
// Muda de SO para SO
#define CLEAR_SCREEN system("clear");

// Tipos de dados:
//------------------------------------------------------------------------------
typedef enum state { READY, RUNNING, BLOCKED, INACTIVE } p_state;

/*
Ready->Running
Running->Blocked
Running->Inativo
Blocked->Ready
Inativo-> Ready
*/

typedef struct memoryPage {
  long *associated_pTable_entry; // Entrada da tabela de páginas associada(PAM)
  char reference_bit;            // Bit de referência(para o second chance)
} memPage_t;

typedef struct memoryFrameTable {
  memPage_t *frame[NUMBER_OF_FRAMES]; // Vetor que associa cada frame na memória
                                      // à página armazenada nele
} frameTable_t;

typedef struct memoryPageTable {
  long *address; // Endereço: address[virt] == phys; -1 significa que está no
                 // disco
} pageTable_t;

typedef struct cmd {
  int arg;    // Argumento do comando(exec,print,...)
  char *call; // Chamada do comando(tempo,...)
} command_t;

typedef struct proc {
  char name[17];         // Nome do processo (limite de 16 bytes)
  int SID;               // Segment ID
  int priority;          // Prioridade
  int seg_size;          // Tamanho do segmento (em bytes)
  char *used_semaphores; // String de semáforos usados, separados por espaço
  pageTable_t *pTable;   // Lista de Tabela de páginas
  command_t **code; // Lista de comandos que compõem o código do programa(Lista
                    // de strings->Matriz)
  int nCommands; // Número de comandos do programa
} process_t;

typedef struct bcp_item {
  process_t *proc;      // Processo
  int next_instruction; // Próxima instrução a ser executada
  p_state status; // Status: r: pronto, R: executando, b: bloqueado, i: inativo
  long remaining_time;   // Tempo restante de cada processo
  int quantum;           // Quantum(5000 a 0)
  int quantum_counter;   // Contador do quantum(0 a 5000)
  int PID;               // ID do processo
  struct bcp_item *next; // Próximo processo na lista
} BCPitem_t;

typedef struct bcp { // Lista dos Processos
  BCPitem_t *head;   // Cabeça da lista de BCPs
  BCPitem_t *tail;   // Cauda da lista de BCPs
} BCP_t;

typedef struct ioop { // Operador de I/O
  BCPitem_t *process; // Ponteiro para o processo bloqueado
  char type; // Tipo de operação: r: leitura, w: escrita, p: impressão
  int remaining_time; // Tempo restante
  struct ioop *next;  // Próxima operação de I/O na fila
} IOop_t;

typedef struct ioqueue { // Fila de IO
  IOop_t *head;          // Cabeça da fila de operações de I/O
} IOqueue_t;

typedef struct semaphor {
  volatile int
      v; // Valor do semáforo (com volatile para nao otimizar e nao dar erro)
  char name;    // Nome do semáforo
  int refcount; // Contador de referências (número de processos que usam o
                // semáforo)
  struct sem_li *waiting_list; // Lista de processos esperando no semáforo
  struct semaphor *next;       // Próximo semáforo na lista
} semaphore_t;

typedef struct sem_li { // Lista de espera dos processos no Semáforo
  BCPitem_t *proc;      // Processo esperando no semáforo
  struct sem_li *next;  // Próximo processo na lista de espera
} sem_list_item_t;

typedef struct all_sem_li { // Lista de todos os semáforos
  semaphore_t *head; // Cabeça da lista de todos os semáforos existentes
} all_sem_list_t;

// Estrutura do disco
typedef struct diskBlock {
  int id;                 // ID do bloco no disco
  char *data;             // Dados armazenados no bloco
  struct diskBlock *next; // Próximo bloco no disco
} diskBlock_t;

typedef struct disk {
  diskBlock_t *head; // Cabeça da lista de blocos do disco
  diskBlock_t *tail; // Ponteiro para o último bloco do disco
} disk_t;

// Estrutura de um processo em espera na fila de disco
typedef struct diskQueueItem {
  process_t *process; // Processo que está esperando
  int block_id;       // ID do bloco que o processo está acessando
  char type;          // Tipo de operação de disco (r: leitura, w: escrita)
  struct diskQueueItem *next; // Próximo processo na fila
} diskQueueItem_t;

typedef struct diskQueue {
  diskQueueItem_t *head; // Cabeça da fila de processos esperando por disco
  diskQueueItem_t *tail; // Cauda da fila de processos esperando por disco
} diskQueue_t;

// Estrutura para armazenar o estado do disco
typedef struct diskState {
  diskBlock_t *current_block;       // Bloco atual
  diskBlock_t *last_accessed_block; // Último bloco acessado
  diskQueue_t *queue; // Fila de processos esperando por acesso ao disco
  int direction;      // Direção do movimento do cabeçote (1: crescente, -1:
                      // decrescente)
} diskState_t;

// Protótipos:
//------------------------------------------------------------------------------
void semaphore_init(semaphore_t *semaph, volatile int v);
void proc_wakeup(BCPitem_t *proc);
void processFinish(BCPitem_t *proc);
int calculate_quantum(int prio);
void showMenu();
void init_data_structures();
void queueProcess(BCPitem_t *proc);
void dequeueProcess(BCPitem_t *item);
void proc_sleep(BCPitem_t *proc);
void insert_semaphore(semaphore_t *item);
void createSemaphore(char name);
void sem_queue(sem_list_item_t **list, BCPitem_t *proc);
void semaphoreP(semaphore_t *semaph, BCPitem_t *proc);
void semaphoreV(semaphore_t *semaph);
int inFrameTable(int pos, process_t *proc);
void memLoadReq(process_t *proc);
void io_queue_add(BCPitem_t *item, char type);
void processInterrupt();
void advanceIOqueue();
semaphore_t *retrieveSemaphore(char name);
void removeSemaphore(semaphore_t *item);
void Free(BCPitem_t *a);
void *mainLoop();
void interpreter(BCPitem_t *curr);
void *menu();
void showSemaphoreList();
void showMemoryInfo();
void viewProcessInfo();
char *getStatus(p_state st);
long calculateRemainingTime(process_t *proc);
void printProcessInfo(process_t *proc);
void processCreate(char *filename);
process_t *readProgramfromDisk(char *filename);
command_t **parsecommands(char *code, int *inst_counter);
int validateFilename(char *filename);
void disk_init(disk_t *disk, int numBlocks, int blockSize);
void disk_write(disk_t *disk, int blockNumber, void *data, int size);
void disk_read(disk_t *disk, int blockNumber, void *data, int size);
int disk_allocate_block(disk_t *disk);
//void disk_free_block(disk_t *disk, int blockNumber);
//int disk_is_block_free(disk_t *disk, int blockNumber);

// Variáveis globais:
//------------------------------------------------------------------------------
long available_memory = MAX_MEM_SIZE; // Memória disponível
frameTable_t frameTable;              // Tabela de frames
BCP_t BCP;                            // Lista de BCPs
IOqueue_t IOqueue;                    // Fila de operações de I/O
BCPitem_t *curr_running = NULL;       // Processo atualmente em execução
BCPitem_t *prev_running = NULL;       // Processo anteriormente em execução
volatile int PID = 0;                 // ID do processo
volatile int stop = 0;                // Flag de parada
sem_t mutex;                          // Semáforo
all_sem_list_t existing_semaphores;   // Lista de todos os semáforos existentes
disk_t disk = {NULL, NULL};           // Disco
diskQueue_t queue = {NULL, NULL};
diskState_t state = {NULL, NULL, &queue, 1}; // Estado do disco
sem_t disk_semaphore;
sem_t io_semaphore;
sem_t memory_semaphore;
pthread_mutex_t mutex_lock; // Mutex do semáforo

// Função para criar uma nova fila de disco
diskQueue_t *createDiskQueue() {
  diskQueue_t *queue = (diskQueue_t *)malloc(sizeof(diskQueue_t));
  if (queue == NULL) {
    perror("Erro ao alocar memória para a fila de disco");
    exit(1);
  }
  queue->head = NULL;
  queue->tail = NULL;

  return queue;
}

void enqueue_disk_queue(diskQueue_t *queue, process_t *process, int block_id,
                        char type) {
  diskQueueItem_t *new_item =
      (diskQueueItem_t *)malloc(sizeof(diskQueueItem_t));
  new_item->process = process;
  new_item->block_id = block_id;
  new_item->type = type;
  new_item->next = NULL;

  if (queue->head == NULL) {
    queue->head = new_item;
    queue->tail = new_item;
  } else {
    queue->tail->next = new_item;
    queue->tail = new_item;
  }
}

// Função para remover e retornar o primeiro item da fila de espera do disco
diskQueueItem_t *dequeue_disk_queue(diskQueue_t *queue) {
  if (queue->head == NULL) {
    return NULL;
  }

  diskQueueItem_t *item = queue->head;
  queue->head = queue->head->next;

  if (queue->head == NULL) {
    queue->tail = NULL;
  }

  return item;
}

// Função para ordenar os pedidos na fila
void sort_queue(diskQueueItem_t **queue) {
  if (*queue == NULL) {
    return;
  }

  diskQueueItem_t *sorted = NULL;
  diskQueueItem_t *current = *queue;
  diskQueueItem_t *next;

  while (current != NULL) {
    next = current->next;

    if (sorted == NULL || sorted->block_id >= current->block_id) {
      current->next = sorted;
      sorted = current;
    } else {
      diskQueueItem_t *temp = sorted;
      while (temp->next != NULL && temp->next->block_id < current->block_id) {
        temp = temp->next;
      }
      current->next = temp->next;
      temp->next = current;
    }

    current = next;
  }

  *queue = sorted;
}

// Função para implementar o algoritmo C-SCAN
void cscan() { // Ele começa no 100, se nao tiver vai ate o final(1024) e volta
               // pro 0)
  diskQueueItem_t *item;
  diskBlock_t *current_block = state.current_block;

  // Listas para pedidos à direita e à esquerda da posição atual
  diskQueueItem_t *right_queue = NULL, *left_queue = NULL;
  diskQueueItem_t *right_tail = NULL, *left_tail = NULL;

  // Dividir os pedidos em duas partes: direita e esquerda
  while ((item = dequeue_disk_queue(state.queue)) != NULL) {
    // Utiliza a lógica correta para determinar a direção
    if (state.direction == 1 && item->block_id >= current_block->id ||
        state.direction == -1 && item->block_id <= current_block->id) {
      if (right_queue == NULL) {
        right_queue = item;
        right_tail = item;
      } else {
        right_tail->next = item;
        right_tail = item;
      }
    } else {
      if (left_queue == NULL) {
        left_queue = item;
        left_tail = item;
      } else {
        left_tail->next = item;
        left_tail = item;
      }
    }
  }

  // Ordenar as filas de pedidos
  sort_queue(&right_queue);
  sort_queue(&left_queue);

  // Processar a fila à direita
  item = right_queue;
  while (item != NULL) {
    state.last_accessed_block = current_block;
    current_block = disk.head;
    while (current_block != NULL && current_block->id != item->block_id) {
      current_block = current_block->next;
    }
    //printf("Move to block %d\n", current_block->id);

    // Processar a operação de leitura/escrita
    if (item->type == 'r') {
      //printf("Reading data from block %d: %s\n", current_block->id,current_block->data);
    } else if (item->type == 'w') {
      //printf("Writing data to block %d\n", current_block->id);
    }

    diskQueueItem_t *temp = item;
    item = item->next;
    free(temp); // Liberar a memória do item processado
  }

  if (left_queue != NULL) {
    state.last_accessed_block = current_block;
    current_block = disk.tail;
    //printf("Move to block %d \n", current_block->id);
  }

  // Inverter a direção
  state.direction *= -1;
  // Processar a fila à esquerda
  item = left_queue;
  while (item != NULL) {
    state.last_accessed_block = current_block;
    current_block = disk.head;
    while (current_block != NULL && current_block->id != item->block_id) {
      current_block = current_block->next;
    }
    //printf("Move to block %d \n", current_block->id);

    // Processar a operação de leitura/escrita
    if (item->type == 'r') {
      //printf("Reading data from block %d: %s\n", current_block->id,current_block->data);
    } else if (item->type == 'w') {
      //printf("Writing data to block %d\n", current_block->id);
    }

    diskQueueItem_t *temp = item;
    item = item->next;
    free(temp); // Liberar a memória do item processado
  }
  if (right_queue != NULL) {
    state.direction *= -1;
    state.last_accessed_block = current_block;
    //printf("Move to block %d \n", current_block->id);
    return;
  }

  state.last_accessed_block = current_block;
  state.current_block = current_block; // Atualiza o bloco atual
  sleep(5);
}

// Função para gerenciar as solicitações de disco
void diskManager(diskState_t state, disk_t disk) {
  // Verifica se há processos esperando na fila de disco
  if (state.queue->head != NULL) {
    cscan();
  }
}

//------------------------------------------------------------------------------

void init_data_structures() {
  // frameTable
  for (int i = 0; i < NUMBER_OF_FRAMES; i++)
    frameTable.frame[i] = NULL;

  // BCP
  BCP.head = NULL;
  BCP.tail = NULL;

  // Disco

  disk_init(&disk, 1024, 1024); // 1024 blocos de tamanho 1024 bytes
  // Inicializar o bloco atual do estado do disco
  state.current_block = disk.head->next;
  for (int i = 0; i < 99; i++) { // Posição inicial da cabeça como 100
    state.current_block = state.current_block->next;
  }

  // IO queue
  IOqueue.head = NULL;

  // mutex
  sem_init(&mutex, 0, 1);
  sem_init(&disk_semaphore, 0, 0);
}

/*
Calcula o quantum de um processo com base na sua prioridade.
*/
int calculate_quantum(int prio) { return QUANTUM / prio; }

/*
Inicializa o semáforo com o valor v.
*/
void semaphore_init(semaphore_t *semaph, volatile int v) {
  pthread_mutex_init(&mutex_lock, NULL);
  semaph->waiting_list = NULL;
  semaph->v = v;
}

void showMenu() {
  CLEAR_SCREEN

  printf("╔═══════════════════════════╗\n");
  printf("║            MENU           ║\n");
  printf("╠═══════════════════════════╣\n");
  printf("║ [1] Criar Processo        ║\n");
  printf("║ [2] Informações Processo  ║\n");
  printf("║ [3] Informações Semáforo  ║\n");
  printf("║ [4] Informações Memória   ║\n");
  printf("║ [5] Informações Disco     ║\n");
  printf("║ [0] Sair                  ║\n");
  printf("╚═══════════════════════════╝\n");
  printf("  Atualmente Rodando: ");
  if (curr_running) {
    printf("%s (%d)\n", curr_running->proc->name, curr_running->PID);
  } else
    printf("nenhum processo\n");
  printf("\n  Opção: \n");
}

/*
Escalona um processo, o colocando na fila de prontos
*/
void queueProcess(BCPitem_t *proc) {
  if (BCP.head == NULL) {
    BCP.head = proc;
    return;
  }

  BCPitem_t *current = BCP.head;
  while (current->next != NULL) {
    current = current->next;
  }
  current->next = proc;
}

/*
Tira processo da fila de prontos, desenfileirar
*/
void dequeueProcess(BCPitem_t *item) {
  BCPitem_t *aux = BCP.head, *prev = NULL;
  for (; aux && aux != item; prev = aux, aux = aux->next)
    ;
  if (aux) {
    if (prev)
      prev->next = aux->next;
    else // aux is the current head
      BCP.head = BCP.head->next;
    aux->next = NULL;
  } else
    printf("vish!!!!!\n");
}

/*
Deixa o processo dormindo e reescalona-o na fila de prontos
*/
void proc_sleep(BCPitem_t *proc) { // Torna inativo
  proc->status = INACTIVE;
  dequeueProcess(proc);
  queueProcess(proc);
  return;
}

/*
Insere semáforo em uma lista de todos os semáforos que estão sendo usados
*/
void insert_semaphore(semaphore_t *item) {
  if (existing_semaphores.head == NULL) {
    existing_semaphores.head = item;
    return;
  }
  semaphore_t *aux, *prev = NULL;
  for (aux = existing_semaphores.head; aux;
       prev = aux, aux = aux->next) { // Busca se o semáforo já existe
    if (item->name == aux->name) {
      aux->refcount++;
      free(item);
      return;
    }
  }
  item->next = aux;
  if (prev)
    prev->next = item;
  else
    existing_semaphores.head = item;
}

/*
Cria um semáforo com um nome (char) e armazena na lista de semáforos existentes
*/
void createSemaphore(char name) {
  semaphore_t *new = malloc(sizeof(semaphore_t));
  new->next = NULL;
  new->name = name;
  new->refcount = 1;
  semaphore_init(new, 0);
  insert_semaphore(new);
}

/*
Adiciona processos bloqueados que estao esperando por um semáforo( por uma falha
de chamada a semaphoreP)
*/
void sem_queue(sem_list_item_t **list, BCPitem_t *proc) {
  sem_list_item_t *new = malloc(sizeof(sem_list_item_t));
  new->next = NULL;
  new->proc = proc;

  if (*list == NULL) {
    *list = new;
    return;
  }
  sem_list_item_t *aux, *prev = NULL;
  for (aux = *list; aux; prev = aux, aux = aux->next)
    ;
  new->next = aux;
  if (prev)
    prev->next = new;
  else
    (*list) = new;
}

/*
Adquire o semáforo e o processo que está usando o semáforo
Função P(s) do semáforo
*/
void semaphoreP(semaphore_t *semaph, BCPitem_t *proc) {
  pthread_mutex_lock(&mutex_lock);
  if (semaph->v < 0) {
    sem_queue(&semaph->waiting_list, proc);
    proc_sleep(proc);
  }
  semaph->v--;
  pthread_mutex_unlock(&mutex_lock);
}

/*
Realiza em release no mutex_lock e acorda o primeiro processo que estava
esperando na fila
Função V(s) do semáforo
*/
void semaphoreV(semaphore_t *semaph) {
  pthread_mutex_lock(&mutex_lock);
  semaph->v++;
  if (semaph->v <= 0) {
    if (semaph->waiting_list) {
      BCPitem_t *proc = semaph->waiting_list->proc;
      semaph->waiting_list = semaph->waiting_list->next;
      proc_wakeup(proc);
    }
  }
  pthread_mutex_unlock(&mutex_lock);
}

/*
Checa se uma página pertence a um processo que está em um determinado frame
*/
int inFrameTable(int pos,
                 process_t *proc) { // Checa se uma pagina esta em certo frame

  if (frameTable.frame[pos] == NULL)
    for (int i = 0; i < ceil((float)proc->seg_size / PAGE_SIZE); i++)
      if (proc->pTable->address + i ==
          frameTable.frame[pos]->associated_pTable_entry)
        return 1;
  return 0;
}

/*
Carrega um páginas faltantes de um processo para memória e seta sua memória
Leva as paginas do Disco para a Memória
*/
void memLoadReq(process_t *proc) {
  int nFrames = ceil((float)proc->seg_size / PAGE_SIZE);
  long *address = proc->pTable->address;
  int i, j, k = 0;
  int missing = 0;
  memPage_t *newPage = NULL;
  long *associated_page;

  for (i = 0; i < nFrames; i++)
    missing++; // Conta quantas paginas faltam

  for (i = 0; i < nFrames; i++)
    if (address[i] == -1) // está no disco
    {
      if (available_memory == 0) {
        for (j = 0; j <= NUMBER_OF_FRAMES && missing > 0;
             j++) { // Procura um frame vazio
          if (j == NUMBER_OF_FRAMES)
            j = 0;

          if (!inFrameTable(j, proc)) {

            if (frameTable.frame[j]->reference_bit == 1)
              frameTable.frame[j]->reference_bit = 0;
            else {
              *(frameTable.frame[j]->associated_pTable_entry) = -1;
              free(frameTable.frame[j]);
              newPage = malloc(sizeof(memPage_t));
              memset(newPage, 0, sizeof(memPage_t));
              newPage->associated_pTable_entry = &(proc->pTable->address[i]);
              frameTable.frame[j] = newPage;
              frameTable.frame[j]->reference_bit = 1;
              proc->pTable->address[i] = j;
              break;
            }
          }
        }

      } else // memória suficiente para a página
      {
        for (k = 0; k < NUMBER_OF_FRAMES && missing > 0; k++)
          if (frameTable.frame[k] == NULL) {
            newPage = malloc(sizeof(memPage_t));
            memset(newPage, 0, sizeof(memPage_t));
            newPage->associated_pTable_entry = &(proc->pTable->address[i]);
            frameTable.frame[k] = newPage;
            frameTable.frame[k]->reference_bit =
                1; // Bit 1 significa que a página ainda nao foi alterada
            proc->pTable->address[i] = k; // atualiza table de páginas
            break;
          }
        available_memory -= PAGE_SIZE;
      }
      missing--;
    } else // sem falta de páginas, então seta bit de referência para 1
      frameTable.frame[address[i]]->reference_bit = 1;
}

/*
Adiciona processo a uma fila de IO pois ele está bloqueado
*/
void io_queue_add(BCPitem_t *item, char type) {
  IOop_t *new = malloc(sizeof(IOop_t));

  new->remaining_time = item->proc->code[item->next_instruction]->arg;
  new->process = item;
  new->type = type;
  new->next = NULL;

  if (IOqueue.head == NULL) {
    IOqueue.head = new;
    return;
  }

  IOop_t *aux, *prev = NULL;
  for (aux = IOqueue.head; aux; prev = aux, aux = aux->next)
    ;
  new->next = aux;
  if (prev)
    prev->next = new;
  else
    IOqueue.head = new;
}

/*
acorda um processo, e seta status para ready
*/
void proc_wakeup(BCPitem_t *proc) { // Inativo->Ready
  proc->status = READY;
  dequeueProcess(proc);
  queueProcess(proc);
}

/*
Interrompe processo e reescalona-o na fila de prontos
*/
void processInterrupt() { // Blocked->Ready
  BCPitem_t *curr = curr_running;
  if (curr) {
    curr->status = READY;
    dequeueProcess(curr);
    memLoadReq(curr->proc);
    queueProcess(curr);
  }
}

/*
Avança a fila de IO por uma unidade de tempo
*/
void advanceIOqueue() {
  IOop_t *aux = IOqueue.head;
  if (aux) {
    aux->remaining_time--;
    aux->process->remaining_time--;
    aux->process->proc->code[aux->process->next_instruction]->arg--;
    if (aux->remaining_time <= 0) // operação de IO finalizou
    {
      aux->process->status = READY;
      aux->process->next_instruction++;

      // Essa operação era a última do programa
      if (aux->process->next_instruction >= aux->process->proc->nCommands) {
        IOqueue.head = IOqueue.head->next;
        processFinish(aux->process);
        showMenu();
        return;
      }
      IOqueue.head = IOqueue.head->next;

      // reescalona o processo
      processInterrupt();

      dequeueProcess(aux->process);
      queueProcess(aux->process);
    }
  }
}
/*
Procura um Semáforo com o nome especificado
*/
semaphore_t *retrieveSemaphore(char name) {
  semaphore_t *aux = existing_semaphores.head;
  while (aux) {
    if (aux->name == name)
      return aux;
    aux = aux->next;
  }
  printf("vish!!!!\n");
  sleep(1);
  return NULL;
}

/*
Remove um semáforo da lista de semáforos existentes
*/
void removeSemaphore(semaphore_t *item) {
  semaphore_t *aux = existing_semaphores.head, *prev = NULL;
  for (; aux && aux != item; prev = aux, aux = aux->next)
    ;
  if (aux) {
    if (prev)
      prev->next = aux->next;
    else // aux is the current head
      existing_semaphores.head = existing_semaphores.head->next;
    free(aux);
  }
}

/*
Desaloca memória/processo/TUDO para o BCP
*/
void Free(BCPitem_t *a) {
  int i = 0;

  semaphore_t *aux;
  while (a->proc->used_semaphores[i] != '\0') {
    aux = existing_semaphores.head;
    while (aux) {
      if (aux->name == a->proc->used_semaphores[i]) {
        aux->refcount--;
        if (aux->refcount <=
            0) // nenhum processo está usando determinado semáforo
          removeSemaphore(aux);
        break;
      }
      aux = aux->next;
    }
    i++;
  }

  long *address = a->proc->pTable->address;
  int nFrames = ceil((float)a->proc->seg_size / PAGE_SIZE);
  for (i = 0; i < nFrames; i++) // libera a memória virtual
  {
    if (address[i] != -1) {
      frameTable.frame[address[i]] = NULL;
      available_memory += PAGE_SIZE;
    }
  }
  free(a->proc->pTable);
  free(a->proc->code);
  free(a->proc);
  free(a);

  // Limpar a memória alocada para os blocos do disco
  // diskBlock_t *current = disk.head;
  // while (current != NULL) {
  //   diskBlock_t *next = current->next;
  //   free(current->data);
  //   free(current);
  //   current = next;
  // }
}

/*
Validação de nome do processoo
*/
int validateFilename(char *filename) {
  int i;
  for (i = strlen(filename) - 1; i >= 0; i--) {
    if (filename[i] == '.')
      break;
  }
  if (strcmp(&filename[i], FILE_EXT) == 0)
    return 1;

  return 0;
}

/*
Traduz o código de um programa (processo) para uma lista de comandos
e coloca nos ARGS de cada processo
*/
command_t **parsecommands(char *code, int *inst_counter) {
  char temp[100];
  int i;
  int count = 0;
  for (i = 0; code[i] != '\0'; i++)
    if (code[i] == '\n' && code[i + 1] != '\n')
      count++;

  if (code[i - 1] != '\n') // file has no trailing newline
    count++;

  *inst_counter = count;

  char **lines = malloc(count * sizeof(char *));
  command_t **cmd = malloc(count * sizeof(command_t *));
  int oldcount = count;

  count = 0;
  int j;

  // quebrar o código em linhas
  sem_wait(&mutex);
  i = 0;
  while (code[i] != '\0') {
    while (code[i] == '\n' &&
           code[i] !=
               '\0') // caso de haver duas novas linhas no final do arquivo
      i++;

    if (code[i] == '\0')
      break;

    for (j = 0; code[i] != '\n' && code[i] != '\0'; i++, j++) {
      temp[j] = code[i];
    }
    temp[j] = '\0';
    lines[count] = malloc(strlen(temp) + 1);
    strcpy(lines[count], temp);
    count++;
    if (code[i] == '\0')
      break;

    i++;
  }
  sem_post(&mutex);

  // transformar comando em um par de syscall e argumento
  char *arg = NULL;
  for (i = 0; i < count; i++) {
    cmd[i] = malloc(sizeof(command_t));
    cmd[i]->call = strtok(lines[i], " ");
    arg = strtok(NULL, " ");
    if (arg)
      cmd[i]->arg = (int)strtol(arg, NULL, 10);
    else // comando é P() ou V()
      cmd[i]->arg = -1;
  }
  return cmd;
}

/*
Lê cada programa do arquivo e cria um processo para cada um
*/
process_t *readProgramfromDisk(char *filename) {

  FILE *file = fopen(filename, "r");

  if (!file) {
    printf("arquivo não existe!\n");
    sleep(2);
    return NULL;
  }

  // pegar tamanho do arquivo
  fseek(file, 0L, SEEK_END);
  long filesize = ftell(file);
  rewind(file);

  process_t *proc = malloc(sizeof(process_t));

  fscanf(file, "%[^\n]", proc->name);
  fscanf(file, "%d\n", &proc->SID);
  fscanf(file, "%d\n", &proc->priority);
  fscanf(file, "%d\n", &proc->seg_size);
  proc->seg_size *= 1024; // converter kbytes para bytes

  // inicializando tabela de páginas
  proc->pTable = malloc(sizeof(pageTable_t));
  proc->pTable->address =
      malloc(ceil((float)proc->seg_size / PAGE_SIZE) * sizeof(long));

  int i = 0;
  char c;

  long sem_start_pos = ftell(file);
  char prevchar;
  int num_of_semaphores = 0;
  while (1) {
    c = fgetc(file);
    if (c == '\n' || c == EOF)
      break;
    if (c != ' ')
      num_of_semaphores++;
  }

  // alocar espaço para uma lista de semáforos usados
  proc->used_semaphores = malloc(num_of_semaphores + 1); //+1 para um null
  fseek(file, sem_start_pos, SEEK_SET);
  i = 0;
  while (1) {
    c = fgetc(file);
    if (c == '\n' || c == EOF)
      break;
    if (c != ' ') {
      proc->used_semaphores[i] = c;
      i++;
    }
  }
  proc->used_semaphores[num_of_semaphores] = '\0';

  // criar os semáforos em memória
  for (int num = 0; num < num_of_semaphores; num++) {
    createSemaphore(proc->used_semaphores[num]);
  }

  char *code = malloc(filesize - ftell(file) + 1); //+1 para null
  i = 0;
  fgetc(file);
  while (1) {
    c = fgetc(file);
    if (c == EOF)
      break;
    code[i] = c;
    i++;
  }
  code[i] = '\0';
  int inst_number;
  proc->code = parsecommands(code, &inst_number);
  proc->nCommands = inst_number;

  fclose(file);

  return proc;
}

void disk_init(disk_t *disk, int numBlocks, int blockSize) {
  for (int id = 0; id <= numBlocks; id++) {
    char data[blockSize];
    snprintf(data, sizeof(data), "Block %d Data", id);

    diskBlock_t *new_block = (diskBlock_t *)malloc(sizeof(diskBlock_t));
    new_block->id = id;
    new_block->data = strdup(data);
    new_block->next = NULL;

    if (disk->head == NULL) {
      disk->head = new_block;
      disk->tail = new_block;
    } else {
      disk->tail->next = new_block;
      disk->tail = new_block;
    }
  }
}

void disk_write(disk_t *disk, int blockNumber, void *data, int size) {
  diskBlock_t *block = disk->head;
  while (block != NULL && block->id != blockNumber) {
    block = block->next;
  }
  if (block != NULL) {
    memcpy(block->data, data, size);
  } else {
    printf("Error: Block %d not found!\n", blockNumber);
  }
}

void disk_read(disk_t *disk, int blockNumber, void *data, int size) {
  diskBlock_t *block = disk->head;
  while (block != NULL && block->id != blockNumber) {
    block = block->next;
  }
  if (block != NULL) {
    memcpy(data, block->data, size);
  } else {
    printf("Error: Block %d not found!\n", blockNumber);
  }
}

int disk_allocate_block(disk_t *disk) {
  diskBlock_t *block = disk->head;
  while (block != NULL) {
    if (block->data == NULL) {
      block->data = malloc(PAGE_SIZE);
      return block->id;
    }
    block = block->next;
  }
  return -1; // All blocks are allocated
}

void disk_free_block(disk_t *disk, int blockNumber) {
  diskBlock_t *block = disk->head;
  while (block != NULL && block->id != blockNumber) {
    block = block->next;
  }
  if (block != NULL) {
    free(block->data);
    block->data = NULL;
  } else {
    printf("Error: Block %d not found!\n", blockNumber);
  }
}

int disk_is_block_free(disk_t *disk, int blockNumber) {
  diskBlock_t *block = disk->head;
  while (block != NULL && block->id != blockNumber) {
    block = block->next;
  }
  if (block != NULL) {
    return block->data == NULL;
  } else {
    printf("Error: Block %d not found!\n", blockNumber);
    return 0;
  }
}

/*
Printa infos de cada processo
*/
void printProcessInfo(process_t *proc) {
  printf("Name: %s\nSegment ID: %d\nPrioridade: %d\nTamanho do Segmento: %d "
         "bytes\n",
         proc->name, proc->SID, proc->priority, proc->seg_size);
  printf("Semáforos: ");
  for (int i = 0; proc->used_semaphores[i] != '\0'; i++)
    printf("%c ", proc->used_semaphores[i]);
  printf("\n\n");
}

/*
Calcula o tempo pra terminar o processo todo
*/
long calculateRemainingTime(process_t *proc) {
  long remaining_time = 0;
  for (int i = 0; i < proc->nCommands; i++) {
    if (proc->code[i]->arg != -1)
      remaining_time += proc->code[i]->arg;
  }
  return remaining_time;
}

/*
Criar um processo lendo-o do arquivo e ja escalona pra Ready
*/
void processCreate(char *filename) {
  // create a bcp register of said process
  BCPitem_t *new = malloc(sizeof(BCPitem_t));
  new->proc = NULL;
  new->next = NULL;
  new->next_instruction = 0;
  new->proc = readProgramfromDisk(filename);
  if (!new->proc) {
    printf("erro\n");
    sleep(2);
    return;
  }
  long size = new->proc->seg_size;

  for (int i = 0; i < ceil((float)new->proc->seg_size / PAGE_SIZE); i++)
    new->proc->pTable->address[i] = -1;

  // carregar processo na memória
  memLoadReq(new->proc);

  new->PID = PID;
  PID++;
  new->status = READY;
  if (new->proc == NULL) {
    free(new);
    return;
  }

  new->remaining_time = calculateRemainingTime(new->proc);
  new->quantum = calculate_quantum(new->proc->priority);
  new->quantum_counter = 0;

  sem_wait(&mutex);
  processInterrupt();
  // colocar processo na fila de prontos
  queueProcess(new);
  sem_post(&mutex);
  printf("processo iniciado %s\n\n", new->proc->name);
}

/*
Termina o Processo ,desescalona e da Free
*/
void processFinish(BCPitem_t *proc) {
  dequeueProcess(proc);
  Free(proc);
}
/*
Pega o status do processo
*/
char *getStatus(p_state st) {
  if (st == READY)
    return "Ready";
  if (st == RUNNING)
    return "Running";
  if (st == BLOCKED)
    return "Blocked";
  if (st == INACTIVE)
    return "Inactive";
  return "Unknown";
}

/*
Printa infos do menu
*/
void viewProcessInfo() {
  sem_wait(&mutex);
  CLEAR_SCREEN
  if (BCP.head == NULL) {
    printf("Nenhum processo escalonado!\n");
    sleep(3);
    sem_post(&mutex);
    return;
  }
  printf("\nProcessos atuais: ");
  printf("\nPID | Nome (Status)\n");
  printf("-----------------------------------------\n");
  BCPitem_t *aux = NULL;
  for (aux = BCP.head; aux; aux = aux->next)
    printf("%d | %s (%s)\n", aux->PID, aux->proc->name, getStatus(aux->status));

  int search_pid;
  printf("\nQual processo que ver informações?\nPID: ");
  scanf("%d", &search_pid);
  int found = 0;
  for (aux = BCP.head; aux; aux = aux->next)
    if (aux->PID == search_pid) {
      printf("\n\n");
      CLEAR_SCREEN
      printProcessInfo(aux->proc);
      printf("Instrução atual:\n");
      for (int i = 0; i < aux->proc->nCommands; i++) {
        if (i == aux->next_instruction) {
          if (aux->proc->code[i]->arg != -1)
            printf("> %s %d\n", aux->proc->code[i]->call,
                   aux->proc->code[i]->arg);
          else
            printf("> %s\n", aux->proc->code[i]->call);
        } else
          printf("  %s\n", aux->proc->code[i]->call, aux->proc->code[i]->arg);
      }
      printf("\nStatus: %s\n", getStatus(aux->status));
      printf("Remaining time: %ld\n", aux->remaining_time);
      found = 1;
      break;
    }
  if (!found)
    printf("PID inválido\n");
  sleep(3);
  sem_post(&mutex);
  return;
}

/*
Mostra a utilização total da memória
*/
void showMemoryInfo() {
  sem_wait(&mutex);
  CLEAR_SCREEN
  printf("Memória utilizada: %d / %d ", MAX_MEM_SIZE - available_memory,
         MAX_MEM_SIZE);
  printf("(%.1f\%)\n",
         (100 * (MAX_MEM_SIZE - available_memory) / (float)(MAX_MEM_SIZE)));
  sleep(3);
  sem_post(&mutex);
}

/*
Mostra a lista total de semáforos
*/
void showSemaphoreList() {
  sem_wait(&mutex);
  CLEAR_SCREEN
  printf("Lista de semáforos existentes:\n");
  semaphore_t *aux = existing_semaphores.head;
  if (!aux) {
    printf("Nenhum!!\n");
    sleep(3);
    sem_post(&mutex);
    return;
  }
  while (aux) {
    printf("%c | Refcount: %d\n", aux->name, aux->refcount);
    aux = aux->next;
  }
  sleep(3);
  sem_post(&mutex);
}

// Função para mostrar as informações do disco
void showDiskInfo() {
  sem_wait(&mutex);
  CLEAR_SCREEN
  printf("Informações do Disco:\n");

  // Bloco atual do cabeçote
  printf("  Bloco Atual do Cabeçote: ");
  if (state.current_block != NULL) {
    printf("%d\n", state.current_block->id);
  } else {
    printf("Nenhum\n");
  }

  // Último bloco acessado
  printf("  Último Bloco Acessado: ");
  if (state.last_accessed_block != NULL) {
    printf("%d\n", state.last_accessed_block->id);
  } else {
    printf("Nenhum\n");
  }

  // Direção do cabeçote
  printf("  Direção do Cabeçote: ");
  if (state.direction == 1) {
    printf("Crescente\n");
  } else {
    printf("Decrescente\n");
  }

  // Fila de disco
  printf("  Fila de Disco:\n");
  diskQueueItem_t *item = state.queue->head;
  if (item == NULL) {
    printf("    Vazia\n");
  } else {
    while (item != NULL) {
      printf("    Processo: %d, Bloco: %d, Tipo: %c\n", item->process->SID,
             item->block_id, item->type);
      item = item->next;
    }
  }
  sleep(3);
  sem_post(&mutex);
}

/*
Função do menu
*/
void *menu() {
  int opt;
  char filename[128];
  do {
    sem_wait(&mutex);
    showMenu();
    sem_post(&mutex);
    scanf(" %d", &opt);
    switch (opt) {
    case 0:
      stop = 1;
      break;
    case 1:
      sem_wait(&mutex);
      printf("Nome do arquivo do programa: ");
      scanf(" %[^\n]", filename);
      sem_post(&mutex);
      processCreate(filename);
      break;
    case 2:
      viewProcessInfo();
      break;

    case 3:
      showSemaphoreList();
      break;

    case 4:
      showMemoryInfo();
      break;
    case 5:
      showDiskInfo();
      break;
    default:
      printf("Opção inválida!\n");
      break;
    }
  } while (opt != 0);
}

/*
Faz toda interpretação do código
Executa a lógia do Round Robin
*/
void interpreter(BCPitem_t *curr) {

  process_t *proc = curr->proc;
  command_t *instruction = proc->code[curr->next_instruction];

  if (instruction->call[0] == 'e') // exec
  {
    sem_wait(&mutex);
    instruction->arg--;
    curr->remaining_time--;

    if (instruction->arg == 0)
      curr->next_instruction++;
    // incrementa o contador de quantum até o valor máximo de seu quantum
    if (curr->quantum_counter + 1 < curr->quantum) {
      curr->quantum_counter++;
    } else {
      curr->quantum_counter = 0;
      proc_sleep(curr);
    }
    sem_post(&mutex);
    return;
  }

  if (instruction->call[0] == 'r') // read
  {
    sem_wait(&mutex);
    curr_running = NULL;
    curr->status = BLOCKED;

    io_queue_add(curr, 'r');
    enqueue_disk_queue(state.queue, curr->proc, instruction->arg, 'r');
    sleep(1);

    sem_post(&mutex);
  }

  if (instruction->call[0] == 'w') // write
  {
    sem_wait(&mutex);
    curr_running = NULL;
    curr->status = BLOCKED;

    io_queue_add(curr, 'w');
    enqueue_disk_queue(state.queue, curr->proc, instruction->arg, 'w');
    sleep(1);

    sem_post(&mutex);
  }

  if (instruction->call[0] == 'p') // print
  {
    sem_wait(&mutex);
    curr_running = NULL;
    curr->status = BLOCKED;
    io_queue_add(curr, 'p');
    sem_post(&mutex);
  }

  if (instruction->call[0] == 'P') // P(sem)
  {

    sem_wait(&mutex);
    // P(s)
    semaphore_t *argSem = retrieveSemaphore(instruction->call[2]);
    curr->next_instruction++;
    semaphoreP(argSem, curr);
    if (curr->quantum_counter + 1 < curr->quantum) {
      curr->quantum_counter++;
    } else {
      curr->quantum_counter = 0;
      proc_sleep(curr);
    }
    sem_post(&mutex);
  }

  if (instruction->call[0] == 'V') // V(sem)
  {
    sem_wait(&mutex);
    // P(s)
    semaphore_t *argSem = retrieveSemaphore(instruction->call[2]);
    curr->next_instruction++;
    semaphoreV(argSem);
    if (curr->quantum_counter + 1 < curr->quantum) {
      curr->quantum_counter++;
    } else {
      curr->quantum_counter = 0;
      proc_sleep(curr);
    }
    sem_post(&mutex);
  }
}

/*
Executa cada iteração do processo
*/
void *mainLoop() {
  while (!stop) {
    prev_running = curr_running;
    curr_running = BCP.head;

    // pula processos bloqueados
    while (curr_running && curr_running->status == BLOCKED)
      curr_running = curr_running->next;

    if (prev_running != curr_running) {
      sem_wait(&mutex);
      showMenu();
      sem_post(&mutex);
      if (curr_running) {
        sem_wait(&mutex);
        memLoadReq(curr_running->proc);
        sem_post(&mutex);
      }
    }

    if (curr_running) {
      curr_running->status = RUNNING;
      interpreter(curr_running);
      if (curr_running != NULL &&
          curr_running->next_instruction >= curr_running->proc->nCommands) {
        sem_wait(&mutex);
        processFinish(curr_running);
        sem_post(&mutex);
      }
    }
    sem_wait(&mutex);
    advanceIOqueue();
    // Aqui você chama o gerenciador de disco para atender às solicitações
    diskManager(state, disk);
    sem_post(&mutex);
    sem_post(&disk_semaphore);
    usleep(500);
  }
}

void *diskManagerThread(void *arg) {
  disk_t *disk = (disk_t *)arg;
  while (1) {
    // Espera por uma notificação do loop principal
    sem_wait(&disk_semaphore);

    // Processa as requisições de disco
    sem_wait(&mutex);
    if (state.queue->head != NULL) {
      cscan();
    }
    sem_post(&mutex);
  }
}

// Função para gerenciar as solicitações de I/O
void *ioManagerThread(void *arg) {
  IOqueue_t *IOqueue = (IOqueue_t *)arg;
  while (1) {
    // Espera por uma notificação do loop principal
    sem_wait(&io_semaphore);

    // Processa as requisições de I/O
    sem_wait(&mutex);

    // Avança a fila de IO
    advanceIOqueue();

    sem_post(&mutex);
  }
}

// Função para gerenciar as solicitações de memória
void *memoryManagerThread(void *arg) {
  frameTable_t *frameTable = (frameTable_t *)arg;
  while (1) {
    // Espera por uma notificação do loop principal
    sem_wait(&memory_semaphore);

    // Processa as requisições de memória
    sem_wait(&mutex);

    // Simplesmente chama memLoadReq para atualizar a tabela de páginas
    memLoadReq(curr_running->proc);

    sem_post(&mutex);
  }
}




/*
Duas threads rodando no programa
Uma vai cuidar do menu e a outra vai cuidar da lógica do Programa em si
*/
int main() {
  CLEAR_SCREEN
  printf("███████╗ ██████╗       ███████╗███████╗███████╗ ██████╗  █████╗ "
         "██████╗  ██████╗ \n");
  printf("██╔════╝██╔═══██╗      ██╔════╝██╔════╝██╔════╝██╔════╝ "
         "██╔══██╗██╔══██╗██╔═══██╗\n");
  printf("███████╗██║   ██║█████╗███████╗███████╗█████╗  ██║  ███╗███████║██║  "
         "██║██║   ██║\n");
  printf("╚════██║██║   ██║╚════╝╚════██║╚════██║██╔══╝  ██║   ██║██╔══██║██║  "
         "██║██║   ██║\n");
  printf("███████║╚██████╔╝      ███████║███████║███████╗╚██████╔╝██║  "
         "██║██████╔╝╚██████╔╝\n");
  printf("╚══════╝ ╚═════╝       ╚══════╝╚══════╝╚══════╝ ╚═════╝ ╚═╝  "
         "╚═╝╚═════╝  ╚═════╝ \n");
  sleep(3);
  init_data_structures();
  state.direction = 1;
  pthread_t kernel;         // Thread do kernel
  pthread_t sim_menu;       // Thread do menu
  pthread_t memory_manager; // Thread da memoria
  pthread_t io_manager;     // Thread de io
  pthread_t disk_manager;   // Thread do disco
  
  pthread_create(&sim_menu, NULL, menu, NULL);
  pthread_create(&kernel, NULL, &mainLoop, NULL);
  pthread_create(&memory_manager, NULL, memoryManagerThread , NULL);
  pthread_create(&io_manager, NULL, ioManagerThread, NULL);
  pthread_create(&disk_manager, NULL, diskManagerThread, &disk);


  
  pthread_join(kernel, NULL);
  pthread_join(sim_menu, NULL);
  pthread_join(memory_manager, NULL);
  pthread_join(io_manager, NULL);
  pthread_join(disk_manager, NULL);
}