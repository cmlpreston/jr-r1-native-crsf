// Compare generated subsets through the actual LVGL lookup/fallback code.
#include "ui_typography.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <vector>

// The pinned originals are references only, not linked into production firmware.
namespace Baseline12 {
#undef LV_FONT_MONTSERRAT_12
#define LV_FONT_MONTSERRAT_12 1
#include R1_BASELINE_FONT_12
}
namespace Baseline14 {
#undef LV_FONT_MONTSERRAT_14
#define LV_FONT_MONTSERRAT_14 1
#include R1_BASELINE_FONT_14
}
namespace Baseline16 {
#undef LV_FONT_MONTSERRAT_16
#define LV_FONT_MONTSERRAT_16 1
#include R1_BASELINE_FONT_16
}
namespace Baseline22 {
#undef LV_FONT_MONTSERRAT_22
#define LV_FONT_MONTSERRAT_22 1
#include R1_BASELINE_FONT_22
}
namespace Baseline32 {
#undef LV_FONT_MONTSERRAT_32
#define LV_FONT_MONTSERRAT_32 1
#include R1_BASELINE_FONT_32
}
namespace Baseline48 {
#undef LV_FONT_MONTSERRAT_48
#define LV_FONT_MONTSERRAT_48 1
#include R1_BASELINE_FONT_48
}

static bool equalGlyph(const lv_font_t *before,const lv_font_t *after,uint32_t point,uint32_t next=0)
{
    lv_font_glyph_dsc_t oldGlyph{},newGlyph{};
    const bool oldFound=lv_font_get_glyph_dsc(before,&oldGlyph,point,next);
    const bool newFound=lv_font_get_glyph_dsc(after,&newGlyph,point,next);
    assert(oldFound==newFound);
    assert(oldGlyph.adv_w==newGlyph.adv_w&&oldGlyph.box_w==newGlyph.box_w&&oldGlyph.box_h==newGlyph.box_h);
    assert(oldGlyph.ofs_x==newGlyph.ofs_x&&oldGlyph.ofs_y==newGlyph.ofs_y&&oldGlyph.bpp==newGlyph.bpp);
    assert(oldGlyph.is_placeholder==newGlyph.is_placeholder);
    if(!oldFound)return false;
    assert(oldGlyph.resolved_font&&newGlyph.resolved_font);
    assert(oldGlyph.resolved_font->line_height==newGlyph.resolved_font->line_height);
    assert(oldGlyph.resolved_font->base_line==newGlyph.resolved_font->base_line);
    assert(oldGlyph.resolved_font->subpx==newGlyph.resolved_font->subpx);
    assert(oldGlyph.resolved_font->underline_position==newGlyph.resolved_font->underline_position);
    assert(oldGlyph.resolved_font->underline_thickness==newGlyph.resolved_font->underline_thickness);
    const size_t bytes=(size_t(oldGlyph.box_w)*oldGlyph.box_h*oldGlyph.bpp+7)/8;
    if(bytes){
        const auto oldBitmap=lv_font_get_glyph_bitmap(oldGlyph.resolved_font,point);
        const auto newBitmap=lv_font_get_glyph_bitmap(newGlyph.resolved_font,point);
        assert(oldBitmap&&newBitmap&&std::memcmp(oldBitmap,newBitmap,bytes)==0);
    }
    return true;
}

int main(int argc,char **argv)
{
    const auto *symbolData=static_cast<const lv_font_fmt_txt_dsc_t *>(r1_symbols_12.dsc);
    assert(symbolData->cmap_num==1);
    const auto &symbolMap=symbolData->cmaps[0];
    assert(symbolMap.type==LV_FONT_FMT_TXT_CMAP_SPARSE_TINY);
    std::vector<uint32_t> symbols;
    for(unsigned i=0;i<symbolMap.list_length;++i)symbols.push_back(symbolMap.range_start+symbolMap.unicode_list[i]);
    assert(!symbols.empty());
    const lv_font_t *oldFonts[]={&Baseline12::lv_font_montserrat_12,&Baseline14::lv_font_montserrat_14,
        &Baseline16::lv_font_montserrat_16,&Baseline22::lv_font_montserrat_22,
        &Baseline32::lv_font_montserrat_32,&Baseline48::lv_font_montserrat_48};
    const lv_font_t *newSymbols[]={&r1_symbols_12,&r1_symbols_14,&r1_symbols_16,&r1_symbols_22,&r1_symbols_32,&r1_symbols_48};
    const lv_font_t *primary[]={&r1_dm_12,&r1_dm_14,&r1_dm_16,&r1_manrope_22,&r1_manrope_32,&r1_manrope_48};
    for(unsigned size=0;size<6;++size){
        // Full symbol metrics/bitmap and mixed text/symbol advances are identical.
        for(const auto point:symbols){
            assert(equalGlyph(oldFonts[size],newSymbols[size],point));
            for(uint32_t next=32;next<127;++next)equalGlyph(oldFonts[size],newSymbols[size],point,next);
            for(const auto next:symbols)equalGlyph(oldFonts[size],newSymbols[size],point,next);
        }
        auto oldPrimary=*primary[size];oldPrimary.fallback=oldFonts[size];
        // Exhaustive normal Unicode lookup catches accidentally lost punctuation
        // as well as changed fallback metrics, without retaining unused PUA icons.
        for(uint32_t point=0;point<0xe000;++point)equalGlyph(&oldPrimary,primary[size],point);
        for(const auto point:symbols)equalGlyph(&oldPrimary,primary[size],point);
        for(int argument=1;argument<argc;++argument){
            const auto point=uint32_t(std::strtoul(argv[argument],nullptr,0));
            lv_font_glyph_dsc_t glyph{};
            assert(lv_font_get_glyph_dsc(primary[size],&glyph,point,0)&&!glyph.is_placeholder);
        }
    }
    // Preserve the former default's text metrics and every ASCII kerning pair.
    assert(LV_FONT_DEFAULT==&r1_default_14);
    for(uint32_t point=32;point<127;++point){
        for(uint32_t next=32;next<127;++next)equalGlyph(oldFonts[1],&r1_default_14,point,next);
        equalGlyph(oldFonts[1],&r1_default_14,point,176);
        equalGlyph(oldFonts[1],&r1_default_14,point,8226);
        for(const auto next:symbols)equalGlyph(oldFonts[1],&r1_default_14,point,next);
    }
    for(const auto point:{uint32_t(176),uint32_t(8226)}){
        for(uint32_t next=32;next<127;++next)equalGlyph(oldFonts[1],&r1_default_14,point,next);
        equalGlyph(oldFonts[1],&r1_default_14,point,176);
        equalGlyph(oldFonts[1],&r1_default_14,point,8226);
    }
    for(const auto point:symbols)equalGlyph(oldFonts[1],&r1_default_14,point);
    puts("Font subsets: exact bitmaps/metrics, six fallback chains, normal Unicode, source glyph coverage and original default kerning PASS");
}
