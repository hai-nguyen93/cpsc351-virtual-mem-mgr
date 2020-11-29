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
char main_mem_fifo[32768]; // 128 physical frames
int page_queue[128];
int qhead=0, qtail=0;
int tlb[16][2]; int current_tlb_entry=0;
int page_table[256];
int current_frame=0;
FILE* fsto;

// data for statistics
int pfc[5], pfc2[5]; // page fault count
int tlbh[5], tlbh2[5]; // tlb hit count
int count[5], count2[5]; // access count

//-------------------------------------------------------------------
unsigned getpage(unsigned x) { return (0xff00 & x) >> 8; }

unsigned getoffset(unsigned x) { return (0xff & x); }

void getpage_offset(unsigned x) {
  unsigned  page   = getpage(x);
  unsigned  offset = getoffset(x);
  printf("x is: %u, page: %u, offset: %u, address: %u, paddress: %u\n", x, page, offset,
         (page << 8) | getoffset(x), page * 256 + offset);
}

int tlb_contains(unsigned x){
  for (int i=0; i<16; i++){
    if (tlb[i][0] == x) { return i; }
  }
  return -1;
}

void update_tlb(unsigned page){
  tlb[current_tlb_entry][0] = page;
  tlb[current_tlb_entry][1] = page_table[page];
  current_tlb_entry = (current_tlb_entry+1) % 16; // round-robin
}

unsigned getframe(unsigned logic_add, unsigned page,
         int *page_fault_count, int *tlb_hit_count){
  // tlb hit         
  int tlb_index = tlb_contains(page);
  if (tlb_index != -1) {
    (*tlb_hit_count)++;
    return tlb[tlb_index][1];
  }

  // tlb miss
  // if page table hit
  if (page_table[page] != -1) { 
    update_tlb(page);
    return page_table[page];
  }

  // page table miss -> page fault
  // find page location in backing_store
  int offset = (logic_add / FRAME_SIZE) * FRAME_SIZE;
  fseek(fsto, offset, 0);

  // bring data into memory, update tlb and page table
  page_table[page] = current_frame;
  current_frame = (current_frame+1) % 256;
  (*page_fault_count)++;  
  fread(&main_mem[page_table[page]*FRAME_SIZE],sizeof(char),256,fsto);
  update_tlb(page);
  return page_table[page];
}

int get_available_frame(unsigned page){
  // empty queue
  if (qhead==0 && qtail==0 && page_queue[qhead]==-1){
    ++qtail;
    page_queue[qhead]=page;
    return qhead;
  }

  // queue not full
  if (page_queue[qtail]==-1){
    page_queue[qtail]=page;
    int val = qtail;
    qtail=(qtail+1)%128;
    return val;
  }

  // queue full
  if (qhead==qtail && page_queue[qtail]!=-1){
    page_queue[qhead]=page;
    int val=qhead;
    qhead=(qhead+1)%128;
    qtail=(qtail+1)%128;
    return val;
  }
}

unsigned getframe_fifo(unsigned logic_add, unsigned page,
         int *page_fault_count, int *tlb_hit_count) {
  // tlb hit         
  int tlb_index = tlb_contains(page);
  if (tlb_index != -1) {
    if (page_queue[tlb[tlb_index][1]]==page) {
      (*tlb_hit_count)++;
      return tlb[tlb_index][1];
    }
  }

  // tlb miss, page table hit
  if (page_table[page]!=-1 && page_queue[page_table[page]]==page) { 
    update_tlb(page);
    return page_table[page];
  }

  // page table miss -> page fault    
  // find location in backing_store
  int offset = (logic_add / FRAME_SIZE) * FRAME_SIZE;
  fseek(fsto, offset, 0);      

  // bring data into memory, update tlb and page table
  int available_frame = get_available_frame(page);
  fread(&main_mem_fifo[available_frame*FRAME_SIZE],sizeof(char),256,fsto);
  page_table[page] = available_frame;
  (*page_fault_count)++;
  update_tlb(page);
  return page_table[page];
}

void part1();
void part2();
int main(int argc, const char* argv[]) {
  // initialize statistics data
  for (int i=0; i<5; ++i){
    pfc[i]=0; pfc2[i]=0;
    tlbh[i]=0; tlbh2[i]=0;
    count[i]=0; count2[i]=0;
  }

  part1(); // 256 physical frames
  part2(); // 128 physical frames

  // Statistics
  printf("\nPart 1 Statistics (256 frames):\n");
  printf("access count\ttlb hit count\tpage fault count\ttlb hit rate\tpage fault rate\n");
  for (int i=0; i<5; ++i){
    printf("%d\t\t%d\t\t%d\t\t\t%.4f\t\t%.4f\n", count[i], tlbh[i], pfc[i],
           1.0f*tlbh[i]/count[i], 1.0f*pfc[i]/count[i]);
  }

  printf("\nPart 2 Statistics (128 frames):\n");
  printf("access count\ttlb hit count\tpage fault count\ttlb hit rate\tpage fault rate\n");
  for (int i=0; i<5; ++i){
    printf("%d\t\t%d\t\t%d\t\t\t%.4f\t\t%.4f\n", count2[i], tlbh2[i], pfc2[i],
           1.0f*tlbh2[i]/count2[i], 1.0f*pfc2[i]/count2[i]);
  }
  printf("\n\t\tDONE!\n");
  return 0;
}

