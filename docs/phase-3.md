# PandOSsh - Fase 3

## Variabili Globali


## Note TLBPagerHandler

Page fault on a load operation e Page fault on a store operation, non vengono esplicitamente distinte. Ma c'e' distinzione sul fatto se necessita' il salvataggio della pagina nel flash andando a verificare se tale pagina ha il dirty bit attivo o meno (impostato durante l'inizializzazione).


## thought challenge
5.3 Updating a Page Table and the TLB Atomically
The order of operations for the Pager are important. Specifically:
• When refreshing the backing store, one must first update the Page Table, and possibly the TLB,
before performing the write operation.
• When reading in from the backing store, one must first perform the read operation before
updating the Page Table and TLB.
Thought Challenge: Why must these operations be done in the prescribed order?
Similarly, the updating of a Page Table entry and its cached counterpart in the TLB, must be
done atomically. This is accomplished in μRISC-V by disabling interrupts before the update state-
ments, and then reenabling them immediately afterwards. Interrupts are disabled and enabled via the
MIE bit of the STATUS register (disable: setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK), enable:
setSTATUS(getSTATUS() | MSTATUS_MIE_MASK)).
Thought Challenge: Why must the Page Table and TLB be updated atomically?