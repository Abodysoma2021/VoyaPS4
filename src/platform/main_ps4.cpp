#include "ui.hpp"
#include <orbis/AppInstUtil.h>
#include <orbis/CommonDialog.h>
#include <orbis/ImeDialog.h>
#include <orbis/Pad.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include <orbis/VideoOut.h>
#include <orbis/libkernel.h>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <future>
#include <sys/stat.h>
#include <unistd.h>

using namespace voya;
namespace {
// Latest launch only, bounded to startup milestones; no URLs, catalog data or credentials.
void boot_trace(const char* stage,const char* message=nullptr,bool reset=false) {
    // Match the SDK trophies sample: channel 0 is visible through GoldHEN's klog server.
    // Emit before filesystem work so sandbox/storage failures remain observable.
    // This export accepts text; its SDK variadic declaration does not format it.
    char diagnostic[640];
    snprintf(diagnostic,sizeof(diagnostic),"[VoyaPS4 %s] stage=%s error=%.400s\n",VOYA_VERSION,stage,message?message:"");
    sceKernelDebugOutText(0,diagnostic);
    static const long started=static_cast<long>(time(nullptr));
    int created=mkdir("/data/voyaps4",0700);
    if(created<0 && errno!=EEXIST) {
        snprintf(diagnostic,sizeof(diagnostic),"[VoyaPS4] boot directory errno=%d\n",errno);
        sceKernelDebugOutText(0,diagnostic);
    }
    int fd=open("/data/voyaps4/boot.log",O_WRONLY|O_CREAT|O_NOFOLLOW|(reset?O_TRUNC:O_APPEND),0600);
    if(fd<0) {
        snprintf(diagnostic,sizeof(diagnostic),"[VoyaPS4] boot log open errno=%d\n",errno);
        sceKernelDebugOutText(0,diagnostic);
        return;
    }
    FILE* output=fdopen(fd,"a");
    if(!output) {close(fd);return;}
    fprintf(output,"{\"event\":\"ps4_startup\",\"entry_point\":\"ps4_app\",\"run_id\":\"%ld-%ld\",\"version\":\"%s\",\"stage\":\"%s\"",
            started,static_cast<long>(getpid()),VOYA_VERSION,stage);
    if(message) {
        fputs(",\"error\":\"",output);
        for(size_t i=0;message[i] && i<400;++i) {
            unsigned char c=message[i];
            if(c=='"' || c=='\\') fputc('\\',output);
            fputc(c>=32 && c<127?c:' ',output);
        }
        fputc('"',output);
    }
    fputs("}\n",output);fflush(output);fsync(fd);fclose(output);
}
void require(int code,const char* operation) {
    if(code>=0) return;
    char text[180];snprintf(text,sizeof(text),"%s (0x%08x)",operation,unsigned(code));throw Error(text);
}
class Display {
    int video=-1,index=0,frame=0;
    void* memory=nullptr;
    void* buffers[2]{};
    off_t offset=0;
    static constexpr size_t allocation=16*1024*1024;
    OrbisKernelEqueue queue{};
public:
    Display() {
        video=sceVideoOutOpen(ORBIS_VIDEO_USER_MAIN,ORBIS_VIDEO_OUT_BUS_MAIN,0,nullptr);
        require(video,"Cannot open display");
        require(sceKernelAllocateDirectMemory(0,sceKernelGetDirectMemorySize(),allocation,0x200000,3,&offset),"Cannot allocate display memory");
        require(sceKernelMapDirectMemory(&memory,allocation,0x33,0,offset,0x200000),"Cannot map display memory");
        buffers[0]=memory;buffers[1]=static_cast<uint8_t*>(memory)+screen_width*screen_height*4;
        OrbisVideoOutBufferAttribute attribute{};
        sceVideoOutSetBufferAttribute(&attribute,0x80000000,1,0,screen_width,screen_height,screen_width);
        require(sceVideoOutRegisterBuffers(video,0,buffers,2,&attribute),"Cannot register display buffers");
        require(sceKernelCreateEqueue(&queue,"VoyaPS4 display"),"Cannot create display queue");
        require(sceVideoOutAddFlipEvent(queue,video,nullptr),"Cannot create display event");
        sceVideoOutSetFlipRate(video,0);
    }
    ~Display() {
        if(video>=0) sceVideoOutClose(video);
        if(memory) {sceKernelMunmap(memory,allocation);sceKernelReleaseDirectMemory(offset,allocation);}
        if(queue) sceKernelDeleteEqueue(queue);
    }
    uint32_t* pixels() {return static_cast<uint32_t*>(buffers[index]);}
    void flip() {
        require(sceVideoOutSubmitFlip(video,index,ORBIS_VIDEO_OUT_FLIP_VSYNC,frame),"Display flip failed");
        for(;;) {
            OrbisVideoOutFlipStatus status{};sceVideoOutGetFlipStatus(video,&status);
            if(status.flipArg==frame) break;
            OrbisKernelEvent event{};int count=0;
            if(sceKernelWaitEqueue(queue,&event,1,&count,nullptr)<0) break;
        }
        index=(index+1)%2;++frame;
    }
};
class Keyboard {
    // The platform IME uses UTF-16. This translation unit is compiled with -fshort-wchar.
    wchar_t buffer[2049]{};
    wchar_t title[50]{};
public:
    bool active=false,catalog=false;
    void open(int user,bool for_catalog) {
        require(sceSysmoduleLoadModule(ORBIS_SYSMODULE_IME_DIALOG),"Cannot load on-screen keyboard");
        sceCommonDialogInitialize();
        memset(buffer,0,sizeof(buffer));memset(title,0,sizeof(title));
        const char* label=for_catalog?"HTTPS catalog URL":"Direct package URL";
        for(size_t i=0;label[i];++i) title[i]=label[i];
        OrbisImeDialogSetting setting{};
        setting.userId=user;setting.type=ORBIS_TYPE_TYPE_URL;
        setting.maxTextLength=2048;setting.inputTextBuffer=buffer;setting.title=title;
        setting.enterLabel=ORBIS_BUTTON_LABEL_GO;
        require(sceImeDialogInit(&setting,nullptr),"Cannot open on-screen keyboard");
        active=true;catalog=for_catalog;
    }
    std::string poll() {
        if(!active || sceImeDialogGetStatus()!=ORBIS_DIALOG_STATUS_STOPPED) return {};
        OrbisDialogResult result{};int code=sceImeDialogGetResult(&result);
        sceImeDialogTerm();active=false;
        if(code<0 || result.endstatus!=ORBIS_DIALOG_OK) return {};
        std::string value;
        for(size_t i=0;i<2048 && buffer[i];++i) {
            if(buffer[i]<32 || buffer[i]>=127) throw Error("Use an ASCII URL; percent-encode other characters");
            value.push_back(char(buffer[i]));
        }
        return trim(value);
    }
};
std::string stage_update(const Job& job,const std::string& path) {
    if(file_sha256(path)!=job.item.sha256) throw Error("Update integrity check failed");
    make_directory("/data/pkg");
    auto target=std::string("/data/pkg/VoyaPS4-update.pkg");
    auto temporary=target+".tmp";
    int fd=open(temporary.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);
    if(fd<0) throw Error("Cannot stage update in /data/pkg");
    std::unique_ptr<FILE,decltype(&fclose)> output(fdopen(fd,"wb"),fclose);
    if(!output) {close(fd);throw Error("Cannot stage update");}
    std::unique_ptr<FILE,decltype(&fclose)> input(fopen(path.c_str(),"rb"),fclose);
    if(!input) throw Error("Cannot read verified update");
    char buffer[128*1024];size_t count;
    while((count=fread(buffer,1,sizeof(buffer),input.get())))
        if(fwrite(buffer,1,count,output.get())!=count) throw Error("Cannot write staged update; check free space");
    if(ferror(input.get()) || fflush(output.get()) || fsync(fileno(output.get()))) throw Error("Cannot save staged update");
    output.reset();
    if(file_sha256(temporary)!=job.item.sha256 || rename(temporary.c_str(),target.c_str()))
        throw Error("Staged update failed verification");
    return "Update saved. Close VoyaPS4, then install /data/pkg/VoyaPS4-update.pkg using your loader.";
}
void install_package(Library& library,const Job& job) {
    if(job.item.update) {library.notice(stage_update(job,library.path(job.item)));return;}
    require(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_APP_INST_UTIL),"Cannot load package installer");
    require(sceAppInstUtilInitialize(),"Cannot initialize package installer");
    struct Session {~Session() {sceAppInstUtilTerminate();}} session;
    auto path=library.path(job.item);
    if(!job.item.sha256.empty() && file_sha256(path)!=job.item.sha256) throw Error("Package integrity changed; installation blocked");
    char title_id[16]{};int is_app=0;
    require(sceAppInstUtilGetTitleIdFromPkg(path.c_str(),title_id,&is_app),"Cannot read package title ID");
    if(std::string(title_id)=="VOYA00001") throw Error("Use Updates to stage VoyaPS4 updates; close the app before installing");
    int exists=0;
    require(sceAppInstUtilAppExists(title_id,&exists),"Cannot check installed package");
    if(exists) {
        library.notice("This title is installed. Use your loader's package installer to replace it; downloaded file is kept.");
        return;
    }
    int result=sceAppInstUtilAppInstallPkg(path.c_str(),nullptr);
    require(result,"Install request failed; use your loader's package installer");
    library.notice("Installation requested. Check PS4 notifications for completion; downloaded file is kept.");
}
}
int main() {
    setvbuf(stdout,nullptr,_IONBF,0);
    boot_trace("entered_main",nullptr,true);
    try {
        boot_trace("opening_display");
        Display display;boot_trace("display_ready");
        Canvas canvas("/app0/assets/fonts/SpaceGrotesk.ttf");boot_trace("font_ready");
        View view;Keyboard keyboard;
        require(scePadInit(),"Cannot initialize controller");
        OrbisUserServiceInitializeParams params{};params.priority=ORBIS_KERNEL_PRIO_FIFO_LOWEST;
        sceUserServiceInitialize(&params);
        int user=-1;require(sceUserServiceGetInitialUser(&user),"Cannot get active user");
        int pad=scePadOpen(user,0,0,nullptr);require(pad,"Cannot open controller");
        boot_trace("controller_ready");
        make_directory("/data/voyaps4");
        boot_trace("storage_ready");
        auto catalog=parse_catalog(read_text("/app0/assets/catalog.json"));
        boot_trace("catalog_ready");
        Library library("/data/voyaps4",catalog,make_http());
        boot_trace("library_ready");
        uint32_t previous=0;std::string confirm_id;
        std::future<void> install_task;
        bool first_frame=true;
        for(;;) {
            auto snapshot=library.snapshot();
            OrbisPadData pad_data{};
            bool connected=scePadReadState(pad,&pad_data)>=0 && pad_data.connected;
            uint32_t held=connected?pad_data.buttons:0;
            uint32_t pressed=held & ~previous;previous=held;
            try {
                if(install_task.valid() && install_task.wait_for(std::chrono::seconds(0))==std::future_status::ready)
                    install_task.get();
                if(keyboard.active) {
                    auto value=keyboard.poll();
                    if(!value.empty()) {
                        if(keyboard.catalog) library.set_catalog(value);
                        else {
                            Item item;item.name="Direct package";item.url=value;
                            item.description="User-provided direct URL";library.enqueue(item);
                            view.tab=1;view.selected=std::max(0,int(library.snapshot().jobs.size())-1);
                        }
                    }
                } else if(!confirm_id.empty()) {
                    if(pressed&ORBIS_PAD_BUTTON_CIRCLE) {confirm_id.clear();view.confirmation.clear();}
                    if(pressed&ORBIS_PAD_BUTTON_CROSS) {
                        auto selected=confirm_id;confirm_id.clear();view.confirmation.clear();
                        if(install_task.valid()) throw Error("An installation is already being prepared");
                        for(const auto& job:snapshot.jobs) if(job.item.id()==selected) {
                            library.notice("Checking package and preparing installation...");
                            install_task=std::async(std::launch::async,[&library,job]{install_package(library,job);});
                        }
                    }
                } else {
                    if(pressed&ORBIS_PAD_BUTTON_L1) {view.tab=(view.tab+3)%4;view.selected=0;}
                    if(pressed&ORBIS_PAD_BUTTON_R1) {view.tab=(view.tab+1)%4;view.selected=0;}
                    int count=view.tab==0?int(snapshot.catalog.size()):int(snapshot.jobs.size());
                    if(pressed&ORBIS_PAD_BUTTON_UP) view.selected=std::max(0,view.selected-1);
                    if(pressed&ORBIS_PAD_BUTTON_DOWN) view.selected=std::min(std::max(0,count-1),view.selected+1);
                    view.selected=std::min(view.selected,std::max(0,count-1));
                    if(pressed&ORBIS_PAD_BUTTON_OPTIONS) keyboard.open(user,true);
                    if(pressed&ORBIS_PAD_BUTTON_TRIANGLE) {
                        if(view.tab==2 && !snapshot.update.url.empty()) library.enqueue(snapshot.update);
                        else keyboard.open(user,false);
                    }
                    if(pressed&ORBIS_PAD_BUTTON_CROSS) {
                        if(view.tab==0 && !snapshot.catalog.empty()) {
                            library.enqueue(snapshot.catalog.at(view.selected));view.tab=1;
                            view.selected=std::max(0,int(library.snapshot().jobs.size())-1);
                        } else if(view.tab==1 && !snapshot.jobs.empty()) {
                            auto job=snapshot.jobs.at(view.selected);
                            if(job.state==State::ready) {
                                confirm_id=job.item.id();view.confirmation=job.item.name;view.confirmation_update=job.item.update;
                            }
                            else library.toggle(job.item.id());
                        } else if(view.tab==2) library.check_update();
                    }
                    if(view.tab==1 && !snapshot.jobs.empty()) {
                        auto id=snapshot.jobs.at(view.selected).item.id();
                        if(pressed&ORBIS_PAD_BUTTON_CIRCLE) library.cancel(id);
                        if(pressed&ORBIS_PAD_BUTTON_SQUARE) library.restart(id);
                    }
                }
            } catch(const std::exception& e) {library.notice(e.what());}
            canvas.attach(display.pixels());
            draw_ui(canvas,library.snapshot(),view);
            canvas.text("L1 / R1   Browse",1000,56,19,0xffa1b3cd,220);
            if(!connected) canvas.text("Reconnect your controller",840,94,22,0xffffb276,390);
            display.flip();
            if(first_frame) {boot_trace("first_frame");first_frame=false;}
        }
    } catch(const std::exception& e) {
        boot_trace("failed",e.what());
        printf("VoyaPS4 startup failed: %s\n",e.what());
        // On this console musl exit -> libkernel _exit raised SIGSYS. Keep the
        // diagnosable failed process alive until the user closes it with the PS menu.
        for(;;) sceKernelUsleep(250000);
    }
}
