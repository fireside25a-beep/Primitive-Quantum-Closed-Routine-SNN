#include "qmem.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t state=0x9e3779b9u;
static uint32_t next(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
int main(void){
 for(unsigned k=0;k<1000;k++){
  struct qmem_row row={0},decoded={0};
  for(int j=0;j<10;j++){row.x[j]=next()&31u;row.z[j]=next()&31u;row.phase[j]=(uint8_t)(next()&3u);}
  unsigned char record[108],addr[32];uint64_t id=0x0000000100000000ULL+k,got=0;
  if(qmem_encode(id,&row,record)||qmem_decode(record,&got,&decoded)||got!=id||
     memcmp(decoded.x,row.x,sizeof(row.x))||memcmp(decoded.z,row.z,sizeof(row.z))||
     memcmp(decoded.phase,row.phase,sizeof(row.phase)))return 11;
  qmem_codeword_hash(&row,addr);
  char a[217],h[65];qmem_hex(record,108,a);qmem_hex(addr,32,h);
  printf("%u %s %s\n",k,a,h);
  record[17]^=1;
  if(qmem_decode(record,&got,&decoded)==0)return 12;
 }
 return 0;
}
