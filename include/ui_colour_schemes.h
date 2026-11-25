#pragma once
#include <stdint.h>

struct UiColourScheme {
    const char *name;
    uint32_t background,surface,header,selection,accent,text,muted,line;
};
// Index zero remains the default for existing preferences.
constexpr UiColourScheme lightColourSchemes[]={
    {"Sage / teal",0xe7ede9,0xffffff,0x203739,0xdce9e2,0x006d5d,0x213a3c,0x4e655b,0xa8bcae},
    {"Cloud / blue",0xe4ebf5,0xffffff,0x233d59,0xd8e5f5,0x1f5f97,0x24374c,0x495f73,0xa6b9cf},
    {"Sand / bronze",0xeee6d8,0xfff9ec,0x49392c,0xe9dbc2,0x795017,0x3d3329,0x625140,0xb9a88d},
    {"Mint / forest",0xdeeee4,0xffffff,0x1f3d2a,0xd2e7d8,0x21683e,0x233b2a,0x405e48,0xa2b9aa},
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
