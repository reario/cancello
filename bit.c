/*
--------------
Setting a bit
-------------
Use the bitwise OR operator (|) to set a bit.
number |= 1 << x;
That will set bit x.


--------------
Clearing a bit
--------------
Use the bitwise AND operator (&) to clear a bit.
number &= ~(1 << x);
That will clear bit x. You must invert the bit string with the bitwise NOT operator (~), then AND it.




--------------
Toggling a bit
--------------
The XOR operator (^) can be used to toggle a bit.
number ^= 1 << x;
That will toggle bit x.




--------------
Checking a bit
--------------
To check a bit, AND it with the bit you want to check:
bit = number & (1 << x);
That will put the value of bit x into the variable bit

*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>

void usage() {
  printf("use: operate [on|off|invert|read]\n");
}

uint16_t read_state(uint16_t reg, uint16_t q) {
  /*legge q-esomo bit di reg*/
 return (reg & (1<<q));
}

uint16_t read_single_state(uint16_t reg, uint16_t q) {
  /*legge q-esomo bit di reg*/
  uint16_t i;
  i=(1<<q); /* 2^q */
  if (reg & i) {return 1;} else {return 0;};
}

uint16_t set_state(uint16_t reg, uint16_t q) {
  /*set del bit q-esimo*/
  return reg | (1<<q);
}

uint16_t reset_state(uint16_t reg, uint16_t q) {
  /*azzera il q-esimo bit*/
  return reg & ~(1<<q);
}

uint16_t invert_state(uint16_t reg, uint16_t q) {
  /*inverte il q-esimo bit*/
  return reg^(1<<q);
}

void printbitssimple(uint16_t n) {
  /*dato l'intero n stampa la rappresentazione binaria*/
  unsigned int i;
  i = 1<<(sizeof(n) * 8 - 1); /* 2^n */
  while (i > 0) {
    if (n & i)
      printf("1");
    else
      printf("0");
    i >>= 1;
  }
  printf("\n");
}
