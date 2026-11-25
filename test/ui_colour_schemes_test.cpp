#include "ui_colour_schemes.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
static double linear(double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);}
static double luminance(uint32_t colour)
{
    // LVGL truncates the configured RGB values to the LCD's RGB565 channels.
    const unsigned red = (colour >> 19) & 31;
    const unsigned green = (colour >> 10) & 63;
    const unsigned blue = (colour >> 3) & 31;
    return .2126*linear(red/31.0)+.7152*linear(green/63.0)+.0722*linear(blue/31.0);
}
static double contrast(uint32_t a,uint32_t b){const auto x=luminance(a),y=luminance(b);return x>y?(x+.05)/(y+.05):(y+.05)/(x+.05);}
int main(){
    static_assert(lightColourSchemeCount==4&&darkColourSchemeCount==4,"Four light and four dark schemes");
    assert(!darkColourChoice(0)&&lightColourChoice(0)==0);
    for(uint8_t c=1;c<=darkColourSchemeCount;++c)assert(darkColourChoice(c));
    for(uint8_t c=darkColourSchemeCount+1;c<colourSchemeChoiceCount;++c)
        assert(!darkColourChoice(c)&&lightColourChoice(c)==c-darkColourSchemeCount);
    assert(lightColourSchemes[0].surface==0xffffff&&lightColourSchemes[1].surface==0xffffff&&
        lightColourSchemes[3].surface==0xffffff);
    assert(contrast(lightColourSchemes[2].surface,0xffffff)>1.03);
    assert(darkColourSchemes[0].accent==0x58d2c3);
    for(uint8_t n=0;n<lightColourSchemeCount;++n){
        const auto &s=lightColourSchemes[n];
        assert(contrast(s.surface,s.background)>=1.15);
        assert(contrast(s.surface,s.selection)>=1.20);
        assert(contrast(s.line,s.surface)>=1.8);
        for(auto background:{s.background,s.surface,s.selection}){
            assert(contrast(s.text,background)>=4.5);
            assert(contrast(s.muted,background)>=4.5);
            assert(contrast(s.accent,background)>=4.5);
        }
        assert(contrast(0xe7edef,s.header)>=4.5);
        if(n!=0)assert(contrast(0xb42332,s.surface)>=4.5);
    }
    puts("Colour schemes: stable IDs, distinct light panels, selections and readable RGB565 text PASS");
}
