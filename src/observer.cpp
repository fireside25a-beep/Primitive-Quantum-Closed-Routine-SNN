#include "observer.hpp"
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace nm_observer {
namespace {
struct Errno {int saved=errno; ~Errno(){errno=saved;}};
int descriptor(const char *name) noexcept {
    const char *s=std::getenv(name);if(!s)return -1;
    int fd=-1;auto r=std::from_chars(s,s+std::strlen(s),fd);
    return r.ec==std::errc{}&&*r.ptr=='\0'&&fd>=3?fd:-1;
}
}
Observer::Observer() noexcept {
    Errno preserve;
    int w=descriptor("NM_OBSERVER_WRITE_FD"),r=descriptor("NM_OBSERVER_GUARD_FD");
    if(w<0||r<0||w==r)return;
    struct stat ws{},rs{};
    int wf=fcntl(w,F_GETFL),rf=fcntl(r,F_GETFL);
    if(fstat(w,&ws)||fstat(r,&rs)||!S_ISFIFO(ws.st_mode)||!S_ISFIFO(rs.st_mode)||
       ws.st_dev!=rs.st_dev||ws.st_ino!=rs.st_ino||wf<0||rf<0||
       (wf&O_ACCMODE)!=O_WRONLY||(rf&O_ACCMODE)!=O_RDONLY||!(wf&O_NONBLOCK))return;
    long capacity=fpathconf(w,_PC_PIPE_BUF);if(capacity<512)return;
    // The retained read end prevents SIGPIPE even if the viewer disappears.
    guard_=fcntl(r,F_DUPFD_CLOEXEC,3);if(guard_<0)return;
    write_=fcntl(w,F_DUPFD_CLOEXEC,3);
    if(write_<0){close(guard_);guard_=-1;return;}
    fcntl(w,F_SETFD,FD_CLOEXEC);fcntl(r,F_SETFD,FD_CLOEXEC);
    limit_=static_cast<unsigned>(std::min<long>(capacity,4096));
}
Observer::~Observer(){
    Errno preserve;
    if(write_>=0){if(!ended_)frame("abort","execution did not reach final root");close(write_);}
    if(guard_>=0)close(guard_);
}
void Observer::frame(std::string_view kind,std::string_view payload) noexcept {
    Errno preserve;if(write_<0)return;
    char bytes[4096];auto seq=sequence_++;
    int prefix=std::snprintf(bytes,sizeof(bytes),"NMOBS1|%llu|%llu|%.*s|",
        static_cast<unsigned long long>(seq),static_cast<unsigned long long>(dropped_),
        static_cast<int>(kind.size()),kind.data());
    if(prefix<0||static_cast<size_t>(prefix)+payload.size()+1>limit_){++dropped_;return;}
    std::memcpy(bytes+prefix,payload.data(),payload.size());
    auto size=static_cast<size_t>(prefix)+payload.size();bytes[size++]='\n';
    // A frame is <= PIPE_BUF: full atomic delivery or EAGAIN, no partial queue.
    if(write(write_,bytes,size)!=static_cast<ssize_t>(size))++dropped_;
}
void Observer::begin(std::string_view mode,std::string_view scenario) noexcept {
    if(write_<0)return;
    char data[128];int n=std::snprintf(data,sizeof(data),"%.*s|%.*s",static_cast<int>(mode.size()),mode.data(),static_cast<int>(scenario.size()),scenario.data());
    if(n>0&&static_cast<size_t>(n)<sizeof(data))frame("begin",{data,static_cast<size_t>(n)});
}
void Observer::tick(std::uint64_t tick,std::string_view record) noexcept {
    if(write_<0)return;
    if(!record.empty()&&record.back()=='\n')record.remove_suffix(1);
    const size_t chunk=limit_-192,parts=(record.size()+chunk-1)/chunk;
    for(size_t part=0;part<parts;++part){
        char data[4096];int prefix=std::snprintf(data,sizeof(data),"%llu|%zu|%zu|",static_cast<unsigned long long>(tick),part,parts);
        auto fragment=record.substr(part*chunk,chunk);
        if(prefix<0||static_cast<size_t>(prefix)+fragment.size()>sizeof(data))return;
        std::memcpy(data+prefix,fragment.data(),fragment.size());
        frame("tick",{data,static_cast<size_t>(prefix)+fragment.size()});
    }
}
void Observer::finish(std::string_view root) noexcept {
    if(!root.empty()&&root.back()=='\n')root.remove_suffix(1);
    frame("end",root);ended_=true;
}
}
