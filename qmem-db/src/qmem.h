#ifndef QMEM_H
#define QMEM_H
#include <stdint.h>
#include <stddef.h>
#define QMEM_RECORD_SIZE 108u
struct qmem_row { uint32_t x[10], z[10]; uint8_t phase[10]; };
uint32_t qmem_crc(const void *p,size_t n);
int qmem_encode(uint64_t id,const struct qmem_row *row,unsigned char out[108]);
int qmem_decode(const unsigned char in[108],uint64_t *id,struct qmem_row *row);
void qmem_hash(const void *p,size_t n,unsigned char out[32]);
void qmem_codeword_hash(const struct qmem_row *row,unsigned char out[32]);
void qmem_hex(const unsigned char *p,size_t n,char *out);
int qmem_unhex(const char *p,size_t n,unsigned char *out);
#endif
