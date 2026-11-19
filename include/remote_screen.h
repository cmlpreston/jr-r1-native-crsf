#pragma once
#include <cstdint>
#include <cstring>

class RemoteScreenLease {
public:
    static constexpr uint32_t TimeoutMs = 25000;
    void renew(uint32_t now) { lastPoll_ = now; leased_ = true; }
    void clear() { leased_ = false; }
    bool active(uint32_t now) const { return leased_ && uint32_t(now - lastPoll_) < TimeoutMs; }

private:
    uint32_t lastPoll_ = 0;
    bool leased_ = false;
};

// Caller owns RGB565 storage. All access is on the UI loop, never the control task.
class RemoteScreen {
public:
    static constexpr int MaxRowsPerReply = 4;
    struct Strip {
        int first = 0;
        int count = 0;
        int next = 0;
    };

    void attach(uint16_t *buffer, int width, int height) {
        data_=buffer;w_=width;h_=height;revision_=0;
        std::memset(rows_,0,sizeof(rows_));
        if(data_)std::memset(data_,0,w_*h_*2);
    }
    void capture(int x,int y,int w,int h,const uint16_t *source) {
        if(!data_||x<0||y<0||w<1||h<1||x+w>w_||y+h>h_)return;
        bool changed = false;
        for (int r = 0; r < h; ++r) {
            auto *dst = data_ + (y + r) * w_ + x;
            if (!rows_[y + r] || std::memcmp(dst, source + r * w, w * 2)) {
                if (!changed && ++revision_ == 0) {
                    // Restart row revisions. The caller also advances the page
                    // epoch when it observes this wrap, forcing a full resync.
                    revision_ = 1;
                    for (int row = 0; row < h_; ++row) rows_[row] = revision_;
                }
                changed = true;
                std::memcpy(dst, source + r * w, w * 2);
                rows_[y + r] = revision_;
            }
        }
    }

    // Scan from the browser's cursor, returning only a contiguous run of dirty
    // rows. Once it reaches the end the browser acknowledges the revision from
    // the first request in the batch. Updates behind the cursor are picked up
    // next batch, even if later strips carried pixels newer than that revision.
    Strip strip(uint32_t since, int cursor) const {
        Strip result;
        if (!data_ || !revision_ || cursor < 0 || cursor >= h_) return result;
        auto dirty = [&](int row) { return since == 0 || rows_[row] > since; };
        int row = cursor;
        while (row < h_ && !dirty(row)) ++row;
        result.first = row;
        while (row < h_ && result.count < MaxRowsPerReply && dirty(row)) {
            ++row;
            ++result.count;
        }
        while (row < h_ && !dirty(row)) ++row;
        result.next = row < h_ ? row : 0;
        return result;
    }
    uint32_t revision() const{return revision_;}
    int width() const{return w_;} int height() const{return h_;}
    const uint16_t *data() const{return data_;}
private:
    uint16_t *data_=nullptr;int w_=0,h_=0;uint32_t revision_=0,rows_[480]={};
};
