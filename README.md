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

- **Fase 3:** Implementazione del **Livello di Supporto** per l'esecuzione di processi utente (*U-proc*) in spazi di indirizzamento logici isolati (Virtual Memory). I componenti principali sono:
    - **Gestore TLB (Pager):** Gestione del page fault e implementazione della memoria virtuale.
    - **Gestore Non-TLB:** Gestione delle eccezioni di tipo Program Trap e delle SYSCALL con valore >= 1.
    - **Supporto I/O orientato ai caratteri:** Astrazione e gestione dei terminali per l'input/output dei processi utente

## Documentation
- [Phase 1](./docs/phase-1.md)
- [Phase 2](./docs/phase-2.md)
- [Phase 3](./docs/phase-3.md)

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
Dopo aver compilato il programma è possibile eseguirlo utilizzando emulatore specificato:

```bash
uriscv
```

Caricare in seguito all'avvio il file di configurazione `config_machine.json`.

In alternativa è possibile avviare l'emulatore con già la configurazione caricata:
```bash
uriscv <path di config_machine.json>
```

Una volta avviata la macchina e fatta partire l'esecuzione, per aprire la finestra del terminale principale (Terminal 0) e iniziare a digitare i comandi: 
- Clicca nel menu dell'emulatore su **Windows** → **Terminal 0** 
- La scorciatoia da tastiera <kbd>Alt</kbd> + <kbd>0</kbd>
