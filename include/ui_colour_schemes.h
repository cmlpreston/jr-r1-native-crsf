#pragma once
#include <stdint.h>

struct UiColourScheme {
    const char *name;
    uint32_t background,surface,header,selection,accent,text,muted,line;
};
// Index zero preserves the existing light palette and old saved preferences.
constexpr UiColourScheme lightColourSchemes[]={
    {"Sage / teal",0xf3f5f1,0xffffff,0x203739,0xe0e9df,0x007e6c,0x213a3c,0x60776d,0xd0ddd3},
    {"Cloud / blue",0xeff4fa,0xfbfdff,0x233d59,0xdde8f5,0x1f5f97,0x24374c,0x53697d,0xb5c4d6},
    {"Sand / bronze",0xf7f2e8,0xfffdf8,0x49392c,0xeee3cf,0x865514,0x3d3329,0x72604f,0xc4b49b},
    {"Mint / forest",0xe9f4ec,0xf8fff9,0x1f3d2a,0xd7eadc,0x21683e,0x233b2a,0x4e6854,0xadc8b5},
};
constexpr UiColourScheme darkColourSchemes[]={
    {"Graphite / teal",0x151b20,0x202a30,0x10191d,0x28433f,0x58d2c3,0xedf3f2,0xaab7bd,0x3b4a52},
    {"Midnight / ice blue",0x111827,0x1d2a3b,0x0b1220,0x293e56,0x82c5ff,0xeef5ff,0xb1bfd0,0x3c4e64},
    {"Warm charcoal / ivory",0x1c1a18,0x2c2925,0x141311,0x433c30,0xe1cfab,0xf3eee6,0xbfb6a9,0x4e4840},
    {"Slate / soft lilac",0x1b1b26,0x292a39,0x14151e,0x413a55,0xc4b1f6,0xf0eef8,0xb9b7ca,0x48495f},
};
constexpr uint8_t lightColourSchemeCount=sizeof(lightColourSchemes)/sizeof(lightColourSchemes[0]);
constexpr uint8_t darkColourSchemeCount=sizeof(darkColourSchemes)/sizeof(darkColourSchemes[0]);
constexpr uint8_t colourSchemeChoiceCount=lightColourSchemeCount+darkColourSchemeCount;
// Original callback IDs 0 (Light) and 1..4 (dark schemes) remain stable.
inline bool darkColourChoice(uint8_t choice){return choice>0&&choice<=darkColourSchemeCount;}
inline uint8_t lightColourChoice(uint8_t choice){return choice>darkColourSchemeCount?choice-darkColourSchemeCount:0;}
