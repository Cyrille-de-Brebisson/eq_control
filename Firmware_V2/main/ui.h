#pragma once
void doUI();
void beginUI();
bool display_pixel(int x, int y);

static uint16_t const keyMenu=  1<<0;
static uint16_t const keyGo=    1<<1;
static uint16_t const keyLeft=  1<<2;
static uint16_t const keySpd=   1<<3;
static uint16_t const keyUp=    1<<4;
static uint16_t const keyDown=  1<<5;
static uint16_t const keyEsc=   1<<6;
static uint16_t const keySync=  1<<7;
static uint16_t const keyRight= 1<<8;

extern uint32_t kbdMask; // used only on windws or simulated HW to press/release keys