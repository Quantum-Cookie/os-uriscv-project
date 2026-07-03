#include <uriscv/liburiscv.h>

#include "h/tconst.h"
#include "h/print.h"

void main() {
    char buffer[16];
    int status;
    
    // Leggi e controlla il primo numero
    print(WRITETERMINAL, "Enter first digit (0-9): ");
    status = SYSCALL(READTERMINAL, (int)&buffer[0], 0, 0);
    
    // Se lo status non è 2 ovvero l'utente ha inserito 0 o piu' di un numero/carattere
    if (status != 2 || buffer[0] < '0' || buffer[0] > '9') {
        print(WRITETERMINAL, "Error: Not a single digit!\n");
        SYSCALL(TERMINATE, 0, 0, 0);
    }
    int num1 = buffer[0] - '0';

    // Leggi e controlla l'operatore
    print(WRITETERMINAL, "Enter operator (+, -, *, /): ");
    status = SYSCALL(READTERMINAL, (int)&buffer[0], 0, 0);
    char op = buffer[0];
    if (status != 2 || (op != '+' && op != '-' && op != '*' && op != '/')) {
        print(WRITETERMINAL, "Error: Invalid operator input!\n");
        SYSCALL(TERMINATE, 0, 0, 0);
    }

    // Leggi e controlla il secondo numero
    print(WRITETERMINAL, "Enter second digit (0-9): ");
    status = SYSCALL(READTERMINAL, (int)&buffer[0], 0, 0);
    if (status != 2 || buffer[0] < '0' || buffer[0] > '9') {
        print(WRITETERMINAL, "Error: Not a single digit!\n");
        SYSCALL(TERMINATE, 0, 0, 0);
    }
    int num2 = buffer[0] - '0';

    // 4. Calcolo del risultato
    int result = 0;
    switch(op) {
        case '+': result = num1 + num2; break;
        case '-': result = num1 - num2; break;
        case '*': result = num1 * num2; break;
        case '/': 
            if (num2 == 0) {
                print(WRITETERMINAL, "Error: Division by zero!\n");
                SYSCALL(TERMINATE, 0, 0, 0);
            }
            result = num1 / num2; 
            break;
    }

    // Stampa del risultato 
    print(WRITETERMINAL, "Result: ");
    // Se risultato negativo stampa prima -
    if (result < 0) {
        print(WRITETERMINAL, "-");
        result = -result;
    }
    
    // Gestione buffer per valori con 2 cifre (massimo valore possibile e' 81)
    if (result >= 10) {
        buffer[0] = (result / 10) + '0';
        buffer[1] = (result % 10) + '0';
        buffer[2] = '\n';
        buffer[3] = '\0';
    } 
    // Gestione buffer per valori con 1 cifra
    else {
        buffer[0] = result + '0';
        buffer[1] = '\n';
        buffer[2] = '\0';
    }
    print(WRITETERMINAL, buffer);

    // Termina e torna alla Shell
    SYSCALL(TERMINATE, 0, 0, 0);
}
