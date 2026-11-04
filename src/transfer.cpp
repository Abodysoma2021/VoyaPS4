#include "core.hpp"
#include "json.hpp"
#include <cstdio>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <unistd.h>

namespace voya {
using json = nlohmann::json;
namespace {
uint64_t number(const std::string& text) {
    if(text.empty() || text.find_first_not_of("0123456789")!=std::string::npos)
        throw Error("Server returned an invalid byte count");
    try {
        auto n=std::stoull(text);
        if(n>max_package_size) throw Error("Package exceeds the size limit");
        return n;
    } catch(const std::logic_error&) { throw Error("Server returned an invalid byte count"); }
}
struct Range { uint64_t start,end,total; };
Range parse_range(const std::string& value) {
    auto dash=value.find('-'),slash=value.find('/');
    if(value.rfind("bytes ",0)!=0 || dash==std::string::npos || slash==std::string::npos || slash<dash)
        throw Error("Server returned an invalid Content-Range");
    Range r{number(value.substr(6,dash-6)),number(value.substr(dash+1,slash-dash-1)),number(value.substr(slash+1))};
    if(r.end<r.start || r.total<=r.end) throw Error("Server returned an invalid Content-Range");
    return r;
}
bool transient(int status) { return status==408 || status==425 || status==429 || status>=500; }
void verify(const Item& item, const std::string& path, const std::function<bool()>& stop) {
    if(item.size && file_size(path)!=item.size) throw Error("Package length does not match catalog");
    if(!item.sha256.empty() && file_sha256(path,stop)!=item.sha256)
        throw Error("SHA-256 mismatch. Use Square to restart this download.");
    std::unique_ptr<FILE,decltype(&fclose)> file(fopen(path.c_str(),"rb"),fclose);
    unsigned char magic[4]{};
    if(!file || fread(magic,1,4,file.get())!=4 || magic[0]!=0x7f ||
       magic[1]!='C' || magic[2]!='N' || magic[3]!='T')
        throw Error("This file is not a PS4 PKG; check your direct URL");
}
void check_space(const std::string& directory, uint64_t remaining) {
    struct statvfs info{};
    if(statvfs(directory.c_str(),&info)==0) {
        uint64_t available=uint64_t(info.f_bavail)*info.f_frsize;
        if(remaining>available || available-remaining<16*1024*1024)
            throw Error("Not enough free storage for this download");
    }
}
}
std::string package_path(const Item& item, const std::string& directory) {
    return directory+"/"+item.id()+".pkg";
}
Outcome transfer(Http& http, const Item& item, const std::string& directory,
                 const std::function<bool()>& stop,
                 const std::function<void(Progress)>& progress) {
    validate_item(item);
    make_directory(directory);
    auto target=package_path(item,directory),partial=target+".part",sidecar=target+".resume.json";
    if(file_size(target)) {
        verify(item,target,stop);
        progress({file_size(target),file_size(target),false,true});
        return Outcome::complete;
    }
    uint64_t offset=file_size(partial);
    std::string validator;
    uint64_t saved_total=0;
    if(offset) {
        try {
            auto saved=json::parse(read_text(sidecar));
            if(saved.at("url")!=item.url || saved.at("sha256")!=item.sha256 || saved.at("size")!=item.size)
                offset=0;
            else {
                validator=saved.value("validator",std::string{});
                saved_total=saved.value("total",uint64_t{0});
                if(validator.find_first_of("\r\n")!=std::string::npos || validator.size()>2048)
                    throw Error("Invalid saved validator");
            }
        } catch(const std::exception&) { offset=0; }
        if(validator.empty() && item.sha256.empty()) offset=0;
        if(item.size && offset>item.size) offset=0;
    }
    if(stop()) return Outcome::paused;
    // A full partial with a known digest can be verified without another network request.
    bool full=offset && !item.sha256.empty() && (item.size?offset==item.size:offset==saved_total);
    uint64_t total=item.size;
    if(full) total=offset;
    if(!full) {
        auto response=http.open(item.url,offset,validator);
        bool restarted=false;
        if(response->status==416 && offset) {
            // A crash may leave all bytes written before promotion. When no digest
            // proves that partial complete, request a fresh representation safely.
            response=http.open(item.url,0,{});offset=0;restarted=true;
        }
        if(response->status!=200 && response->status!=206)
            throw Error("Download server returned HTTP "+std::to_string(response->status),transient(response->status));
        uint64_t response_size=0;
        if(response->status==206) {
            auto range=parse_range(response->header("content-range"));
            if(range.start!=offset) throw Error("Server returned the wrong byte range");
            if(item.size && range.total!=item.size) throw Error("Remote package size changed");
            if(saved_total && offset && range.total!=saved_total) throw Error("Remote package changed; restart the download");
            auto etag=response->header("etag");
            auto modified=response->header("last-modified");
            if(offset && !validator.empty() && etag!=validator && modified!=validator)
                throw Error("Remote package validator changed; restart the download");
            total=range.total;
            response_size=range.end-range.start+1;
        } else {
            restarted=restarted || offset>0;
            offset=0;
        }
        auto length=response->header("content-length");
        if(!length.empty()) {
            uint64_t declared=number(length);
            if(response_size && declared!=response_size) throw Error("Response length conflicts with byte range");
            response_size=declared;
            if(response->status==200) {
                if(item.size && declared!=item.size) throw Error("Remote package size differs from catalog");
                total=declared;
            }
        }
        if(total && total<offset) throw Error("Partial file is larger than the remote package");
        check_space(directory,total?total-offset:0);
        std::string fresh=response->header("etag");
        if(fresh.rfind("W/",0)==0) fresh.clear();
        if(fresh.empty()) fresh=response->header("last-modified");
        if(fresh.size()>2048 || fresh.find_first_of("\r\n")!=std::string::npos)
            throw Error("Invalid response validator");
        int fd=open(partial.c_str(),O_WRONLY|O_CREAT|O_NOFOLLOW|(offset?O_APPEND:O_TRUNC),0600);
        if(fd<0) throw Error("Cannot open partial package for writing");
        std::unique_ptr<FILE,decltype(&fclose)> file(fdopen(fd,"ab"),fclose);
        if(!file) { close(fd); throw Error("Cannot open package stream"); }
        atomic_write(sidecar,json{{"url",item.url},{"sha256",item.sha256},{"size",item.size},
                                 {"validator",fresh},{"total",total}}.dump());
        uint64_t done=offset,received=0,checkpoint=offset;
        progress({done,total,restarted,false});
        // PS4 worker stacks cannot hold a 128 KiB automatic buffer.
        std::vector<uint8_t> buffer(128*1024);
        while(!stop()) {
            size_t count=response->read(buffer.data(),buffer.size());
            if(!count) break;
            if(done+count>max_package_size || (total && done+count>total) ||
               (response_size && received+count>response_size))
                throw Error("Server sent more bytes than expected");
            if(fwrite(buffer.data(),1,count,file.get())!=count) throw Error("Storage write failed; check free space");
            done+=count; received+=count;
            if(done-checkpoint>=4*1024*1024) {
                if(fflush(file.get()) || fsync(fileno(file.get()))) throw Error("Cannot checkpoint package to storage");
                checkpoint=done;
            }
            progress({done,total,restarted,false});
        }
        if(fflush(file.get()) || fsync(fileno(file.get()))) throw Error("Cannot save partial package");
        if(stop()) return Outcome::paused;
        if((response_size && received!=response_size) || (total && done!=total))
            throw Error("Connection interrupted; waiting to resume",true);
        total=done;
    }
    if(stop()) return Outcome::paused;
    progress({file_size(partial),total,false,true});
    try { verify(item,partial,stop); }
    catch(const Error&) { if(stop()) return Outcome::paused; throw; }
    if(rename(partial.c_str(),target.c_str())) throw Error("Cannot finalize downloaded package");
    progress({total,total,false,false});
    return Outcome::complete;
}
}
