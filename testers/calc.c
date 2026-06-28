#include <uriscv/liburiscv.h>

#include "h/tconst.h"
#include "h/print.h"

void main() {
    char buffer[16];
    int status;
    
    // 1. Leggi e controlla il primo numero
    print(WRITETERMINAL, "Enter first digit (0-9): ");
    status = SYSCALL(READTERMINAL, (int)&buffer[0], 0, 0);
    
    // Se lo status non è 2 (es. l'utente ha scritto "42\n" -> status 3, o solo "\n" -> status 1)
    if (status != 2 || buffer[0] < '0' || buffer[0] > '9') {
        print(WRITETERMINAL, "Error: Not a single digit!\n");
        SYSCALL(TERMINATE, 0, 0, 0);
    }
    int num1 = buffer[0] - '0';

    // 2. Leggi e controlla l'operatore
    print(WRITETERMINAL, "Enter operator (+, -, *, /): ");
    status = SYSCALL(READTERMINAL, (int)&buffer[0], 0, 0);
    if (status != 2) {
        print(WRITETERMINAL, "Error: Invalid operator input!\n");
        SYSCALL(TERMINATE, 0, 0, 0);
    }
    char op = buffer[0];

    // 3. Leggi e controlla il secondo numero
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
        default:
            print(WRITETERMINAL, "Error: Unknown operator!\n");
            SYSCALL(TERMINATE, 0, 0, 0);
    }

    // 5. Stampa del risultato (gestisce anche numeri negativi o a 2 cifre come 81)
    print(WRITETERMINAL, "Result: ");
    if (result < 0) {
        print(WRITETERMINAL, "-");
        result = -result;
    }
    
    if (result >= 10) {
        buffer[0] = (result / 10) + '0';
        buffer[1] = (result % 10) + '0';
        buffer[2] = '\n';
        buffer[3] = '\0';
    } else {
        buffer[0] = result + '0';
        buffer[1] = '\n';
        buffer[2] = '\0';
    }
    print(WRITETERMINAL, buffer);

    // 6. Termina e torna alla Shell
    SYSCALL(TERMINATE, 0, 0, 0);
}
