#pragma once
#include <cstddef>
#include <cstdlib>
#include <new>
#include <utility>
#if defined(ARDUINO) && !defined(JR_R1_SIMULATOR)
#include <esp_heap_caps.h>
#endif

// UI-owned, non-DMA storage. It is never allocated by the control/RF tasks.
// Keep its lifetime stable while LVGL/client code may hold pointers into it.
struct UiColdAllocator {
    using Allocate = void *(*)(size_t);
    using Release = void (*)(void *);
    static void *allocate(size_t bytes) {
#if defined(ARDUINO) && !defined(JR_R1_SIMULATOR)
        void *p=heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        // A missing/exhausted PSRAM heap must not make the UI dereference null.
        // Internal fallback retains the feature if sufficient memory remains.
        return p?p:heap_caps_malloc(bytes,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
#else
        return std::malloc(bytes);
#endif
    }
    static void release(void *p) { std::free(p); }
};

template<class T> class UiColdStorage {
public:
    explicit UiColdStorage(UiColdAllocator::Allocate allocate=UiColdAllocator::allocate,
                           UiColdAllocator::Release release=UiColdAllocator::release)
        :allocate_(allocate),release_(release){}
    ~UiColdStorage(){reset();}
    UiColdStorage(const UiColdStorage &)=delete;
    UiColdStorage &operator=(const UiColdStorage &)=delete;
    bool initialize(){
        if(value_)return true;
        void *p=allocate_(sizeof(T));if(!p)return false;
        value_=new(p) T{};return true;
    }
    void reset(){if(value_){value_->~T();release_(value_);value_=nullptr;}}
    T *get(){return value_;} const T *get()const{return value_;}
    explicit operator bool()const{return value_!=nullptr;}
private:
    T *value_=nullptr;
    UiColdAllocator::Allocate allocate_;
    UiColdAllocator::Release release_;
};

// Non-owning fixed-size view keeps array indexing/range iteration explicit.
// The owner must be available before indexing; UI failure paths check it.
template<class T,size_t N> class UiColdArrayView {
public:
    void attach(T *p){data_=p;}
    T &operator[](size_t n){return data_[n];}
    const T &operator[](size_t n)const{return data_[n];}
    T *data(){return data_;} const T *data()const{return data_;}
    T *begin(){return data_;} T *end(){return data_?data_+N:nullptr;}
    const T *begin()const{return data_;} const T *end()const{return data_?data_+N:nullptr;}
    static constexpr size_t bytes(){return sizeof(T)*N;}
private:
    T *data_=nullptr;
};
