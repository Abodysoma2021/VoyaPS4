#include "core.hpp"
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

// OpenOrbis 0.5.4 implements lstat via fstatat, which returns ENOSYS on PS4.
// Compile metadata.cpp with -Dlstat=voya_unavailable_lstat to reproduce that ABI.
extern "C" int voya_unavailable_lstat(const char*,struct stat*) {
    errno=ENOSYS;return -1;
}
template<class F> void rejects(F action) {
    bool failed=false;
    try {action();} catch(const voya::Error&) {failed=true;}
    assert(failed);
}
int main(int argc,char** argv) {
    assert(argc==2);
    std::string directory=argv[1],file=directory+"/metadata.json";
    voya::make_directory(directory); // Existing directories must survive ENOSYS lstat.
    voya::make_directory(directory+"/new");
    voya::atomic_write(file,"{\"schema\":1}");
    assert(voya::file_size(file)==12);
    assert(voya::read_text(file)=="{\"schema\":1}");
    assert(voya::file_size(directory+"/missing")==0);
    rejects([&]{voya::make_directory(file);});
    rejects([&]{voya::file_size(directory);});
    assert(symlink(file.c_str(),(directory+"/file-link").c_str())==0);
    assert(symlink((directory+"/new").c_str(),(directory+"/dir-link").c_str())==0);
    rejects([&]{voya::file_size(directory+"/file-link");});
    rejects([&]{voya::make_directory(directory+"/dir-link");});
    assert(mkfifo((directory+"/pipe").c_str(),0600)==0);
    rejects([&]{voya::file_size(directory+"/pipe");});
    // A sparse file exercises byte counts above 4 GiB without large disk usage.
    int fd=open((directory+"/large").c_str(),O_WRONLY|O_CREAT,0600);
    assert(fd>=0);assert(ftruncate(fd,off_t(5)*1024*1024*1024+17)==0);close(fd);
    assert(voya::file_size(directory+"/large")==uint64_t(5)*1024*1024*1024+17);
    puts("Storage works without lstat; links and special files rejected");
}
