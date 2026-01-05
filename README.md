# PandOSsh

## Description
PandOSsh è un progetto educativo pensato per comprendere il funzionamento reale di un sistema operativo.  

## Requirements
- **CMake** >= 3.25
- **Toolchain:** gcc-riscv64-unknown-elf

## Emulator
- Usiamo **uRISCV** per eseguire il progetto - [GitHub uRISCV](https://github.com/virtualsquare/uriscv)

## Build 
Per compilare il progetto eseguire i seguenti comandi nella cartella principale:
```bash
cmake -B build
cmake --build build
```

## Run
Dopo aver compilato il programma (attualmente insieme al file di test `/phase1/p1test.c`) è possibile eseguirlo utilizzando emulatore specificato:

```bash
uriscv
```

Caricare in seguito all'avvio il file di configurazione `config_machine.json`.

Oppure per avviare l'emulatore con già la configurazione caricata:
```bash
uriscv config_machine.json
```
