#define _POSIX_C_SOURCE 200809L
#include "qmem.h"
#include "primitiveq.h"
#include <openssl/sha.h>
#include <sqlite3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* This executable is explicitly the development SQLite backend, NOT LMDB.
 * All source, trace and receipts use real 0.4.8 data. See README status. */
#define MAX_TEXT (16u*1024u*1024u)
static void fail(const char *msg){fprintf(stderr,"QMEM_DEV_REFUSED: %s\n",msg);exit(3);}
static void sqlok(int result,sqlite3 *db){if(result!=SQLITE_OK&&result!=SQLITE_DONE&&result!=SQLITE_ROW){fprintf(stderr,"SQLite error: %s\n",sqlite3_errmsg(db));exit(2);}}
static void command(sqlite3 *db,const char *sql){char *msg=NULL;int rc=sqlite3_exec(db,sql,NULL,NULL,&msg);if(rc!=SQLITE_OK){fprintf(stderr,"SQLite: %s\n",msg?msg:"unknown");sqlite3_free(msg);exit(2);}}
static sqlite3 *connect_db(const char *path,int initialize,int writable){
 if(!path||!*path||!strcmp(path,":memory:")||!strncmp(path,"file:",5))fail("invalid persistent database path");
 struct stat st;
 if(lstat(path,&st)==0){
  if(initialize)fail("init refuses an existing database");
  if(!S_ISREG(st.st_mode)||st.st_nlink!=1)fail("refuse symlink, special file or hard-linked database");
 }else if(errno!=ENOENT)fail("database path cannot be inspected");
 else if(!initialize)fail("database does not exist; use init first");
 sqlite3 *db=NULL;int flags=(initialize?SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE:(writable?SQLITE_OPEN_READWRITE:SQLITE_OPEN_READONLY));
 #ifdef SQLITE_OPEN_NOFOLLOW
 flags |= SQLITE_OPEN_NOFOLLOW;
#endif
 if(sqlite3_open_v2(path,&db,flags,NULL)!=SQLITE_OK)fail("cannot open storage");
 command(db,"PRAGMA foreign_keys=ON;PRAGMA trusted_schema=OFF;PRAGMA busy_timeout=0;");
 if(initialize)command(db,"CREATE TABLE IF NOT EXISTS rows(tick INTEGER NOT NULL,id INTEGER NOT NULL,hash BLOB NOT NULL CHECK(length(hash)=32),record BLOB NOT NULL CHECK(length(record)=108),PRIMARY KEY(tick,id));"
       "CREATE TABLE IF NOT EXISTS content(hash BLOB PRIMARY KEY CHECK(length(hash)=32),norm BLOB NOT NULL CHECK(length(norm)=90));"
       "CREATE TABLE IF NOT EXISTS events(tick INTEGER PRIMARY KEY,line TEXT NOT NULL,rawhash BLOB NOT NULL CHECK(length(rawhash)=32));"
       "CREATE TABLE IF NOT EXISTS nm_receipt(id INTEGER PRIMARY KEY CHECK(id=1),source TEXT NOT NULL,hash BLOB NOT NULL CHECK(length(hash)=32));"
       "CREATE TABLE IF NOT EXISTS attempts(id INTEGER PRIMARY KEY AUTOINCREMENT,op TEXT NOT NULL,status TEXT NOT NULL,reason INTEGER NOT NULL,before_root BLOB NOT NULL,after_root BLOB NOT NULL,payload_hash BLOB NOT NULL,receipt TEXT NOT NULL);");
 return db;
}
static sqlite3_stmt *prepare(sqlite3 *db,const char *sql){sqlite3_stmt *s=NULL;sqlok(sqlite3_prepare_v2(db,sql,-1,&s,NULL),db);return s;}
static void put32(unsigned char *b,uint32_t v){for(int i=0;i<4;i++)b[i]=(unsigned char)(v>>(8*i));}
static void put64(unsigned char *b,uint64_t v){for(int i=0;i<8;i++)b[i]=(unsigned char)(v>>(8*i));}
static uint32_t read32(const unsigned char *p){uint32_t x=0;for(unsigned i=0;i<4;i++)x|=(uint32_t)p[i]<<(8*i);return x;}
static uint64_t unsigned_decimal(const char *s){if(!s||!*s||*s=='-'||*s=='+')fail("invalid unsigned value");errno=0;char *end=NULL;uintmax_t v=strtoumax(s,&end,10);if(errno||!end||*end||v>UINT64_MAX)fail("invalid unsigned value");return (uint64_t)v;}
static void blob(sqlite3_stmt *s,int col,const void *ptr,int length){sqlok(sqlite3_bind_blob(s,col,ptr,length,SQLITE_TRANSIENT),sqlite3_db_handle(s));}
static void textbind(sqlite3_stmt *s,int col,const char *v){sqlok(sqlite3_bind_text(s,col,v,-1,SQLITE_TRANSIENT),sqlite3_db_handle(s));}
static void store_hash(SHA256_CTX *sha,const void *p,size_t n){if(n && SHA256_Update(sha,p,n)!=1)fail("SHA update");}
static void hash_root(sqlite3 *db,unsigned char root[32]){
 SHA256_CTX sha;if(SHA256_Init(&sha)!=1)fail("SHA init");
 static const char tag[]="QMEM_SQLITE_DEV_LOGICAL_V1\n";store_hash(&sha,tag,sizeof(tag)-1);
 sqlite3_stmt *s=prepare(db,"SELECT tick,id,hash,record FROM rows ORDER BY tick,id");
 int rc;unsigned char nums[16];while((rc=sqlite3_step(s))==SQLITE_ROW){
  int h=sqlite3_column_bytes(s,2),r=sqlite3_column_bytes(s,3);
  if(h!=32||r!=108)fail("corrupt stored row length");
  unsigned char sep='C';store_hash(&sha,&sep,1);put64(nums,(uint64_t)sqlite3_column_int64(s,0));put64(nums+8,(uint64_t)sqlite3_column_int64(s,1));store_hash(&sha,nums,16);
  store_hash(&sha,sqlite3_column_blob(s,2),32);store_hash(&sha,sqlite3_column_blob(s,3),108);
 }
 if(rc!=SQLITE_DONE)fail("root scan failed");sqlite3_finalize(s);
 s=prepare(db,"SELECT tick,line FROM events ORDER BY tick");
 while((rc=sqlite3_step(s))==SQLITE_ROW){
  const unsigned char *line=sqlite3_column_text(s,1);int len=sqlite3_column_bytes(s,1);
  unsigned char sep='E';store_hash(&sha,&sep,1);put64(nums,(uint64_t)sqlite3_column_int64(s,0));store_hash(&sha,nums,8);
  put64(nums,(uint64_t)len);store_hash(&sha,nums,8);store_hash(&sha,line,(size_t)len);
 }
 if(rc!=SQLITE_DONE)fail("root event scan failed");sqlite3_finalize(s);
 SHA256_Final(root,&sha);
}
static void root_hex(sqlite3 *db,char result[65]){unsigned char root[32];hash_root(db,root);qmem_hex(root,32,result);}
static char *load(const char *path,size_t *len){
 int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
 if(fd<0)fail("cannot open safe input file");
 struct stat st;
 if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_nlink!=1||st.st_size<0||st.st_size>MAX_TEXT)
  fail("input must be a unique regular file <=16 MiB");
 FILE *fp=fdopen(fd,"rb");if(!fp)fail("cannot attach input stream");
 size_t sz=(size_t)st.st_size;
 char *buf=malloc(sz+1);if(!buf)fail("out of memory");
 if(fread(buf,1,sz,fp)!=sz)fail("short input read");
 if(fgetc(fp)!=EOF)fail("input changed during read");
 if(fclose(fp))fail("close failed");
 if(memchr(buf,0,sz))fail("embedded NUL in text");
 buf[sz]=0;*len=sz;return buf;
}
static int valid_hex64(const char *p){for(int i=0;i<64;i++)if(!((p[i]>='0'&&p[i]<='9')||(p[i]>='a'&&p[i]<='f')))return 0;return p[64]==0;}
static char *field(char *line,const char *name){
 size_t n=strlen(name);char *p=line;
 while(p&&*p){if((p==line||p[-1]==';')&&!strncmp(p,name,n)&&p[n]=='=')return p+n+1;
  p=strchr(p,';');if(p)p++;
 }return NULL;
}
static void token(char *out,size_t cap,const char *value){if(!value)fail("missing tick field");size_t n=strcspn(value,";\n");if(!n||n>=cap)fail("invalid tick field length");memcpy(out,value,n);out[n]=0;}
static void text_hash(char *hex,const char *text,size_t len){unsigned char h[32];qmem_hash(text,len,h);qmem_hex(h,32,hex);}
/* Canonical source digest is SHA256 of everything preceding receipt_root=. */
static size_t verify_nm_receipt(char *input,size_t n){
 static const char header[]="NEUROMANTLE_COUPLED_MEMORY_QECC5_V1\n";
 if(n<sizeof(header)-1||memcmp(input,header,sizeof(header)-1))fail("receipt is not certified five-code coupled memory");
 char *footer=strstr(input,"\nreceipt_root=");if(!footer)fail("missing upstream receipt footer");
 char *value=footer+strlen("\nreceipt_root=");if(!valid_hex64(value)&&!(strlen(value)==65&&value[64]=='\n'))fail("invalid upstream footer");
 if(strlen(value)!=65||value[64]!='\n'||value[65]!=0)fail("receipt footer not terminal");
 char digest[65];text_hash(digest,input,(size_t)(footer-input)+1);
 if(memcmp(digest,value,64))fail("upstream receipt hash mismatch");
 return (size_t)(footer-input)+1;
}
static void check_canonical(const struct qmem_row *r){
 pq_state zero,one;if(pq_init_five_code(&zero))fail("native five code failed");one=zero;
 for(unsigned i=0;i<5;i++)if(pq_pauli(&one,i,'X'))fail("logical X failed");
 unsigned bit=r->phase[9]>>1;
 for(int j=0;j<10;j++){
  pq_row x=bit?one.rows[j]:zero.rows[j];
  if(x.x!=r->x[j]||x.z!=r->z[j]||x.phase!=r->phase[j])fail("noncanonical committed five-code tableau");
 }
}
static void cell_record(const char *hex,uint64_t id,unsigned char rec[108],unsigned char addr[32],unsigned char norm[90]){
 if(strlen(hex)!=180||qmem_unhex(hex,90,norm))fail("invalid tableau hex");
 struct qmem_row row={0};
 for(int j=0;j<10;j++){row.x[j]=read32(norm+9*j);row.z[j]=read32(norm+9*j+4);row.phase[j]=norm[9*j+8];}
 if(qmem_encode(id,&row,rec))fail("tableau range/shape invalid");
 uint64_t parsed=0;struct qmem_row mirror;
 if(qmem_decode(rec,&parsed,&mirror)||parsed!=id||memcmp(row.x,mirror.x,sizeof(row.x))||memcmp(row.z,mirror.z,sizeof(row.z))||memcmp(row.phase,mirror.phase,sizeof(row.phase)))fail("codeword frame round-trip failed");
 check_canonical(&row);qmem_codeword_hash(&row,addr);
}
static void add_row(sqlite3 *db,uint64_t tick,uint64_t id,const unsigned char hash[32],const unsigned char record[108],const unsigned char norm[90]){
 if(id>INT64_MAX||tick>INT64_MAX)fail("SQLite index exceeds signed 64-bit");
 sqlite3_stmt *s=prepare(db,"INSERT INTO content(hash,norm) VALUES(?,?) ON CONFLICT(hash) DO NOTHING");
 blob(s,1,hash,32);blob(s,2,norm,90);sqlok(sqlite3_step(s),db);sqlite3_finalize(s);
 s=prepare(db,"SELECT norm FROM content WHERE hash=?");blob(s,1,hash,32);
 if(sqlite3_step(s)!=SQLITE_ROW||sqlite3_column_bytes(s,0)!=90||memcmp(sqlite3_column_blob(s,0),norm,90))fail("codeword digest collision or corruption");sqlite3_finalize(s);
 s=prepare(db,"INSERT INTO rows(tick,id,hash,record) VALUES(?,?,?,?) ON CONFLICT(tick,id) DO NOTHING");
 sqlok(sqlite3_bind_int64(s,1,(sqlite3_int64)tick),db);sqlok(sqlite3_bind_int64(s,2,(sqlite3_int64)id),db);blob(s,3,hash,32);blob(s,4,record,108);
 sqlok(sqlite3_step(s),db);sqlite3_finalize(s);
 s=prepare(db,"SELECT hash,record FROM rows WHERE tick=? AND id=?");
 sqlok(sqlite3_bind_int64(s,1,(sqlite3_int64)tick),db);sqlok(sqlite3_bind_int64(s,2,(sqlite3_int64)id),db);
 if(sqlite3_step(s)!=SQLITE_ROW||sqlite3_column_bytes(s,0)!=32||sqlite3_column_bytes(s,1)!=108||
    memcmp(sqlite3_column_blob(s,0),hash,32)||memcmp(sqlite3_column_blob(s,1),record,108))fail("cell id conflict");
 sqlite3_finalize(s);
}
static void add_event(sqlite3 *db,uint64_t tick,const char *line){
 unsigned char h[32];qmem_hash(line,strlen(line),h);
 sqlite3_stmt *s=prepare(db,"INSERT INTO events(tick,line,rawhash) VALUES(?,?,?) ON CONFLICT(tick) DO NOTHING");
 sqlok(sqlite3_bind_int64(s,1,(sqlite3_int64)tick),db);textbind(s,2,line);blob(s,3,h,32);sqlok(sqlite3_step(s),db);sqlite3_finalize(s);
 s=prepare(db,"SELECT line,rawhash FROM events WHERE tick=?");sqlok(sqlite3_bind_int64(s,1,(sqlite3_int64)tick),db);
 if(sqlite3_step(s)!=SQLITE_ROW||strcmp((const char*)sqlite3_column_text(s,0),line)||sqlite3_column_bytes(s,1)!=32||memcmp(sqlite3_column_blob(s,1),h,32))fail("event tick conflict");sqlite3_finalize(s);
}
static char *receipt(sqlite3 *db,const char *op,const char *status,int reason,const unsigned char *payload,size_t length,const char before[65],const char after[65]){
 sqlite3_stmt *s=prepare(db,"SELECT COALESCE(MAX(id),0)+1 FROM attempts");if(sqlite3_step(s)!=SQLITE_ROW)fail("attempt id lookup");
 uint64_t id=(uint64_t)sqlite3_column_int64(s,0);sqlite3_finalize(s);if(id==0||id>INT64_MAX)fail("attempt overflow");
 char digest[65];text_hash(digest,(const char*)payload,length);
 char frame[1024];int n=snprintf(frame,sizeof(frame),"QMEM_RECEIPT_V1\ntx_id=%" PRIu64 "\nop=%s\nstatus=%s\nreason=%d\ngeneration=0\nbefore_root=%s\nafter_root=%s\npayload_sha256=%s\n",id,op,status,reason,before,after,digest);
 if(n<0||(size_t)n>=sizeof(frame))fail("receipt buffer exceeded");
 char root[65];text_hash(root,frame,(size_t)n);
 char full[1150];int total=snprintf(full,sizeof(full),"%sreceipt_root=%s\n",frame,root);if(total<0||(size_t)total>=sizeof(full))fail("receipt output exceeded");
 s=prepare(db,"INSERT INTO attempts(id,op,status,reason,before_root,after_root,payload_hash,receipt) VALUES(?,?,?,?,?,?,?,?)");
 sqlok(sqlite3_bind_int64(s,1,(sqlite3_int64)id),db);textbind(s,2,op);textbind(s,3,status);
 sqlok(sqlite3_bind_int(s,4,reason),db);blob(s,5,before,64);blob(s,6,after,64);blob(s,7,digest,64);textbind(s,8,full);sqlok(sqlite3_step(s),db);sqlite3_finalize(s);
 char *saved=strdup(full);if(!saved)fail("receipt allocation failed");return saved;
}
static void import_cells(sqlite3 *db,const char *qmxfile,const char *receiptfile){
 size_t rlen=0,xlen=0;char *r=load(receiptfile,&rlen),*x=load(qmxfile,&xlen);(void)xlen;
 verify_nm_receipt(r,rlen);
 if(strncmp(x,"QMEM_EXPORT_V1\n",strlen("QMEM_EXPORT_V1\n")))fail("unsupported QMX header");
 char oldroot[65];root_hex(db,oldroot);
 command(db,"BEGIN IMMEDIATE");
 sqlite3_stmt *s=prepare(db,"INSERT INTO nm_receipt(id,source,hash) VALUES(1,?,?) ON CONFLICT(id) DO NOTHING");
 unsigned char rhash[32];qmem_hash(r,rlen,rhash);textbind(s,1,r);blob(s,2,rhash,32);sqlok(sqlite3_step(s),db);sqlite3_finalize(s);
 s=prepare(db,"SELECT hash,source FROM nm_receipt WHERE id=1");if(sqlite3_step(s)!=SQLITE_ROW||sqlite3_column_bytes(s,0)!=32||memcmp(sqlite3_column_blob(s,0),rhash,32)||strcmp((const char*)sqlite3_column_text(s,1),r))fail("source receipt conflict");sqlite3_finalize(s);
 char *xp=x+strlen("QMEM_EXPORT_V1\n");char *rp=r;uint64_t expectedtick=0;size_t count=0;
 char prevstate[65]={0},prevmem[65]={0};
 while(*xp){
  char *line_end=strchr(xp,'\n');if(!line_end)fail("unterminated QMX line");*line_end=0;
  if(strncmp(xp,"tick=",5))fail("expected tick snapshot line");
  char *rootval=field(xp,"root");char hxroot[65];token(hxroot,sizeof(hxroot),rootval);if(!valid_hex64(hxroot))fail("invalid QMX root");
  char tickword[32];token(tickword,sizeof(tickword),field(xp,"tick"));uint64_t tick=unsigned_decimal(tickword);if(tick!=expectedtick++)fail("nonconsecutive tick in QMX");
  xp=line_end+1;
  unsigned char nbytes[4];unsigned char digest[32];SHA256_CTX sha;SHA256_Init(&sha);
  static const char tag[]="NM_QMEMORY_TABLEAU_V1";store_hash(&sha,tag,sizeof(tag)-1);
  /* edges count is validated once rows are counted; store current cells in digest after 4-byte count. */
  SHA256_CTX aftertag=sha;
  unsigned char *framebuf=NULL;size_t frame_len=0,frame_cap=0;uint64_t id=0;
  for(;;){
   if(strncmp(xp,"cell=",5))break;
   char *end=strchr(xp,'\n');if(!end)fail("unterminated cell record");*end=0;
   char number[32];token(number,sizeof(number),field(xp,"cell"));if(unsigned_decimal(number)!=++id)fail("nonconsecutive cell id");
   const char *src=field(xp,"tableau");if(!src)fail("missing tableau");
   unsigned char rec[108],hash[32],norm[90];cell_record(src,id,rec,hash,norm);
   if(frame_len+91>frame_cap){size_t newer=frame_cap?frame_cap*2:2912;if(newer<frame_len+91)newer=frame_len+91;unsigned char *tmp=realloc(framebuf,newer);if(!tmp)fail("OOM");framebuf=tmp;frame_cap=newer;}
   framebuf[frame_len++]=5;memcpy(framebuf+frame_len,norm,90);frame_len+=90;
   add_row(db,tick,id,hash,rec,norm);
   xp=end+1;
  }
  if(!id||id%32||id>8192||id>UINT32_MAX*32ULL)fail("QMX cell count invalid");
  if(count&&count!=(size_t)id)fail("QMX edge count changed");count=(size_t)id;
  sha=aftertag;put32(nbytes,(uint32_t)(id/32));store_hash(&sha,nbytes,4);store_hash(&sha,framebuf,frame_len);SHA256_Final(digest,&sha);free(framebuf);
  char actualroot[65];qmem_hex(digest,32,actualroot);
  if(strcmp(actualroot,hxroot))fail("QMX tableau root mismatch");
  while(*rp&&strncmp(rp,"tick=",5)){char *end=strchr(rp,'\n');if(!end)fail("upstream receipt missing ticks");rp=end+1;}
  if(!*rp)fail("missing corresponding upstream tick");
  char *end=strchr(rp,'\n');if(!end)fail("unterminated upstream tick");*end=0;
  char tm[32],qm[512],bm[65],bs[65],ss[65],eh[65];
  token(tm,sizeof(tm),field(rp,"tick"));token(qm,sizeof(qm),field(rp,"qmemory"));
  token(bm,sizeof(bm),field(rp,"qmemory_before"));token(bs,sizeof(bs),field(rp,"before"));token(ss,sizeof(ss),field(rp,"state"));token(eh,sizeof(eh),field(rp,"event"));
  if(unsigned_decimal(tm)!=tick)fail("upstream tick differs");
  if(strlen(qm)<65||qm[64]!=':'||strncmp(qm,actualroot,64))fail("receipt/QMX root disagreement");
  if(!valid_hex64(bm)||!valid_hex64(bs)||!valid_hex64(ss)||!valid_hex64(eh))fail("invalid 64-byte hex upstream field");
  if(tick&&(strcmp(bs,prevstate)||strcmp(bm,prevmem)))fail("upstream state/memory chain mismatch");
  strcpy(prevstate,ss);strcpy(prevmem,actualroot);
  add_event(db,tick,rp);rp=end+1;
 }
 if(expectedtick==0)fail("empty QMX transaction");
 while(*rp&&strncmp(rp,"tick=",5)){char *end=strchr(rp,'\n');if(!end)break;rp=end+1;}
 if(*rp&&strncmp(rp,"tick=",5)==0)fail("receipt has extra tick");
 char newroot[65];root_hex(db,newroot);
 char *committed_receipt=receipt(db,"event","admit",0,rhash,32,oldroot,newroot);
 command(db,"COMMIT");
 fputs(committed_receipt,stdout);free(committed_receipt);
 fprintf(stderr,"QMEM_SQLITE_DEV_IMPORT_PASS ticks=%" PRIu64 " cells_per_tick=%zu root=%s\n",expectedtick,count,newroot);
 free(r);free(x);
}
static void verify_attempts(sqlite3 *db,const char final_root[65]){
 sqlite3_stmt *s=prepare(db,"SELECT id,op,status,reason,before_root,after_root,payload_hash,receipt FROM attempts ORDER BY id");
 int rc;uint64_t expected=1;char previous[65];
 unsigned char empty[32];static const char tag[]="QMEM_SQLITE_DEV_LOGICAL_V1\n";
 qmem_hash(tag,sizeof(tag)-1,empty);qmem_hex(empty,32,previous);
 while((rc=sqlite3_step(s))==SQLITE_ROW){
  uint64_t id=(uint64_t)sqlite3_column_int64(s,0);
  if(id!=expected++)fail("attempt id gap");
  const char *op=(const char*)sqlite3_column_text(s,1),*status=(const char*)sqlite3_column_text(s,2);
  int reason=sqlite3_column_int(s,3);
  if(!op||!status||strcmp(status,"admit")||strcmp(op,"event")||reason!=0)fail("invalid stored attempt metadata");
  if(sqlite3_column_bytes(s,4)!=64||sqlite3_column_bytes(s,5)!=64||sqlite3_column_bytes(s,6)!=64)fail("attempt root digest width invalid");
  char before[65],after[65],payload[65];
  memcpy(before,sqlite3_column_blob(s,4),64);before[64]=0;
  memcpy(after,sqlite3_column_blob(s,5),64);after[64]=0;
  memcpy(payload,sqlite3_column_blob(s,6),64);payload[64]=0;
  if(!valid_hex64(before)||!valid_hex64(after)||!valid_hex64(payload)||strcmp(previous,before))fail("broken attempt chain");
  char data[1024];int n=snprintf(data,sizeof(data),"QMEM_RECEIPT_V1\ntx_id=%" PRIu64 "\nop=%s\nstatus=%s\nreason=%d\ngeneration=0\nbefore_root=%s\nafter_root=%s\npayload_sha256=%s\n",id,op,status,reason,before,after,payload);
  if(n<=0||(size_t)n>=sizeof(data))fail("stored receipt frame overflow");
  char digest[65];text_hash(digest,data,(size_t)n);
  char canonical[1150];int len=snprintf(canonical,sizeof(canonical),"%sreceipt_root=%s\n",data,digest);
  if(len<=0||(size_t)len>=sizeof(canonical))fail("stored receipt overflow");
  const char *actual=(const char*)sqlite3_column_text(s,7);
  if(!actual||sqlite3_column_bytes(s,7)!=len||memcmp(actual,canonical,(size_t)len))fail("stored receipt differs from canonical frame");
  strcpy(previous,after);
 }
 if(rc!=SQLITE_DONE)fail("attempt scan failed");sqlite3_finalize(s);
 if(expected==1||strcmp(previous,final_root))fail("final attempt root differs from logical root");
}
static void verify_db(sqlite3 *db){
 sqlite3_stmt *s=prepare(db,"SELECT tick,id,hash,record FROM rows ORDER BY tick,id");int rc;uint64_t currenttick=UINT64_MAX,lastid=0,ticks=0,count=0;SHA256_CTX sha;
 unsigned char head[4],digest[32];char framehex[65],nmroot[65];
 while((rc=sqlite3_step(s))==SQLITE_ROW){
  uint64_t tick=(uint64_t)sqlite3_column_int64(s,0),id=(uint64_t)sqlite3_column_int64(s,1);
  if(tick!=currenttick){
   if(currenttick!=UINT64_MAX){if(lastid%32)fail("stored cell count invalid");SHA256_Final(digest,&sha);qmem_hex(digest,32,framehex);
    sqlite3_stmt *ev=prepare(db,"SELECT line FROM events WHERE tick=?");sqlite3_bind_int64(ev,1,(sqlite3_int64)currenttick);
    if(sqlite3_step(ev)!=SQLITE_ROW)fail("missing stored event");char *line=strdup((const char*)sqlite3_column_text(ev,0));if(!line)fail("out of memory");
    char val[512];token(val,sizeof(val),field(line,"qmemory"));if(strncmp(val,framehex,64)||val[64]!=':')fail("stored world memory mismatch");free(line);sqlite3_finalize(ev);ticks++;
   }
   currenttick=tick;lastid=0;
   sqlite3_stmt *cs=prepare(db,"SELECT COUNT(*) FROM rows WHERE tick=?");sqlite3_bind_int64(cs,1,(sqlite3_int64)tick);
   if(sqlite3_step(cs)!=SQLITE_ROW)fail("count rows");uint64_t k=(uint64_t)sqlite3_column_int64(cs,0);sqlite3_finalize(cs);
   if(!k||k%32||k>8192)fail("invalid stored edge count");
   SHA256_Init(&sha);static const char tag[]="NM_QMEMORY_TABLEAU_V1";store_hash(&sha,tag,sizeof(tag)-1);
   put32(head,(uint32_t)(k/32));store_hash(&sha,head,4);
  }
  if(id!=lastid+1)fail("stored cell id gap");lastid=id;count++;
  if(sqlite3_column_bytes(s,2)!=32||sqlite3_column_bytes(s,3)!=108)fail("invalid stored cell length");
  unsigned char hash[32],rec[108];memcpy(hash,sqlite3_column_blob(s,2),32);memcpy(rec,sqlite3_column_blob(s,3),108);
  uint64_t did;struct qmem_row row;
  if(qmem_decode(rec,&did,&row)||did!=id)fail("stored record CRC or shape failure");unsigned char recomputed[32];qmem_codeword_hash(&row,recomputed);
  if(memcmp(hash,recomputed,32))fail("stored codeword hash mismatch");check_canonical(&row);
  sqlite3_stmt *lookup=prepare(db,"SELECT norm FROM content WHERE hash=?");blob(lookup,1,hash,32);
  if(sqlite3_step(lookup)!=SQLITE_ROW||sqlite3_column_bytes(lookup,0)!=90)fail("missing content index");
  const unsigned char *body=(const unsigned char*)sqlite3_column_blob(lookup,0);
  for(int j=0;j<10;j++)if(read32(body+9*j)!=row.x[j]||read32(body+9*j+4)!=row.z[j]||body[9*j+8]!=row.phase[j])fail("row/content index disagrees");
  sqlite3_finalize(lookup);
  unsigned char unit=5;store_hash(&sha,&unit,1);
  unsigned char norm[90];for(int j=0;j<10;j++){put32(norm+j*9,row.x[j]);put32(norm+j*9+4,row.z[j]);norm[j*9+8]=row.phase[j];}
  store_hash(&sha,norm,90);
  sqlite3_stmt *cs=prepare(db,"SELECT norm FROM content WHERE hash=?");
  blob(cs,1,hash,32);
  if(sqlite3_step(cs)!=SQLITE_ROW||sqlite3_column_bytes(cs,0)!=90||memcmp(sqlite3_column_blob(cs,0),norm,90))fail("content-address index corruption");
  sqlite3_finalize(cs);
 }
 if(rc!=SQLITE_DONE)fail("failed DB verification scan");sqlite3_finalize(s);
 if(currenttick!=UINT64_MAX){
  SHA256_Final(digest,&sha);qmem_hex(digest,32,nmroot);
  sqlite3_stmt *ev=prepare(db,"SELECT line FROM events WHERE tick=?");sqlite3_bind_int64(ev,1,(sqlite3_int64)currenttick);
  if(sqlite3_step(ev)!=SQLITE_ROW)fail("final event missing");char *line=strdup((const char*)sqlite3_column_text(ev,0));if(!line)fail("OOM");char val[512];token(val,sizeof(val),field(line,"qmemory"));if(strncmp(val,nmroot,64)||val[64]!=':')fail("final memory mismatch");free(line);sqlite3_finalize(ev);ticks++;
 }
 s=prepare(db,"SELECT source,hash FROM nm_receipt WHERE id=1");if(sqlite3_step(s)!=SQLITE_ROW)fail("stored upstream receipt missing");
 size_t n=(size_t)sqlite3_column_bytes(s,0);char *src=malloc(n+1);if(!src)fail("OOM");memcpy(src,sqlite3_column_text(s,0),n);src[n]=0;verify_nm_receipt(src,n);
 unsigned char upstream_digest[32];qmem_hash(src,n,upstream_digest);
 if(sqlite3_column_bytes(s,1)!=32||memcmp(sqlite3_column_blob(s,1),upstream_digest,32))fail("stored source digest mismatch");
 free(src);sqlite3_finalize(s);
 /* Stored side-indexes, events and attempt receipts are checked independently. */
 s=prepare(db,"SELECT hash,norm FROM content ORDER BY hash");
 while((rc=sqlite3_step(s))==SQLITE_ROW){
  if(sqlite3_column_bytes(s,0)!=32||sqlite3_column_bytes(s,1)!=90)fail("bad content length");
  const unsigned char *norm=sqlite3_column_blob(s,1),*actual=sqlite3_column_blob(s,0);
  struct qmem_row rw={0};for(int j=0;j<10;j++){rw.x[j]=read32(norm+9*j);rw.z[j]=read32(norm+9*j+4);rw.phase[j]=norm[j*9+8];}
  unsigned char addr[32];qmem_codeword_hash(&rw,addr);
  if(memcmp(actual,addr,32))fail("content hash mismatch");
  sqlite3_stmt *back=prepare(db,"SELECT COUNT(*) FROM rows WHERE hash=?");blob(back,1,addr,32);
  if(sqlite3_step(back)!=SQLITE_ROW||sqlite3_column_int64(back,0)==0)fail("orphan content");sqlite3_finalize(back);
 }
 if(rc!=SQLITE_DONE)fail("content table iteration");sqlite3_finalize(s);
 s=prepare(db,"SELECT tick,line,rawhash FROM events ORDER BY tick");
 uint64_t nexttick=0;while((rc=sqlite3_step(s))==SQLITE_ROW){
  if(sqlite3_column_int64(s,0)!=(sqlite3_int64)nexttick++)fail("event tick hole");
  int len=sqlite3_column_bytes(s,1);const char *line=(const char*)sqlite3_column_text(s,1);
  if(len<=0||len>65536||sqlite3_column_bytes(s,2)!=32)fail("event blob length");
  unsigned char h[32];qmem_hash(line,(size_t)len,h);
  if(memcmp(h,sqlite3_column_blob(s,2),32))fail("event blob sha mismatch");
 }
 if(rc!=SQLITE_DONE||nexttick!=ticks)fail("unexpected event count");sqlite3_finalize(s);
 s=prepare(db,"SELECT id,op,status,reason,before_root,after_root,payload_hash,receipt FROM attempts ORDER BY id");
 uint64_t expected_attempt=0;while((rc=sqlite3_step(s))==SQLITE_ROW){
  if(sqlite3_column_int64(s,0)!=(sqlite3_int64)++expected_attempt)fail("attempt id gap");
  if(expected_attempt>1){
   sqlite3_stmt *before=prepare(db,"SELECT after_root FROM attempts WHERE id=?");
   sqlok(sqlite3_bind_int64(before,1,(sqlite3_int64)(expected_attempt-1)),db);
   if(sqlite3_step(before)!=SQLITE_ROW||sqlite3_column_bytes(before,0)!=64||
     memcmp(sqlite3_column_blob(before,0),sqlite3_column_blob(s,4),64))fail("attempt root chain mismatch");
   sqlite3_finalize(before);
  }
  const char *a=(const char*)sqlite3_column_text(s,7);
  if(!a||sqlite3_column_bytes(s,4)!=64||sqlite3_column_bytes(s,5)!=64||sqlite3_column_bytes(s,6)!=64)fail("bad attempt frame");
  const char *footer=strstr(a,"receipt_root=");if(!footer||strlen(footer)!=strlen("receipt_root=")+65)fail("attempt footer invalid");
  char calc[65];text_hash(calc,a,(size_t)(footer-a));if(strncmp(calc,footer+13,64))fail("attempt receipt SHA mismatch");
  char canonical[1024];int n=snprintf(canonical,sizeof(canonical),"QMEM_RECEIPT_V1\ntx_id=%" PRIu64 "\nop=%s\nstatus=%s\nreason=%d\ngeneration=0\nbefore_root=%.*s\nafter_root=%.*s\npayload_sha256=%.*s\n",expected_attempt,sqlite3_column_text(s,1),sqlite3_column_text(s,2),sqlite3_column_int(s,3),64,(const char*)sqlite3_column_blob(s,4),64,(const char*)sqlite3_column_blob(s,5),64,(const char*)sqlite3_column_blob(s,6));
  if(n<0||(size_t)n>=sizeof(canonical)||(size_t)n!=(size_t)(footer-a)||memcmp(canonical,a,(size_t)n))fail("attempt fields differ");
 }
 if(rc!=SQLITE_DONE)fail("attempt scan error");sqlite3_finalize(s);
 if(expected_attempt){
  sqlite3_stmt *last=prepare(db,"SELECT after_root FROM attempts ORDER BY id DESC LIMIT 1");
  if(sqlite3_step(last)!=SQLITE_ROW||sqlite3_column_bytes(last,0)!=64)fail("missing last receipt root");
  char root_current[65];root_hex(db,root_current);
  if(memcmp(sqlite3_column_blob(last,0),root_current,64))fail("last receipt root does not match data");
  sqlite3_finalize(last);
 }
 char root[65];root_hex(db,root);verify_attempts(db,root);printf("QMEM_SQLITE_DEV_VERIFY_PASS ticks=%" PRIu64 " rows=%" PRIu64 " logical_root=%s\n",ticks,count,root);
}
static void show(sqlite3 *db,const char *tickstr,const char *idstr){
 uint64_t tick=unsigned_decimal(tickstr),id=unsigned_decimal(idstr);if(tick>INT64_MAX||id>INT64_MAX)fail("out-of-range key");
 sqlite3_stmt *s=prepare(db,"SELECT hash,record FROM rows WHERE tick=? AND id=?");sqlite3_bind_int64(s,1,(sqlite3_int64)tick);sqlite3_bind_int64(s,2,(sqlite3_int64)id);
 if(sqlite3_step(s)!=SQLITE_ROW)fail("cell not found");if(sqlite3_column_bytes(s,0)!=32||sqlite3_column_bytes(s,1)!=108)fail("corrupt record");
 const unsigned char *rec=sqlite3_column_blob(s,1);uint64_t storedid;struct qmem_row row;
 if(qmem_decode(rec,&storedid,&row)||storedid!=id)fail("corrupt record");
 char h[65],b[217];qmem_hex(sqlite3_column_blob(s,0),32,h);qmem_hex(rec,108,b);
 printf("tick=%" PRIu64 "\nid=%" PRIu64 "\ncodeword_hash=%s\nrecord=%s\nlogical_bit=%u\n",tick,id,h,b,row.phase[9]>>1);
 sqlite3_finalize(s);
}
static FILE *new_facts_file(const char *path){
 int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
 if(fd<0)fail("refuse existing or unsafe output file");
 FILE *f=fdopen(fd,"wb");if(!f){close(fd);fail("cannot attach stream to output");}
 return f;
}
static void export_facts(sqlite3 *db,const char *prefix){
 if(strchr(prefix,'\n')||strlen(prefix)>800)fail("invalid output prefix");
 char path[1024];if(snprintf(path,sizeof(path),"%s.cell.facts",prefix)>= (int)sizeof(path))fail("output too long");
 FILE *f=new_facts_file(path);
 sqlite3_stmt *s=prepare(db,"SELECT tick,id,record FROM rows ORDER BY tick,id");int rc;
 while((rc=sqlite3_step(s))==SQLITE_ROW){
  uint64_t tick=(uint64_t)sqlite3_column_int64(s,0),id=(uint64_t)sqlite3_column_int64(s,1);uint64_t rid;struct qmem_row row;
  if(sqlite3_column_bytes(s,2)!=108||qmem_decode(sqlite3_column_blob(s,2),&rid,&row)||rid!=id)fail("bad record for facts");
  for(int j=0;j<10;j++)if(fprintf(f,"%" PRIu64 "\t%" PRIu64 "\t%d\t%u\t%u\t%u\n",tick,id,j,row.x[j],row.z[j],row.phase[j])<0)fail("failed writing facts");
 }
 if(rc!=SQLITE_DONE)fail("facts scan error");sqlite3_finalize(s);if(fclose(f))fail("facts flush error");
 if(snprintf(path,sizeof(path),"%s.event.facts",prefix)>= (int)sizeof(path))fail("output too long");
 f=new_facts_file(path);
 s=prepare(db,"SELECT tick,line,rawhash FROM events ORDER BY tick");
 while((rc=sqlite3_step(s))==SQLITE_ROW){
  const unsigned char *txt=sqlite3_column_text(s,1);unsigned char h[32];qmem_hash(txt,(size_t)sqlite3_column_bytes(s,1),h);
  if(sqlite3_column_bytes(s,2)!=32||memcmp(sqlite3_column_blob(s,2),h,32))fail("stored event digest mismatch");
  uint64_t tick=(uint64_t)sqlite3_column_int64(s,0);char *line=strdup((const char*)sqlite3_column_text(s,1));if(!line)fail("OOM");
  char bef[65],aft[65],ev[65],qm[512];token(bef,sizeof(bef),field(line,"before"));token(aft,sizeof(aft),field(line,"state"));token(ev,sizeof(ev),field(line,"event"));token(qm,sizeof(qm),field(line,"qmemory"));
  if(!valid_hex64(bef)||!valid_hex64(aft)||!valid_hex64(ev)||strlen(qm)<65||qm[64]!=':')fail("invalid event fact input");
  if(fprintf(f,"%" PRIu64 "\t%s\t%s\t%s\t%.*s\n",tick,bef,aft,ev,64,qm)<0)fail("event facts write failed");free(line);
 }
 if(rc!=SQLITE_DONE)fail("event scan error");sqlite3_finalize(s);if(fclose(f))fail("facts flush failure");
 puts("QMEM_SQLITE_DEV_FACTS_PASS");
}
int main(int argc,char **argv){
 if(argc<3){fprintf(stderr,"usage: qmem-dev init DB | import DB EXPORT.qmx UPSTREAM.receipt | verify DB | root DB | get DB TICK CELL_ID | facts DB PREFIX\n");return 2;}
 int initialize=(!strcmp(argv[1],"init")&&argc==3);
 int writable=(!strcmp(argv[1],"import")&&argc==5);
 sqlite3 *db=connect_db(argv[2],initialize,writable);
 if(!strcmp(argv[1],"init")&&argc==3){puts("QMEM_SQLITE_DEV_INIT_PASS");}
 else if(!strcmp(argv[1],"import")&&argc==5)import_cells(db,argv[3],argv[4]);
 else if(!strcmp(argv[1],"verify")&&argc==3)verify_db(db);
 else if(!strcmp(argv[1],"root")&&argc==3){char h[65];root_hex(db,h);puts(h);}
 else if(!strcmp(argv[1],"get")&&argc==5)show(db,argv[3],argv[4]);
 else if(!strcmp(argv[1],"facts")&&argc==4)export_facts(db,argv[3]);
 else fail("unsupported operation or argument count");
 if(sqlite3_close(db)!=SQLITE_OK)fail("database close failed");return 0;
}
