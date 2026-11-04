#pragma once
#include "library.hpp"
#include <vector>

namespace voya {
constexpr int screen_width=1280,screen_height=720;
struct Font;
class Canvas {
    uint32_t* pixels;
    std::unique_ptr<Font> font;
public:
    explicit Canvas(const std::string& font_path);
    ~Canvas();
    void attach(uint32_t* buffer) {pixels=buffer;}
    void rect(int x,int y,int width,int height,uint32_t color);
    void circle(int x,int y,int radius,uint32_t color);
    void text(const std::string& text,int x,int y,int size,uint32_t color,int max_width=1100);
};
struct View {
    int tab=0,selected=0;
    std::string confirmation;
    bool confirmation_update=false;
};
void draw_ui(Canvas& canvas,const Snapshot& snapshot,const View& view);
}
