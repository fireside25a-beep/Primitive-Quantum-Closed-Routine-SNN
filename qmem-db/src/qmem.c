#include "qmem.h"
#include <string.h>
#include <openssl/sha.h>
static void le32(unsigned char *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(i*8));}
static void le64(unsigned char *p,uint64_t v){for(unsigned i=0;i<8;i++)p[i]=(unsigned char)(v>>(i*8));}
static uint32_t get32(const unsigned char *p){uint32_t v=0;for(unsigned i=0;i<4;i++)v|=(uint32_t)p[i]<<(i*8);return v;}
static uint64_t get64(const unsigned char *p){uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)p[i]<<(i*8);return v;}
uint32_t qmem_crc(const void *p,size_t n){const unsigned char *b=p;uint32_t crc=~(uint32_t)0;for(size_t i=0;i<n;i++){crc^=b[i];for(unsigned j=0;j<8;j++)crc=(crc>>1)^(0xEDB88320u & (0u-(crc&1u)));}return ~crc;}
void qmem_hash(const void *p,size_t n,unsigned char out[32]){SHA256((const unsigned char*)p,n,out);}
static int valid(const struct qmem_row *r){if(!r)return 0;for(int i=0;i<10;i++)if(r->x[i]>31||r->z[i]>31||(r->phase[i]&~3u))return 0;return 1;}
void qmem_codeword_hash(const struct qmem_row *r,unsigned char out[32]){
    unsigned char b[sizeof("QMEM_CODEWORD_V1")-1+90];size_t n=0;
    memcpy(b,"QMEM_CODEWORD_V1",sizeof("QMEM_CODEWORD_V1")-1);n+=sizeof("QMEM_CODEWORD_V1")-1;
    for(int i=0;i<10;i++,n+=4)le32(b+n,r->x[i]);
    for(int i=0;i<10;i++,n+=4)le32(b+n,r->z[i]);
    for(int i=0;i<10;i++)b[n++]=r->phase[i];
    qmem_hash(b,n,out);
}
int qmem_encode(uint64_t id,const struct qmem_row *r,unsigned char out[108]){
    if(!valid(r)||!out)return -1;
    memset(out,0,108);memcpy(out,"QMCW",4);out[4]=1;le64(out+5,id);
    for(int i=0;i<10;i++){le32(out+13+4*i,r->x[i]);le32(out+53+4*i,r->z[i]);out[93+i]=r->phase[i];}
    le32(out+104,qmem_crc(out,104));return 0;
}
int qmem_decode(const unsigned char in[108],uint64_t *id,struct qmem_row *r){
    if(!in||!r||memcmp(in,"QMCW",4)||in[4]!=1||in[103]!=0||get32(in+104)!=qmem_crc(in,104))return -1;
    for(int i=0;i<10;i++){r->x[i]=get32(in+13+4*i);r->z[i]=get32(in+53+4*i);r->phase[i]=in[93+i];}
    if(!valid(r))return -1;
    if(id)*id=get64(in+5);
    return 0;
}
void qmem_hex(const unsigned char *p,size_t n,char *out){static const char h[]="0123456789abcdef";for(size_t i=0;i<n;i++){out[2*i]=h[p[i]>>4];out[2*i+1]=h[p[i]&15];}out[2*n]=0;}
int qmem_unhex(const char *p,size_t n,unsigned char *out){if(!p||!out||n>SIZE_MAX/2||strlen(p)!=n*2)return -1;for(size_t i=0;i<n;i++){unsigned v=0;for(int j=0;j<2;j++){unsigned char c=(unsigned char)p[2*i+j];if(c>='0'&&c<='9')v=16*v+c-'0';else if(c>='a'&&c<='f')v=16*v+c-'a'+10;else return -1;}out[i]=(unsigned char)v;}return p[n*2]? -1:0;}
