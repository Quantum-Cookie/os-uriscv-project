# PandOSsh

## Description
PandOSsh è un progetto educativo pensato per comprendere il funzionamento reale di un sistema operativo.  

- Fase 1: Implementazione delle strutture dati fondamentali che rappresentano entità dai livelli superiori e funzioni di gestione:
    - **PCB (Process Control Block):** Rappresentano i processi. Viene gestito l'allocazione, manipolazione code e alberi di processi.
    - **ASL (Active Semaphore List):** Gestisce lista semafori e i relativi processi bloccati.

- Fase 2: Implementazione del nucleo introducendo la gestione delle eccezioni e multiprogrammazione. I componenti principali sono:
    - **Scheduler:**  Implementa un algoritmo di scheduling preemptive Round-Robin.
    - **SYSCALL:** Fornisce un'interfaccia di servizi di sistema (NSYS1-NSYS10) per il controllo dei processi e interazione con i dispositivi I/O.
    - **Gestione Interrupt:** Gestisce gli Interrupt provenienti dai dispositivi di I/O e dai timer.
    - **Pass Up or Die:** Gestisce le chiamate alle SYSCALL no definite, i Program Trap e le eccezioni TLB.

## Requirements
- **CMake** >= 3.25
- **Toolchain:** gcc-riscv64-unknown-elf

## Emulator
- Usiamo **uRISCV** per l'esecuzione il progetto - [GitHub uRISCV](https://github.com/virtualsquare/uriscv)

## Build 
Per compilare il progetto eseguire i seguenti comandi nella cartella principale:

```bash
cmake -B build
cmake --build build
```

## Run
Dopo aver compilato il programma (attualmente insieme al file di test `/phase2/p2test.c`) è possibile eseguirlo utilizzando emulatore specificato:

```bash
uriscv
```

Caricare in seguito all'avvio il file di configurazione `config_machine.json`.

In alternativa è possibile avviare l'emulatore con già la configurazione caricata:
```bash
uriscv config_machine.json
```

## Testing
Attualmente il progetto è configurato per essere compilato insieme al file di test `/phase2/p2test.c`.

Una volta avviata la macchina e fatta partire l'esecuzione, sul terminale dalla quale è stata avviata si dovrebbero vedere i risultati dei test.

Il corretto superamento è confermato dal messaggio:
>**System halted**.