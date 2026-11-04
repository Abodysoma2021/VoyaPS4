#include "ui.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace voya {
namespace {
constexpr uint32_t background=0xff101b30,panel=0xff192942,edge=0xff2c405e;
constexpr uint32_t ink=0xfff2f5fc,muted=0xffa1b3cd,blue=0xff589cff,apricot=0xffffb276;
std::string bytes(uint64_t count) {
    char text[50];
    if(count>=1024ULL*1024*1024) snprintf(text,sizeof(text),"%.2f GB",count/(1024.0*1024*1024));
    else snprintf(text,sizeof(text),"%.1f MB",count/(1024.0*1024));
    return text;
}
std::string visible(const std::string& text) {
    std::string result;
    for(unsigned char c:text) result+=(c>=32 && c<127)?char(c):' ';
    return result;
}
}
struct Font {
    struct Size {
        std::vector<uint8_t> atlas=std::vector<uint8_t>(512*512);
        stbtt_bakedchar glyphs[95]{};
    };
    std::vector<uint8_t> file;
    std::map<int,std::unique_ptr<Size>> sizes;
    explicit Font(const std::string& path) {
        FILE* input=fopen(path.c_str(),"rb");
        if(!input) throw Error("Cannot open bundled font");
        fseek(input,0,SEEK_END);long length=ftell(input);rewind(input);
        if(length<=0 || length>2*1024*1024) {fclose(input);throw Error("Invalid bundled font");}
        file.resize(length);
        auto count=fread(file.data(),1,file.size(),input);fclose(input);
        if(count!=file.size()) throw Error("Cannot read bundled font");
    }
    Size& at(int pixels) {
        auto& size=sizes[pixels];
        if(!size) {
            size=std::make_unique<Size>();
            if(stbtt_BakeFontBitmap(file.data(),0,float(pixels),size->atlas.data(),512,512,32,95,size->glyphs)<=0)
                throw Error("Cannot render bundled font");
        }
        return *size;
    }
};
Canvas::Canvas(const std::string& path):pixels(nullptr),font(std::make_unique<Font>(path)) {}
Canvas::~Canvas()=default;
void Canvas::rect(int x,int y,int w,int h,uint32_t color) {
    int left=std::max(0,x),right=std::min(screen_width,x+w);
    if(left>=right) return;
    for(int row=std::max(0,y);row<std::min(screen_height,y+h);++row)
        std::fill(pixels+row*screen_width+left,pixels+row*screen_width+right,color);
}
void Canvas::circle(int x,int y,int r,uint32_t color) {
    for(int dy=-r;dy<=r;++dy) {
        int dx=int(std::sqrt(double(r*r-dy*dy)));
        rect(x-dx,y+dy,dx*2+1,1,color);
    }
}
void Canvas::text(const std::string& value,int x,int y,int size,uint32_t color,int max_width) {
    auto& face=font->at(size);
    float cursor=float(x);
    for(unsigned char character:visible(value)) {
        const auto& g=face.glyphs[character-32];
        if(cursor+g.xadvance>x+max_width) break;
        int gx=int(cursor+g.xoff),gy=int(y+size+g.yoff);
        int w=g.x1-g.x0,h=g.y1-g.y0;
        for(int row=0;row<h;++row) for(int col=0;col<w;++col) {
            int px=gx+col,py=gy+row;
            if(px<0 || py<0 || px>=screen_width || py>=screen_height) continue;
            unsigned alpha=face.atlas[(g.y0+row)*512+g.x0+col];
            uint32_t old=pixels[py*screen_width+px],mixed=0xff000000;
            for(int shift:{0,8,16}) mixed|=((((color>>shift)&255)*alpha+((old>>shift)&255)*(255-alpha))/255)<<shift;
            pixels[py*screen_width+px]=mixed;
        }
        cursor+=g.xadvance;
    }
}
void draw_ui(Canvas& c,const Snapshot& s,const View& v) {
    c.rect(0,0,1280,720,background);
    // A continuous route mark echoes transfer progress and the Voya wordmark.
    c.rect(48,58,7,33,blue);c.rect(63,70,7,21,apricot);c.rect(78,48,7,43,ink);
    c.text("voya",100,43,43,ink,160);c.text("PS4",202,57,18,blue,70);
    c.text("Your next stop.",48,108,20,muted,200);
    const char* tabs[]={"Library","Downloads","Updates","About"};
    for(int i=0;i<4;++i) {
        if(i==v.tab) {c.rect(32,204+i*58,212,47,panel);c.rect(32,204+i*58,4,47,blue);}
        c.text(tabs[i],58,211+i*58,25,i==v.tab?ink:muted,180);
    }
    c.text("VoyaPS4 " VOYA_VERSION,48,577,18,muted,200);
    c.text("Keep the app open",48,609,18,muted,200);
    c.text("while downloading.",48,634,18,muted,200);
    c.rect(280,48,1,604,edge);
    int left=324,width=908;
    c.text(tabs[v.tab],left,47,36,ink,width);
    if(v.tab==0) {
        c.rect(left,116,width,178,panel);
        c.text("A better way",left+26,133,43,ink,650);
        c.text("to keep going.",left+26,183,43,ink,650);
        c.text("Pause. Reconnect. Pick up where you left off.",left+26,246,22,muted,720);
        c.circle(1120,181,38,blue);c.rect(1080,172,72,17,panel);
        c.circle(1161,224,13,apricot);
        c.text("Your sources",left,310,24,ink,width);
        c.text("Triangle  Add URL     Options  Connect catalog",left+340,313,19,muted,580);
        if(s.catalog.empty()) {
            c.text("Add a package URL to start your library.",left,382,27,ink,width);
            c.text("Choose a homebrew release or a package you are authorized to use.",left,430,21,muted,width);
        }
        int first=std::max(0,v.selected-1);
        for(int row=0;row<2 && row+first<int(s.catalog.size());++row) {
            int index=first+row,y=355+row*119;
            bool focused=index==v.selected;
            c.rect(left,y,width,104,focused?blue:edge);
            c.rect(left+2,y+2,width-4,100,panel);
            c.circle(left+47,y+49,26,focused?apricot:edge);
            c.text(s.catalog[index].name.substr(0,1),left+37,y+29,30,background,50);
            c.text(s.catalog[index].name,left+92,y+15,28,ink,600);
            c.text(s.catalog[index].description,left+92,y+54,19,muted,690);
            c.text(s.catalog[index].size?bytes(s.catalog[index].size):"Direct URL",left+748,y+19,20,blue,150);
        }
    } else if(v.tab==1) {
        c.text("A saved place in every download.",left,103,23,muted,width);
        if(s.jobs.empty()) {
            c.text("Your queue is clear.",left,223,42,ink,width);
            c.text("Select a title in Library, or press Triangle to add its direct URL.",left,290,23,muted,width);
        }
        int first=std::max(0,v.selected-2);
        for(int row=0;row<3 && row+first<int(s.jobs.size());++row) {
            int index=first+row,y=160+row*139;const auto& j=s.jobs[index];
            c.rect(left,y,width,124,index==v.selected?blue:edge);
            c.rect(left+2,y+2,width-4,120,panel);
            c.text(j.item.name,left+19,y+9,27,ink,565);
            c.text(state_label(j.state),left+620,y+15,21,j.state==State::failed?apricot:blue,270);
            c.rect(left+20,y+57,width-40,7,edge);
            if(j.total) c.rect(left+20,y+57,int((width-40)*std::min(1.0,double(j.done)/j.total)),7,blue);
            std::string stats=bytes(j.done)+" / "+(j.total?bytes(j.total):"unknown size");
            if(j.bytes_per_second>1) stats+="    "+bytes(uint64_t(j.bytes_per_second))+"/s";
            c.text(stats,left+20,y+70,20,muted,420);
            c.text(j.message,left+450,y+73,18,muted,435);
            if(index==v.selected && !j.message.empty()) c.text(j.message,left,591,18,muted,width);
        }
        c.text("X  Pause / Resume / Install     O  Cancel     Square  Restart",left,620,20,ink,width);
    } else if(v.tab==2) {
        c.text("The journey gets better.",left,135,45,ink,width);
        c.text("Installed version  " VOYA_VERSION,left,204,25,blue,width);
        c.text("X  Check for an update",left,274,26,ink,width);
        if(!s.update.url.empty()) {
            c.rect(left,332,width,94,panel);
            c.text(s.update.name,left+20,345,28,ink,width-40);
            c.text("Triangle  Download verified update",left+20,386,22,blue,width-40);
        }
        c.text("Updates use the same resumable download queue.",left,465,23,muted,width);
        c.text("When ready, close VoyaPS4 and install the new PKG",left,507,23,muted,width);
        c.text("using your loader's package installer. Your queue stays saved.",left,540,23,muted,width);
    } else {
        c.text("Built for the long download.",left,134,42,ink,width);
        const char* lines[]={"Direct HTTP(S) packages and your own catalogs.",
            "Automatic retries with a saved queue and partial files.",
            "SHA-256 verification when the publisher provides a checksum.",
            "Native PS4 homebrew built with the open-source OpenOrbis SDK.",
            "Downloads pause when the app closes. Relaunch to resume.",
            "Firmware 13.52 hardware compatibility is awaiting verification.",
            "Source: github.com/Abodysoma2021/VoyaPS4"};
        for(int i=0;i<7;++i) c.text(lines[i],left,224+i*46,22,muted,width);
    }
    c.rect(left,658,width,1,edge);
    c.text(s.notice,left,674,18,muted,width);
    if(!v.confirmation.empty()) {
        c.rect(342,238,856,239,blue);c.rect(345,241,850,233,panel);
        c.text(v.confirmation_update?"Prepare this app update?":"Install this package?",375,264,34,ink,760);
        c.text(v.confirmation,375,321,24,apricot,760);
        c.text(v.confirmation_update?"X  Prepare update     O  Go back":"X  Request installation     O  Go back",375,403,24,ink,760);
    }
}
}
