#include <uriscv/liburiscv.h>

#include "h/tconst.h"
#include "h/print.h"

int strcmp(char *str1, char *str2) {
    // Cicla finché i caratteri sono uguali e non siamo arrivati alla fine di str1
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    
    // Se sono arrivate alla fine insieme, *str1 e *str2 saranno entrambi '\0' (uguali)
    if (*str1 == *str2) {
        return 1; // Stringhe uguali
    } else {
        return 0; // Stringhe diverse
    }
}

void main() {
char buffer[64];

    // Messaggio di benvenuto stampato solo all'avvio
    print(WRITETERMINAL, "--- Welcome to PandOS Shell ---\n");

    while(1) {
        // Stampiamo il prompt per l'input
        print(WRITETERMINAL, "> ");
        
        // Leggiamo il comando dall'utente
        int status = SYSCALL(READTERMINAL, (int)&buffer[0], 0, 0);
        
        // Controlliamo che la lettura sia andata a buon fine
        if (status > 0) {
            // Rimuoviamo il carattere '\n' inserendo il terminatore di stringa
            buffer[status - 1] = EOS;

            // 1. Controllo per il comando speciale di uscita
            if (strcmp(buffer, EXIT)) {
                print(WRITETERMINAL, "Shell exiting. Goodbye!\n");
                // Chiamiamo la SYS2 (Terminate Process) per chiudere la Shell stessa
                SYSCALL(TERMINATE, 0, 0, 0); 
            }
            
            // 2. Controllo dei comandi legati agli U-procs tramite i rispettivi ASID
            else if (strcmp(buffer, "fibeight")) {
                SYSCALL(EXECUTE, ASID_FIBEIGHT, 0, 0);
            }
            else if (strcmp(buffer, "echo")) {
                SYSCALL(EXECUTE, ASID_ECHO, 0, 0);
            }
            else if (strcmp(buffer, "fibeleven")) {
                SYSCALL(EXECUTE, ASID_FIBELEVEN, 0, 0);
            }
            else if (strcmp(buffer, "uname")) {
                SYSCALL(EXECUTE, ASID_UNAME, 0, 0);
            }
            else if (strcmp(buffer, "date")) {
                SYSCALL(EXECUTE, ASID_DATE, 0, 0);
            }
            else if (strcmp(buffer, "sl")) {
                SYSCALL(EXECUTE, ASID_SL, 0, 0);
            }
            else if (strcmp(buffer, "calc")) {
                SYSCALL(EXECUTE, ASID_CALC, 0, 0);
            }
            
            // 3. Gestione comando sconosciuto
            else {
                // Se l'utente preme solo invio (stringa vuota), non stampiamo l'errore
                if (buffer[0] != EOS) {
                    print(WRITETERMINAL, "Unknown command. Available: fibeight, echo, fibeleven, uname, date, sl, calc, exit\n");
                }
            }
        }
    }
}