void part1(){
  FILE* fadd = fopen("addresses.txt", "r");    // open file addresses.txt  (contains the logical addresses)
  if (fadd == NULL) { fprintf(stderr, "Could not open file: 'addresses.txt'\n");  exit(FILE_ERROR);  }

  FILE* fcorr = fopen("correct.txt", "r");     // contains the logical and physical address, and its value
  if (fcorr == NULL) { fprintf(stderr, "Could not open file: 'correct.txt'\n");  exit(FILE_ERROR);  }

  char buf[BUFLEN];
  unsigned   page, offset, physical_add, frame = 0;
  unsigned   logic_add;                  // read from file address.txt
  unsigned   virt_add, phys_add, value;  // read from file correct.txt

  fsto = fopen("BACKING_STORE.bin", "rb");   
  if (fsto == NULL) { fprintf(stderr, "Could not open file: 'BACKING_STORE.bin'\n");  exit(FILE_ERROR);  }
  
  // Initialize page table, tlb
  for (int i=0; i<256; ++i) { page_table[i]=-1; }
  for (int i=0; i<16; ++i) { tlb[i][0]=-1; }
  
  int access_count=0, page_fault_count=0, tlb_hit_count=0;
  current_frame = 0;
  current_tlb_entry = 0;

  while (fscanf(fadd, "%d", &logic_add) != EOF) {
    ++access_count;

    fscanf(fcorr, "%s %s %d %s %s %d %s %d", buf, buf, &virt_add,
           buf, buf, &phys_add, buf, &value);  // read from file correct.txt

    // fscanf(fadd, "%d", &logic_add);  // read from file address.txt
    page   = getpage(  logic_add);
    offset = getoffset(logic_add);
    frame = getframe(logic_add, page, &page_fault_count, &tlb_hit_count);

    physical_add = frame * FRAME_SIZE + offset;
    int val = (int)(main_mem[physical_add]);

    // update tlb hit count and page fault count every 200 accesses
    if (access_count > 0 && access_count%200==0){
      tlbh[(access_count/200)-1] = tlb_hit_count;
      pfc[(access_count/200)-1] = page_fault_count;
      count[(access_count/200)-1] = access_count;
    }
    
    printf("logical: %5u (page: %3u, offset: %3u) ---> physical: %5u -> value: %d-- passed\n", 
            logic_add, page, offset, physical_add, val);
    if (access_count % 5 == 0) { printf("\n"); }

    assert(physical_add == phys_add);
    assert(value == val);
  }
  fclose(fcorr);
  fclose(fadd);
  fclose(fsto);
  
  printf("ALL logical ---> physical assertions PASSED!\n");
  printf("ALL read memory value assertions PASSED!\n"); 

  printf("\n\t\t...Part 1 done.\n");
}

void part2(){
  char buf[BUFLEN];
  unsigned   page, offset, physical_add, frame = 0;
  unsigned   logic_add;                  // read from file address.txt
  unsigned   virt_add, phys_add, value;  // read from file correct.txt

  printf("\n Start Part 2...\n");

  // Initialize page table, tlb, page queue
  for (int i=0; i<256; ++i) { page_table[i]=-1; }
  for (int i=0; i<16; ++i) { tlb[i][0]=-1; }
  for (int i=0; i<128; ++i) { page_queue[i]=-1; }
  
  int access_count=0, page_fault_count=0, tlb_hit_count=0;
  qhead=0; qtail=0;

  FILE* fadd = fopen("addresses.txt", "r");    // open file addresses.txt  (contains the logical addresses)
  if (fadd == NULL) { fprintf(stderr, "Could not open file: 'addresses.txt'\n");  exit(FILE_ERROR);  }

  FILE* fcorr = fopen("correct.txt", "r");     // contains the logical and physical address, and its value
  if (fcorr == NULL) { fprintf(stderr, "Could not open file: 'correct.txt'\n");  exit(FILE_ERROR);  }

  fsto = fopen("BACKING_STORE.bin", "rb");   
  if (fsto == NULL) { fprintf(stderr, "Could not open file: 'BACKING_STORE.bin'\n");  exit(FILE_ERROR);  }

  while (fscanf(fadd, "%d", &logic_add) != EOF) {
    ++access_count;

    fscanf(fcorr, "%s %s %d %s %s %d %s %d", buf, buf, &virt_add,
           buf, buf, &phys_add, buf, &value);  // read from file correct.txt

    // fscanf(fadd, "%d", &logic_add);  // read from file address.txt
    page   = getpage(  logic_add);
    offset = getoffset(logic_add);
    frame = getframe_fifo(logic_add, page, &page_fault_count, &tlb_hit_count);

    physical_add = frame * FRAME_SIZE + offset;
    int val = (int)(main_mem_fifo[physical_add]);

    // update tlb hit count and page fault count every 200 accesses
    if (access_count > 0 && access_count%200==0){
      tlbh2[(access_count/200)-1] = tlb_hit_count;
      pfc2[(access_count/200)-1] = page_fault_count;
      count2[(access_count/200)-1] = access_count;
    }
    
    printf("logical: %5u (page: %3u, offset: %3u) ---> physical: %5u -> value: %d-- passed\n", 
            logic_add, page, offset, physical_add, val);
    if (access_count % 5 == 0) { printf("\n"); }

    assert(value == val);
  }
  fclose(fcorr);
  fclose(fadd);
  fclose(fsto);

  printf("ALL read memory value assertions PASSED!\n");
  printf("\n\t\t...Part 2 done.\n");
}