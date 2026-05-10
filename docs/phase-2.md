# PandOSsh - Fase 2

## Variabili Globali
Sono state introdotte una serie di variabili condivise tra i diversi moduli della fase 2:

`int processCount`: Rappresenta il numero dei processi attualmente attivi.

`int softBlockCount`: Rappresenta il numero dei processi in attesa di qualche evento (i.e. I/O, timer).

`struct list_head readyQueue`: Coda dei PCB ready, ovvero che possono essere scelti dallo scheduler per l'esecuzione.

`pcb_t* currentProcess`: Puntatore al PCB del processo in attuale esecuzione.

`int deviceSemaphore[NRSEMAPHORES]`: Array di interi come semaforo per ogni dispositivo esterno. Seguendo la seguente mappatura:

| Index       | Device Class               | IntlineNo |
| :---------- | :------------------------- | :-------- |
| **0**       | Interval Timer             | 2         |
| **1 – 8**   | Disk Devices               | 3         |
| **9 – 16**  | Flash Devices              | 4         |
| **17 – 24** | Network (Ethernet) Devices | 5         |
| **25 – 32** | Printer Devices            | 6         |
| **33 – 40** | Terminal Devices (TX)      | 7         |
| **41 – 48** | Terminal Devices (RX)      | 7         |

`pcb_t* rootProcess`: Il primo processo che viene instanziato, il quale rappresenta la radice dell'intero albero dei processi.

`cpu_t startRunningTime`: Memorizza il timestamp del momento in cui **currentProcess** inizia la sua esecuzione per calcolare tempo totale di CPU consumato.