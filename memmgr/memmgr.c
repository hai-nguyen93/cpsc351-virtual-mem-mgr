//
//  memmgr.c
//  memmgr
//
//  Created by William McCarthy on 17/11/20.
//  Copyright © 2020 William McCarthy. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define ARGC_ERROR 1
#define FILE_ERROR 2
#define BUFLEN 256
#define FRAME_SIZE  256

char main_mem[65536];
FILE* fsto;

//-------------------------------------------------------------------
unsigned getpage(unsigned x) { return (0xff00 & x) >> 8; }

unsigned getoffset(unsigned x) { return (0xff & x); }

void getpage_offset(unsigned x) {
  unsigned  page   = getpage(x);
  unsigned  offset = getoffset(x);
  printf("x is: %u, page: %u, offset: %u, address: %u, paddress: %u\n", x, page, offset,
         (page << 8) | getoffset(x), page * 256 + offset);
}

unsigned getframe_page_table(unsigned logic_add, unsigned page, int table[],
          int *current_frame, int *page_fault_count){
  // if page table hit
  if (table[page] != -1) { return table[page];}

  // page table miss
  table[page] = (*current_frame);
  (*current_frame)++;
  (*page_fault_count)++;
  int offset = (logic_add / FRAME_SIZE) * FRAME_SIZE;
  fseek(fsto, offset, 0);
  fread(&main_mem[table[page]*FRAME_SIZE],sizeof(char),256,fsto);
  return table[page];
}

int main(int argc, const char* argv[]) {
  FILE* fadd = fopen("addresses.txt", "r");    // open file addresses.txt  (contains the logical addresses)
  if (fadd == NULL) { fprintf(stderr, "Could not open file: 'addresses.txt'\n");  exit(FILE_ERROR);  }

  FILE* fcorr = fopen("correct.txt", "r");     // contains the logical and physical address, and its value
  if (fcorr == NULL) { fprintf(stderr, "Could not open file: 'correct.txt'\n");  exit(FILE_ERROR);  }

  char buf[BUFLEN];
  unsigned   page, offset, physical_add, frame = 0;
  unsigned   logic_add;                  // read from file address.txt
  unsigned   virt_add, phys_add, value;  // read from file correct.txt

  printf("ONLY READ FIRST 20 entries -- TODO: change to read all entries\n\n");

  // not quite correct -- should search page table before creating a new entry
      //   e.g., address # 25 from addresses.txt will fail the assertion
      // TODO:  add page table code
      // TODO:  add TLB code

  fsto = fopen("BACKING_STORE.bin", "rb");   
  if (fsto == NULL) { fprintf(stderr, "Could not open file: 'BACKING_STORE.bin'\n");  exit(FILE_ERROR);  }

  unsigned current_frame=0;
  int page_table[256];
  for (int i=0; i<256; ++i) { page_table[i]=-1; }
  int tlb[16];
  int access_count=0, page_fault_count=0, tlb_hit_count=0;

  while (fscanf(fadd, "%d", &logic_add) != EOF) {
    ++access_count;

    fscanf(fcorr, "%s %s %d %s %s %d %s %d", buf, buf, &virt_add,
           buf, buf, &phys_add, buf, &value);  // read from file correct.txt

    // fscanf(fadd, "%d", &logic_add);  // read from file address.txt
    page   = getpage(  logic_add);
    offset = getoffset(logic_add);
    frame = getframe_page_table(logic_add, page, page_table, &current_frame, &page_fault_count);

    physical_add = frame * FRAME_SIZE + offset;
    int val = (int)(main_mem[physical_add]);
    fseek(fsto, logic_add, 0);
    // char x = (fgetc(fsto));
    
    // for(int i=0; i<256; ++i){
    //   printf("%d ", main_mem[frame * FRAME_SIZE +i]);
    //   if (i%5==0) printf("\n");
    // }

    // todo: read BINARY_STORE and confirm value matches read value from correct.txt
    
    printf("logical: %5u (page: %3u, offset: %3u) ---> physical: %5u -> value: %d-- passed\n", 
            logic_add, page, offset, physical_add, val);
    // printf("x= %d\n", x);
    if (access_count % 5 == 0) { printf("\n"); }

    assert(physical_add == phys_add);
    assert(value == val);
  }
  fclose(fcorr);
  fclose(fadd);
  fclose(fsto);
  
  printf("ONLY READ FIRST 20 entries -- TODO: change to read all entries\n\n");
  
  printf("ALL logical ---> physical assertions PASSED!\n");
  printf("!!! This doesn't work passed entry 24 in correct.txt, because of a duplicate page table entry\n");
  printf("--- you have to implement the PTE and TLB part of this code\n");

//  printf("NOT CORRECT -- ONLY READ FIRST 20 ENTRIES... TODO: MAKE IT READ ALL ENTRIES\n");
  
  printf("page fault count: %d-------total count:%d\n", page_fault_count, access_count);
  printf("page fault rate: %.3f\n", 1.0f*page_fault_count/access_count);
  printf("\n\t\t...done.\n");
  return 0;
}
