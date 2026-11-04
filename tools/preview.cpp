#include "ui.hpp"
#include <cstdio>
#include <vector>

int main() {
    voya::Canvas canvas("assets/fonts/SpaceGrotesk.ttf");
    std::vector<uint32_t> pixels(voya::screen_width*voya::screen_height);
    canvas.attach(pixels.data());
    voya::Snapshot snapshot;
    snapshot.catalog=voya::parse_catalog(voya::read_text("assets/catalog.json"));
    voya::Job job;job.item=snapshot.catalog.at(0);job.state=voya::State::downloading;
    job.done=14500000;job.total=job.item.size;job.bytes_per_second=2800000;
    snapshot.jobs.push_back(job);
    for(int tab=0;tab<4;++tab) {
        voya::View view;view.tab=tab;
        voya::draw_ui(canvas,snapshot,view);
        char path[100];snprintf(path,sizeof(path),"build/preview-%d.ppm",tab);
        FILE* file=fopen(path,"wb");
        if(!file) return 1;
        fprintf(file,"P6\n1280 720\n255\n");
        for(auto color:pixels) {
            unsigned char rgb[]={static_cast<unsigned char>(color>>16),static_cast<unsigned char>(color>>8),static_cast<unsigned char>(color)};
            fwrite(rgb,1,3,file);
        }
        fclose(file);
    }
}
