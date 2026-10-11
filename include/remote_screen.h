#pragma once
#include <cstdint>
#include <cstring>
// Caller owns RGB565 storage. All access is on the UI loop, never the control task.
class RemoteScreen {
public:
    void attach(uint16_t *buffer, int width, int height) {
        data_=buffer;w_=width;h_=height;revision_=0;
        std::memset(rows_,0,sizeof(rows_));
        if(data_)std::memset(data_,0,w_*h_*2);
    }
    void capture(int x,int y,int w,int h,const uint16_t *source) {
        if(!data_||x<0||y<0||w<1||h<1||x+w>w_||y+h>h_)return;
        for(int r=0;r<h;++r){auto *dst=data_+(y+r)*w_+x;
            if(!rows_[y+r]||std::memcmp(dst,source+r*w,w*2)){
                std::memcpy(dst,source+r*w,w*2);rows_[y+r]=++revision_;
            }
        }
    }
    bool region(uint32_t since,int &first,int &count) const {
        if(!data_||!revision_)return false;
        first=h_;int last=-1;
        for(int y=0;y<h_;++y)if(!since||since>revision_||rows_[y]>since){if(first==h_)first=y;last=y;}
        count=last-first+1;return last>=first;
    }
    uint32_t revision() const{return revision_;}
    int width() const{return w_;} int height() const{return h_;}
    const uint16_t *data() const{return data_;}
private:
    uint16_t *data_=nullptr;int w_=0,h_=0;uint32_t revision_=0,rows_[480]={};
};
