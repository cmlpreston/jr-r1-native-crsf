#include "ui_colour_schemes.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
static double linear(unsigned c){const double v=c/255.0;return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);}
static double luminance(uint32_t c){return .2126*linear((c>>16)&255)+.7152*linear((c>>8)&255)+.0722*linear(c&255);}
static double contrast(uint32_t a,uint32_t b){const auto x=luminance(a),y=luminance(b);return x>y?(x+.05)/(y+.05):(y+.05)/(x+.05);}
int main(){
    static_assert(lightColourSchemeCount==4&&darkColourSchemeCount==4,"Four light and four dark schemes");
    assert(!darkColourChoice(0)&&lightColourChoice(0)==0);
    for(uint8_t c=1;c<=darkColourSchemeCount;++c)assert(darkColourChoice(c));
    for(uint8_t c=darkColourSchemeCount+1;c<colourSchemeChoiceCount;++c)
        assert(!darkColourChoice(c)&&lightColourChoice(c)==c-darkColourSchemeCount);
    // Retain the original light/dark palette IDs and colours. Check the new
    // light palettes on the actual surfaces used by captions and selections.
    assert(lightColourSchemes[0].background==0xf3f5f1&&darkColourSchemes[0].accent==0x58d2c3);
    for(uint8_t n=1;n<lightColourSchemeCount;++n){
        const auto &s=lightColourSchemes[n];
        for(auto background:{s.background,s.surface,s.selection}){
            assert(contrast(s.text,background)>=4.5);
            assert(contrast(s.muted,background)>=4.5);
            assert(contrast(s.accent,background)>=4.5);
        }
        assert(contrast(0xffffff,s.header)>=4.5);
        assert(contrast(0xb42332,s.surface)>=4.5);
    }
    puts("Colour schemes: stable IDs, light choices, and readable text/captions/accents/headers/fault colours PASS");
}
