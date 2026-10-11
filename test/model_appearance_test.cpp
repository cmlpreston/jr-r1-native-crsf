#include "model_appearance.h"
#include <cassert>
#include <cstdio>
int main(){
 uint8_t r=220,g=40,b=40;
 recolourModelPixel(r,g,b,kModelOriginalColour);assert(r==220&&g==40&&b==40);
 recolourModelPixel(r,g,b,0x0000ff);assert(r==40&&g==40&&b==220);
 r=g=b=90;recolourModelPixel(r,g,b,0xff0000);assert(r==90&&g==90&&b==90);
 assert(validModelColour(0xffffff)&&validModelColour(kModelOriginalColour)&&!validModelColour(0x1000001));
 // Yellow buggy wheel rims must not be painted; main body should be.
 assert(!modelPaintRegion(1,28,60,160,108));assert(modelPaintRegion(1,78,40,160,108));
 // F1 rear wing and front tyre excluded, nose included.
 assert(!modelPaintRegion(7,126,23,160,108));assert(!modelPaintRegion(7,89,75,160,108));
 assert(modelPaintRegion(7,63,61,160,108));
 assert(!modelPaintRegion(8,80,50,160,108));
 assert(modelPaintRole(0,40,17,160,108,20,150,230)==1); // Hyper 7 wing blue
 assert(modelPaintRole(0,40,17,160,108,210,210,210)==2); // wing white accent
 assert(modelPaintRole(0,40,17,160,108,50,50,50)==3); // wing dark base
 assert(modelPaintRole(0,10,60,160,108,50,50,50)==0); // tyre protected
 r=g=b=200;recolourModelMaterial(r,g,b,0xff0000,2);assert(r==200&&g==0&&b==0);
 r=g=b=40;recolourModelMaterial(r,g,b,0xffffff,3);assert(r==80&&g==80&&b==80);
 puts("Model appearance: original, neutral preservation, tint and body masks PASS");
}
