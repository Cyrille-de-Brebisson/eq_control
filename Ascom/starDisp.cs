using System;
using TPixel = System.UInt32;
using uint32_t = System.UInt32;
using uint64_t = System.UInt64;
using int32_t = System.Int32;
using static CStars;
using System.Diagnostics;
using System.Security.Cryptography;

namespace StarDisp { 
class CArduViseur {

const double MPI= 3.14159265358979323846264338327f;

////////////////////////////////////////
// Framebuffer functions...
int clipx=0, clipX=0, clipy=0, clipY=0; // here is the framebuffer, its size, and the cliping region for the clipped version of the drawing primitives
// sacve/restore clip allows a function to save the current clipping region (in a 4 uint array) and restore it later...
public void saveClip(ref int[] s, int x, int y, int X, int Y)
{
    s[0]= clipx; 
    s[1]= clipX; 
    s[2]= clipy; 
    s[3]= clipY; 
    clipx= x; clipX= X; clipy= y; clipY= Y; 
}
public void updateClip(ref int[] s, int x, int y, int X, int Y) 
{ 
    s[0]= clipx; 
    s[1]= clipX; 
    s[2]= clipy; 
    s[3]= clipY; 
	if (clipx<x) clipx= x; if (clipX>=X) clipX= X;
	if (clipy<y) clipy= y; if (clipY>=Y) clipY= Y;
}
public void restoreClip(ref int[] s) 
{ 
    clipx= s[0]; 
    clipX= s[1]; 
    clipy= s[2]; 
    clipY= s[3]; 
}

unsafe public void hLine2(uint *fb, int x, int y, int w, TPixel c) { while (--w>=0) fb[x++ +y*lastSky.screenw]=c; }
unsafe public void vLine2(uint *fb, int x, int y, int h, TPixel c) { while (--h>=0) fb[x+(y++)*lastSky.screenw]= c; }
unsafe public void pixel2(uint *fb, int x, int y, TPixel c) { fb[x+y*lastSky.screenw]= c; }
unsafe public void textCenter(uint *fb, int x, int y, ref string t, TPixel fg) { int l= t.Length; text(fb, x-l*3,y-4, l*6, t, fg); }

const double degToRad = MPI/180.0f;
public static double dtr(double a) { return (double)(a*degToRad); }

string[] bayerGreek = new string[]
{
    "Alp", "Bet", "Chi", "Del", "Eps", "Eta", "Gam", "Iot",
    "Kap", "Lam", "Mu",  "Nu",  "Ome", "Omi", "Phi", "Pi",
    "Psi", "Rho", "Sig", "Tau", "The", "Ups", "Xi",  "Zet"
};
string[] bayerConst = new string[]
{"And", "Ant", "Aps", "Aql", "Aqr", "Ara", "Ari", "Aur", "Boo", "CMa", "CMi", "CVn", "Cae", "Cam", "Cap", "Car", "Cas", "Cen", "Cep", "Cet", "Cha", "Cir", "Cnc", "Col", "Com", "CrA", "CrB", "Crt", "Cru", "Crv", "Cyg", "Del", "Dor", "Dra", "Equ", "Eri", "For", "Gem", "Gru", "Her", "Hor", "Hya", "Hyi", "Ind", "LMi", "Lac", "Leo", "Lep", "Lib", "Lup", "Lyn", "Lyr", "Men", "Mic", "Mon", "Mus", "Nor", "Oct", "Oph", "Ori", "Pav", "Peg", "Per", "Phe", "Pic", "PsA", "Psc", "Pup", "Pyx", "Ret", "Scl", "Sco", "Sct", "Ser", "Sex", "Sge", "Sgr", "Tau", "Tel", "TrA", "Tri", "Tuc", "UMa", "UMi", "Vel", "Vir", "Vol", "Vul"};

unsafe void fillRect(uint *fb, int32_t x, int32_t y, int32_t w, int32_t h, TPixel c)
{
    if (y+h<clipy || y>=clipY) return;
    if (x+w<clipx || x>=clipX) return;
    if (x<clipx) { w-=clipx-x; x=clipx; }
    if (y<clipy) { h-=clipy-y; y=clipy; }
    if (w>=clipX-x) w= clipX-x;
    if (h>=clipY-y) h= clipY-y;
    if (w<=0) return;
    if (h<=0) return;
    while (--h>=0) hLine2(fb, x, y++, w, c);
}
unsafe public void rect(uint *fb, int32_t x, int32_t y, int32_t w, int32_t h, TPixel c)
{
    if (h<=0 || w<=0) return;
    hLine(fb, x, y, w, c);
    if (h>1) hLine(fb, x, y+h-1, w, c);
    if (h<2) return;
    vLine(fb, x, y+1, h-2, c);
    if (w>1) vLine(fb, x+w-1, y+1, h-2, c);
}
unsafe public void rect(uint *fb, int32_t x, int32_t y, int32_t w, int32_t h, TPixel c, TPixel cin)
{
    rect(fb, x, y, w, h, c);
    fillRect(fb, x+1, y+1, w-2, h-2, cin);
}
unsafe public void hLine(uint *fb, int32_t x, int32_t y, int32_t w, TPixel c)
{
    if (y<clipy || y>=clipY) return;
    if (x<clipx) { w-=clipx-x; x= clipx; }
    if (w>=clipX-x) w= clipX-x-1;
    if (w<0) return;
    hLine2(fb, x, y, w, c);
}
unsafe public void vLine(uint *fb, int32_t x, int32_t y, int32_t h, TPixel c)
{
    if (x<clipx || x>=clipX) return;
    if (y<clipy) { h-=clipy-y; y=clipy; }
    if (h>=clipY-y) h= clipY-y-1;
    if (h<0) return;
    vLine2(fb, x, y, h, c);
}
unsafe public void pixel(uint *fb, int32_t x, int32_t y, TPixel c)
{
    if (y<clipy || y>=clipY || x<clipx || x>=clipX) return;
    pixel2(fb, x, y, c);
}
unsafe public void blit(uint *fb, int32_t x, int32_t y, ref TPixel[] b, int32_t w, int32_t h, TPixel trensparent)
{
    int src= 0;
    for (int32_t Y=0; Y<h; Y++)
    {
        int32_t ty= y+Y; if (ty<clipy || ty>=clipY) continue;
        for (int32_t X=0; X<w; X++)
        {
            int32_t tx= x+X; TPixel c= b[src++];
            if (tx<clipx || tx>=clipX || c==trensparent) continue;
            fb[x+X+(y+Y)*lastSky.screenw]= c;
        }
    }
}
unsafe public void blit(uint *fb, int32_t x, int32_t y, ref TPixel[] b, int32_t w, int32_t h)
{
    int src= 0;
    for (int32_t Y=0; Y<h; Y++)
    {
        int32_t ty= y+Y; if (ty<clipy || ty>=clipY) continue;
        for (int32_t X=0; X<w; X++)
        {
            int32_t tx= x+X; TPixel c= b[src++];
            if (tx<clipx || tx>=clipX) continue;
            fb[x+X+(y+Y)*lastSky.screenw]= c;
        }
    }
}
unsafe public bool lightUp(uint *fb, int32_t x, int32_t y, ref byte[] b, uint32_t filter)
{
    int w= 0;
    if (b.Length==1) w= 1;
    else if (b.Length<=9) w= 3;
    else if (b.Length<=25) w= 5;
    else if (b.Length<=49) w= 7;
    else w= 9;
    x-= w/2; y-= w/2; int h= w;
    if (x+w<=clipx || x>=clipX) return false;
    if (y+h<=clipy || y>=clipY) return false;
    int src= 0;
    for (int32_t Y=0; Y<h; Y++)
    {
        for (int32_t X=0; X<w; X++)
        {
            int32_t tx= x+X, ty= y+Y; byte c= b[src++]; c*=4;
            if (ty<clipy || ty>=clipY|| tx<clipx || tx>=clipX) continue;
            TPixel p= fb[x+X+(y+Y)*lastSky.screenw]= c;
            uint32_t R= (p&0xff); if ((filter&1)!=0) R+=c; if (R>0xff) R= 0xff;
            uint32_t G= ((p>>8)&0xff); if ((filter&2)!=0) G+=(uint)(c*2); if (G>0xff) G= 0xff;
            uint32_t B= ((p>>16)&0xff); if ((filter&4)!=0) B+=c; if (B>0xff) B= 0xff;
            fb[x+X+(y+Y)*lastSky.screenw]= R|(G<<8)|(B<<16);
        }
    }
    return true;
}
    // update x1 and y1 so that x1=clip and y1 is proportional as needed
    static void lclip(ref double x1, double x2, double clip, ref double y1, double y2)
    {
        y1= (y2-y1)*(clip-x1)/(x2-x1)+y1;
        x1= clip;
    }
unsafe public void line(uint *fb, double x1, double y1, double x2, double y2, TPixel c)
{
    if (x1<clipx && x2>=clipx) lclip(ref x1, x2, (double)(clipx), ref y1, y2);
    if (x2<clipx && x1>=clipx) lclip(ref x2, x1, (double)(clipx), ref y2, y1);
    if (x1<clipX && x2>=clipX) lclip(ref x2, x1, (double)(clipX-1), ref y2, y1);
    if (x2<clipX && x1>=clipX) lclip(ref x1, x2, (double)(clipX-1), ref y1, y2);
    if (y1<clipy && y2>=clipy) lclip(ref y1, y2, (double)(clipy), ref x1, x2);
    if (y2<clipy && y1>=clipy) lclip(ref y2, y1, (double)(clipy), ref x2, x1);
    if (y1<clipY && y2>=clipY) lclip(ref y2, y1, (double)(clipY-1), ref x2, x1);
    if (y2<clipY && y1>=clipY) lclip(ref y1, y2, (double)(clipY-1), ref x1, x2);
    if ((x1<clipx && x2<clipx) || (x1>=clipX && x2>=clipX)) return;
    if ((y1<clipy && y2<clipy) || (y1>=clipY && y2>=clipY)) return;
    line2(fb, (int32_t)(x1), (int32_t)(y1), (int32_t)(x2), (int32_t)(y2), c);
}
    int abs(int a) {  if (a>=0) return a; return -a; }
unsafe public void line2(uint *fb, int32_t x1, int32_t y1, int32_t x2, int32_t y2, TPixel c)
{
    int32_t dx= abs(x2-x1), dy= abs(y2-y1);
    if (dx==0) { if (y1<y2) vLine(fb, x1, y1, y2-y1, c); else vLine(fb, x1, y2, y1-y2, c); return; }
    if (dy==0) { if (x1<x2) hLine(fb, x1, y1, x2-x1, c); else hLine(fb, x2, y1, x1-x2, c); return; }
    if (dy<=dx) 
    {
        if (x1>x2) { int32_t t= x1; x1= x2; x2= t; t= y1; y1= y2; y2= t; }
        int32_t err= dx/2;
        int32_t x= dx; while (--x>=0)
        {
            fb[x1++ +y1*lastSky.screenw]= c;
            err-= dy; if (err<0) { err+= dx; if (y1<y2) y1++; else y1--; }
        }
    } else {
        if (y1>y2) { int32_t t= x1; x1= x2; x2= t; t= y1; y1= y2; y2= t; }
        int32_t err= dy/2;
        int32_t y= dy; while (--y>=0)
        {
            fb[x1 + (y1++) *lastSky.screenw]= c;
            err-= dx; if (err<0) { x1+= x1<x2? 1 : -1;  err+= dy; }
        }
    }
}

uint64_t[] font8= new uint64_t[]
{ 0x0000E0E0E0000000UL, 0x0000E0E0E0000000UL, 0x0000E0E0E0000000UL, 0x0000E0E0E0000000UL, 0x0000E0E0E0000000UL, 0x0000E0E0E0000000UL, 0x0000E0E0E0000000UL, 0x0000E0E0E0000000UL, 0x0000E0E0E0000000UL, 0x0000E0E0E0000000UL, 0x0000000000000000UL, 0x0000E0E0E0000000UL, 0x0000E0E0E0000000UL, 0x0000E0E0E0101000UL, 0x0000E0E0E0000000UL, 0x0000E0E0E0000000UL, 
0x0000E0E0E0000000UL, 0x0000E0E0E0000000UL, 0x0000E0E0E0000000UL, 0x0000E0E0E0000000UL, 0x0000E0E0E0000000UL, 0x0000E0E0E0000000UL, 0x0000E0E0E0000000UL, 0x0000E0E0E0000000UL, 0x0000E0E0E0000000UL, 0x0000E0E0E0000000UL, 0x0000E0E0E0000000UL, 0x0000E0E0E0000000UL, 0x80C0E1F1E1C08000UL, 0xF1F1F1F1F1F1F100UL, 0x0000000000515100UL, 0x0000000000515100UL, 
0x0000000000000000UL, 0x4040404040004000UL, 0xA0A0A00000000000UL, 0xA0A0F1A0F1A0A000UL, 0x40E150E041F04000UL, 0x3031804020918100UL, 0x2050502051906100UL, 0x4040400000000000UL, 0x8040202020408000UL, 0x2040808080402000UL, 0x00A040F140A00000UL, 0x004040F140400000UL, 0x0000000060604020UL, 0x000000F100000000UL, 0x0000000000606000UL, 0x0001804020100000UL, 
0xE01191513111E000UL, 0x406040404040E000UL, 0xE01101C02010F100UL, 0xE01101E00111E000UL, 0x80C0A090F1808000UL, 0xF110F0010111E000UL, 0xC02010F01111E000UL, 0xF101804020202000UL, 0xE01111E01111E000UL, 0xE01111E101806000UL, 0x0060600060600000UL, 0x0060600060604020UL, 0x8040201020408000UL, 0x0000F100F1000000UL, 0x1020408040201000UL, 0xE011018040004000UL, 
0xE01151D15010E100UL, 0xE01111F111111100UL, 0xF01111F01111F000UL, 0xE01110101011E000UL, 0x7090111111907000UL, 0xF11010F01010F100UL, 0xF11010F010101000UL, 0xE01110109111E100UL, 0x111111F111111100UL, 0xE04040404040E000UL, 0x010101011111E000UL, 0x1190503050901100UL, 0x101010101010F100UL, 0x11B1515111111100UL, 0x1111315191111100UL, 0xE01111111111E000UL, 
0xF01111F010101000UL, 0xE011111151906100UL, 0xF01111F050901100UL, 0xE01110E00111E000UL, 0xF140404040404000UL, 0x111111111111E000UL, 0x111111A0A0404000UL, 0x1111115151B11100UL, 0x1111A040A0111100UL, 0x1111A04040404000UL, 0xF10180402010F100UL, 0xE02020202020E000UL, 0x0010204080010000UL, 0xE08080808080E000UL, 0x40A0110000000000UL, 0x000000000000F100UL, 
0x2020400000000000UL, 0x0000E001E111E100UL, 0x1010F0111111F000UL, 0x0000E1101010E100UL, 0x0101E1111111E100UL, 0x0000E011F110E000UL, 0x40A0207020202000UL, 0x0000E01111E101E0UL, 0x1010F01111111100UL, 0x400060404040E000UL, 0x8000C08080809060UL, 0x1010905030509000UL, 0x604040404040E000UL, 0x0000B05151511100UL, 0x0000F01111111100UL, 0x0000E0111111E000UL, 
0x0000F01111F01010UL, 0x0000E11111E10101UL, 0x0000D13010101000UL, 0x0000E110E001F000UL, 0x2020702020A04000UL, 0x000011111111E100UL, 0x0000111111A04000UL, 0x000011115151A000UL, 0x000011A040A01100UL, 0x0000111111E101E0UL, 0x0000F1804020F100UL, 0xC02020102020C000UL, 0x4040404040404000UL, 0x6080800180806000UL, 0x0000205180000000UL, 0x51A051A051A05100UL, 
0x000180406090F100UL, 0xF10011A040A01100UL, 0x00F111A0A0400000UL, 0xC140404050604000UL, 0x8041404040502000UL, 0xF12140804021F100UL, 0x3070F0F1F0703000UL, 0x0000F1A0A0A0A000UL, 0x204080E11111E000UL, 0x01804020F100F100UL, 0x10204080F100F100UL, 0x0080F140F1200000UL, 0x0000006190906100UL, 0x004080F180400000UL, 0x004020F120400000UL, 0x4040404051E04000UL, 
0x40E0514040404000UL, 0x0000215180808000UL, 0x402040E090906000UL, 0x0000E010F010E000UL, 0x0000A05141410101UL, 0x609090F090906000UL, 0x0010102040A01100UL, 0x00C02121E0202010UL, 0x0000E19090906000UL, 0x0000E15040418000UL, 0x000090115151A000UL, 0x000040A011F10000UL, 0xF1A0A0A0A0A0A000UL, 0xE011111111A0B100UL, 0x0000E0E0E0000000UL, 0x0000A05151A00000UL, 
0xC02170207021C000UL, 0x4000404040404000UL, 0x0040E15050E14000UL, 0xC02120702020F100UL, 0x11E0111111E01100UL, 0x1111A0F140F14000UL, 0x4040400040404000UL, 0xC020E011E0806000UL, 0xA000000000000000UL, 0xE01171317111E000UL, 0x6080E09060F00000UL, 0x0041A050A0410000UL, 0x000000F080000000UL, 0x000000F000000000UL, 0xE0117171B111E000UL, 0xF100000000000000UL, 
0xE0A0E00000000000UL, 0x004040F14040F100UL, 0xE080E020E0000000UL, 0xE080E080E0000000UL, 0x8040000000000000UL, 0x0000009090907110UL, 0xE171716141416100UL, 0x0000006060000000UL, 0x0000000000408060UL, 0x604040E000000000UL, 0xE01111E000F10000UL, 0x0050A041A0500000UL, 0x1090502051C10100UL, 0x109050A111808100UL, 0x3021B06071C10100UL, 0x400040201011E000UL, 
0x2040E011F1111100UL, 0x8040E011F1111100UL, 0x40A0E011F1111100UL, 0xA050E011F1111100UL, 0xA000E011F1111100UL, 0xE0A0E011F1111100UL, 0xA15050F15050D100UL, 0xE011101011E08060UL, 0x2040F110F010F100UL, 0x8040F110F010F100UL, 0x40A0F110F010F100UL, 0xA000F110F010F100UL, 0x2040E0404040E000UL, 0x8040E0404040E000UL, 0x40A0E0404040E000UL, 0xA000E0404040E000UL, 
0x60A0217121A06000UL, 0x41A0113151911100UL, 0x2040E0111111E000UL, 0x8040E0111111E000UL, 0x40A0E0111111E000UL, 0xA050E0111111E000UL, 0xA000E0111111E000UL, 0x0011A040A0110000UL, 0x01E0915131E01000UL, 0x204011111111E000UL, 0x804011111111E000UL, 0x40A000111111E000UL, 0xA00011111111E000UL, 0x804011A040404000UL, 0x7020E021E0207000UL, 0xE011F01111F01010UL, 
0x2040E001E111E100UL, 0x8040E001E111E100UL, 0x40A0E001E111E100UL, 0xA050E001E111E100UL, 0xA000E001E111E100UL, 0xE0A0E001E111E100UL, 0x0000B141F150F100UL, 0x0000E11010E18060UL, 0x2040E011F110E000UL, 0x8040E011F110E000UL, 0x40A0E011F110E000UL, 0xA000E011F110E000UL, 0x204000604040E000UL, 0x804000604040E000UL, 0x40A000604040E000UL, 0xA00000604040E000UL, 
0x80C180E090906000UL, 0x41A000F011111100UL, 0x204000E01111E000UL, 0x804000E01111E000UL, 0x40A000E01111E000UL, 0x41A000E01111E000UL, 0xA00000E01111E000UL, 0x004000F100400000UL, 0x000061905121D000UL, 0x204000111111E100UL, 0x804000111111E100UL, 0x40A000111111E100UL, 0xA00000111111E100UL, 0x8040001111E101E0UL, 0x0010709090701010UL, 0xA000001111E101E0UL };
unsafe void text(uint *fb, int32_t x, int32_t Y, int32_t w, string t, TPixel fg, TPixel bg)
{
    for (int i=0; i<t.Length; i++)
    {
        if (w<6) break;
        uint64_t ca= font8[(byte)(t[i])];
        w-= 6;
        for (int32_t y=8; --y>=0;)
        {
            fb[x+(Y+y)*lastSky.screenw]=   (ca&0x10)!=0?fg:bg; 
            fb[x+1+(Y+y)*lastSky.screenw]= (ca&0x20)!=0?fg:bg; 
            fb[x+2+(Y+y)*lastSky.screenw]= (ca&0x40)!=0?fg:bg; 
            fb[x+3+(Y+y)*lastSky.screenw]= (ca&0x80)!=0?fg:bg;
            fb[x+4+(Y+y)*lastSky.screenw]= (ca&0x1)!=0?fg:bg; 
            fb[x+5+(Y+y)*lastSky.screenw]= (ca&0x2)!=0?fg:bg;
            ca>>= 8;
        }
        x+= 6;
    }
    rect(fb, x, Y, x, 8, bg);
}
unsafe public void text(uint *fb, int32_t x, int32_t Y, int32_t w, string t, TPixel fg)
{
    for (int i=0; i<t.Length; i++)
    {
        if (w<6) break;
        uint64_t ca= font8[(byte)(t[i])];
        w-= 6;
        for (int32_t y=8; --y>=0;)
        {
            if ((ca&0x10)!=0) fb[x+0+(Y+y)*lastSky.screenw]= fg; 
            if ((ca&0x20)!=0) fb[x+1+(Y+y)*lastSky.screenw]= fg; 
            if ((ca&0x40)!=0) fb[x+2+(Y+y)*lastSky.screenw]= fg; 
            if ((ca&0x80)!=0) fb[x+3+(Y+y)*lastSky.screenw]= fg;
            if ((ca&0x1)!=0) fb[x+4+(Y+y)*lastSky.screenw]= fg; 
            if ((ca&0x2)!=0) fb[x+5+(Y+y)*lastSky.screenw]= fg;
            ca>>= 8;
        }
        x+= 6;
    }
}
unsafe public void textClip(uint *fb, int32_t x, int32_t Y, string t, TPixel fg)
{
    for (int i=0; i<t.Length; i++)
    {
        if (x>clipX-6) break;
        uint64_t ca= font8[(byte)(t[i])];
        for (int32_t y=8; --y>=0;)
        {
            if ((ca&0x10)!=0) pixel(fb, x+0, (Y+y), fg); 
            if ((ca&0x20)!=0) pixel(fb, x+1, (Y+y), fg); 
            if ((ca&0x40)!=0) pixel(fb, x+2, (Y+y), fg); 
            if ((ca&0x80)!=0) pixel(fb, x+3, (Y+y), fg);
            if ((ca&0x1)!=0) pixel(fb, x+4, (Y+y), fg); 
            if ((ca&0x2)!=0) pixel(fb, x+5, (Y+y), fg);
            ca>>= 8;
        }
        x+= 6;
    }
}


	int limMag=10; bool findLimMag=true;
	void resetlimMagFind() { limMag=10; findLimMag= true; }
	// This has do do with trying to easely locate objects that have been displayed. on each display, the location of items of interest will be save here...
	class TDisplayObjType { public int x, y; public uint index, type; };
    TDisplayObjType[] displayObjs= new TDisplayObjType[100];
	void addDspObj(int x, int y, uint index, uint type)
	{
		if (nbDisplayObjs==100) return;
        if (displayObjs[nbDisplayObjs]==null) displayObjs[nbDisplayObjs]= new TDisplayObjType();
		displayObjs[nbDisplayObjs].x= x; displayObjs[nbDisplayObjs].y= y;
		displayObjs[nbDisplayObjs].index= index; displayObjs[nbDisplayObjs++].type= type;
	}
	public string displayText=""; // text to display at the bottom of the screen
	int nbDisplayObjs=0;
	int penInitx=0, penInity=0; public bool penDrag=false, penDown=false;


        
public void penEvent(int penx, int peny, bool down) // generates clics and drags from HW pen readings...
{
    if (!down) { penDown= false; penDrag= false; return; } 
    if (!penDown) { penInitx= penx; penInity= peny; click(penx, peny); penDown= true; penDrag= false; }
    int32_t dx= penx-penInitx, dy= peny-penInity;
    if (penDrag) drag(penx, peny, dx, dy); 
    else {
        int32_t delta= dx*dx+dy*dy;
        if (delta>10*10) { penDrag= true; drag(penx, peny, dx, dy); }
    }
}

double radnorm(double a) { while (a<0.0f) a+= MPI*2.0f; while (a>=MPI*2.0f) a-= MPI*2.0f; return a; }

    double skydownra= 1000.0f, skydowndec;

bool XYToraDec(int32_t x, int32_t y, out double ra, out double dec)
{ // I tried using reverse math, but the to cartesian + rotation in one go can not/is hard to inverse... so, doing a 2 equation solve using raDecToXY. should be fast enough for governement purpose!
    ra= lastSky.skyra; dec= lastSky.skydec; double x1, y1; raDecToXY(ra, dec, out x1, out y1); // get default return...
    double step= fullSkyAngles[lastSky.viewAnglei]/100.0f;
    double d=100.0f;
    for (int i=0; i<200; i++)
    {
        if (d<6.0f) break; // close enough (2.5pixels)
        double ra2= radnorm(ra+step), dec2= radnorm(dec);      double x2, y2; raDecToXY(ra2, dec2, out x2, out y2); double d2= Math.Abs(x2-x)*Math.Abs(x2-x)+Math.Abs(y2-y)*Math.Abs(y2-y);
        double ra3= radnorm(ra-step), dec3= radnorm(dec);      double x3, y3; raDecToXY(ra3, dec3, out x3, out y3); double d3= Math.Abs(x3-x)*Math.Abs(x3-x)+Math.Abs(y3-y)*Math.Abs(y3-y);
        double ra4= radnorm(ra),      dec4= radnorm(dec+step); double x4, y4; raDecToXY(ra4, dec4, out x4, out y4); double d4= Math.Abs(x4-x)*Math.Abs(x4-x)+Math.Abs(y4-y)*Math.Abs(y4-y);
        double ra5= radnorm(ra),      dec5= radnorm(dec-step); double x5, y5; raDecToXY(ra5, dec5, out x5, out y5); double d5= Math.Abs(x5-x)*Math.Abs(x5-x)+Math.Abs(y5-y)*Math.Abs(y5-y);
             if (d2<=d3 && d2<=d4 && d2<=d5) { d= d2; ra= ra2; dec= dec2; x1= x2; y1= y2; continue; }
        else if (d3<=d2 && d3<=d4 && d3<=d5) { d= d3; ra= ra3; dec= dec3; x1= x3; y1= y3; continue; }
        else if (d4<=d2 && d4<=d3 && d4<=d5) { d= d4; ra= ra4; dec= dec4; x1= x4; y1= y4; continue; }
        else if (d5<=d2 && d5<=d3 && d5<=d4) { d= d5; ra= ra5; dec= dec5; x1= x5; y1= y5; continue; }
    }
    //qDebug() << "reverse lookup at x/y" << x << y << "found ra/dec" << todeg(ra) << todeg(dec) << "for x/y" << x1 << y1 << "d=" << d;
    return true;
}

void click(int32_t x, int32_t y)
{
    if (x==-1) return;
    XYToraDec(x, y, out skydownra, out skydowndec);
    displayText= "";
    int bd= 10000; TDisplayObjType d= null;
    for (int i=nbDisplayObjs; --i>=0;)
    {
        int D= (x-displayObjs[i].x)*(x-displayObjs[i].x)+(y-displayObjs[i].y)*(y-displayObjs[i].y);
        if (D<bd) { d= displayObjs[i]; bd= D; }
        //Debug.WriteLine("do:"+displayObjs[i].type.ToString()+" "+displayObjs[i].x.ToString()+"/"+displayObjs[i].y.ToString()+" d:"+D.ToString());
    }
    //Debug.WriteLine("click:"+x.ToString()+"/"+y.ToString()+" d:"+bd.ToString());
    if (bd>36) return;
    if (d.type==0)
    {  
        TStarMore sm= TStarMore.fromStarIndex((int)d.index); if (sm==null) return;
        displayText= sm.name+bayerGreek[sm.bayerGreek]+bayerConst[sm.bayerConst]+ " "+sm.dist.ToString() + "ly mag:"+sm.iabsMag.ToString();
        return;
    }
    TSkyCatalog sk= null; string name=""; char prefix=' ';
    if (d.type==1) { sk= messier[d.index]; name= sk.getString(ref CStars.messierStrings); prefix= 'M'; }
    if (d.type==2) { sk= caldwell[d.index]; name= sk.getString(ref CStars.cadwellStrings); prefix= 'C'; }
    double fp= sk.getSze(); string t1= fp.ToString();
    if (name=="")
        displayText= (prefix+d.index+1).ToString()+" "+sk.getDst()+"ly mag:"+sk.mag.ToString()+ t1+"min";
    else
        displayText= (prefix+d.index+1).ToString()+" "+name+" "+sk.getDst()+"ly mag:"+sk.mag.ToString()+ t1+"min";
}

void drag(int32_t px, int32_t py, int32_t dx, int32_t dy)
{
    if (skydownra<1000.0f)
        SkyForXYRaDec(px, py, skydownra, skydowndec);
}
    
byte [][] starPatterns= new byte[][]{
      new byte[] {31},
      new byte[] {0,9,0,9,31,9,0,9,0},
      new byte[] {0,16,0,16,31,16,0,16,0},
      new byte[] {0,24,0,24,31,24,0,24,0},
      new byte[] {0,31,0,31,31,31,0,31,0},
      new byte[] {9,31,9,31,31,31,9,31,9},
      new byte[] {16,31,16,31,31,31,16,31,16},
      new byte[] {24,31,24,31,31,31,24,31,24},
      new byte[] {31,31,31,31,31,31,31,31,31},
      new byte[] {0,3,3,3,0,3,31,31,31,3,3,31,31,31,3,3,31,31,31,3,0,3,3,3,0},
      new byte[] {0,6,6,6,0,6,31,31,31,6,6,31,31,31,6,6,31,31,31,6,0,6,6,6,0},
      new byte[] {0,8,8,8,0,8,31,31,31,8,8,31,31,31,8,8,31,31,31,8,0,8,8,8,0},
      new byte[] {0,11,11,11,0,11,31,31,31,11,11,31,31,31,11,11,31,31,31,11,0,11,11,11,0},
      new byte[] {0,14,14,14,0,14,31,31,31,14,14,31,31,31,14,14,31,31,31,14,0,14,14,14,0},
      new byte[] {0,16,16,16,0,16,31,31,31,16,16,31,31,31,16,16,31,31,31,16,0,16,16,16,0},
      new byte[] {0,19,19,19,0,19,31,31,31,19,19,31,31,31,19,19,31,31,31,19,0,19,19,19,0},
      new byte[] {0,21,21,21,0,21,31,31,31,21,21,31,31,31,21,21,31,31,31,21,0,21,21,21,0},
      new byte[] {0,24,24,24,0,24,31,31,31,24,24,31,31,31,24,24,31,31,31,24,0,24,24,24,0},
      new byte[] {0,26,26,26,0,26,31,31,31,26,26,31,31,31,26,26,31,31,31,26,0,26,26,26,0},
      new byte[] {0,29,29,29,0,29,31,31,31,29,29,31,31,31,29,29,31,31,31,29,0,29,29,29,0},
      new byte[] {0,31,31,31,0,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,0,31,31,31,0},
      new byte[] {0,0,3,3,3,0,0,0,3,31,31,31,3,0,3,31,31,31,31,31,3,3,31,31,31,31,31,3,3,31,31,31,31,31,3,0,3,31,31,31,3,0,0,0,3,3,3,0,0},
      new byte[] {0,0,4,4,4,0,0,0,4,31,31,31,4,0,4,31,31,31,31,31,4,4,31,31,31,31,31,4,4,31,31,31,31,31,4,0,4,31,31,31,4,0,0,0,4,4,4,0,0},
      new byte[] {0,0,6,6,6,0,0,0,6,31,31,31,6,0,6,31,31,31,31,31,6,6,31,31,31,31,31,6,6,31,31,31,31,31,6,0,6,31,31,31,6,0,0,0,6,6,6,0,0},
      new byte[] {0,0,8,8,8,0,0,0,8,31,31,31,8,0,8,31,31,31,31,31,8,8,31,31,31,31,31,8,8,31,31,31,31,31,8,0,8,31,31,31,8,0,0,0,8,8,8,0,0},
      new byte[] {0,0,10,10,10,0,0,0,10,31,31,31,10,0,10,31,31,31,31,31,10,10,31,31,31,31,31,10,10,31,31,31,31,31,10,0,10,31,31,31,10,0,0,0,10,10,10,0,0},
      new byte[] {0,0,12,12,12,0,0,0,12,31,31,31,12,0,12,31,31,31,31,31,12,12,31,31,31,31,31,12,12,31,31,31,31,31,12,0,12,31,31,31,12,0,0,0,12,12,12,0,0},
      new byte[] {0,0,14,14,14,0,0,0,14,31,31,31,14,0,14,31,31,31,31,31,14,14,31,31,31,31,31,14,14,31,31,31,31,31,14,0,14,31,31,31,14,0,0,0,14,14,14,0,0},
      new byte[] {0,0,16,16,16,0,0,0,16,31,31,31,16,0,16,31,31,31,31,31,16,16,31,31,31,31,31,16,16,31,31,31,31,31,16,0,16,31,31,31,16,0,0,0,16,16,16,0,0},
      new byte[] {0,0,18,18,18,0,0,0,18,31,31,31,18,0,18,31,31,31,31,31,18,18,31,31,31,31,31,18,18,31,31,31,31,31,18,0,18,31,31,31,18,0,0,0,18,18,18,0,0},
      new byte[] {0,0,20,20,20,0,0,0,20,31,31,31,20,0,20,31,31,31,31,31,20,20,31,31,31,31,31,20,20,31,31,31,31,31,20,0,20,31,31,31,20,0,0,0,20,20,20,0,0},
      new byte[] {0,0,22,22,22,0,0,0,22,31,31,31,22,0,22,31,31,31,31,31,22,22,31,31,31,31,31,22,22,31,31,31,31,31,22,0,22,31,31,31,22,0,0,0,22,22,22,0,0}
};

    bool raDecToXY(double ra, double dec, out double x, out double y)
{
    // to cartesian with rotation by a and d...
    x= 0; y= 0; 
    double fx= lastSky.cosSkyd*Math.Cos(ra-lastSky.skyra)*Math.Cos(dec) + lastSky.sinSkyd*Math.Sin(dec);
    if (fx<=0.1f) return false;
    double fy= Math.Sin(ra-lastSky.skyra)*Math.Cos(dec) / fx;
    double fz= (-lastSky.sinSkyd*Math.Cos(ra-lastSky.skyra)*Math.Cos(dec) + lastSky.cosSkyd*Math.Sin(dec)) / fx;
    // x/y/z are now on the radius 1 sphere...
    // rotation of the screen/camera
    double rx= fy*lastSky.cosSkyrot-fz*lastSky.sinSkyrot, ry= fy*lastSky.sinSkyrot+fz*lastSky.cosSkyrot;
    // projection (scaling really)...
    rx*= lastSky.scale; ry*= lastSky.scale;
    if (Math.Abs(rx)>1024.0f || Math.Abs(ry)>1024.0f) return false; // clipping
    // and finally, shift to position
    x= lastSky.x+lastSky.w/2+rx; y= lastSky.y+lastSky.h/2-ry;
    return true;
}
    bool raDecToXY(double ra, double dec, out int x, out int y)
    {
        double fx, fy;
        bool ret= raDecToXY(ra, dec, out fx, out fy);
        x= (int)fx; y= (int)fy;
        return ret;
    }

static double[] fullSkyAngles= new double[] { dtr(50), dtr(40), dtr(30), dtr(20), dtr(10), dtr(5), dtr(2), dtr(1) };

// What sky point do we need to aim to so that the given ra/dec point to a given pixel..
bool SkyForXYRaDec(int32_t x, int32_t y, double ra, double dec)
{
    double fx1, fy1; if (!raDecToXY(ra, dec, out fx1, out fy1)) return false;
    int x1= (int)fx1, y1= (int)fy1;
    if (abs(x1-x)*abs(x1-x)+abs(y1-y)*abs(y1-y)<6.0f) return true; // good enough!
    double step= fullSkyAngles[lastSky.viewAnglei]/100.0f;
    double d=100.0f;
    for (int i=0; i<200; i++)
    {
        if (d<6.0f) break; // close enough (2.5pixels)
        double tskyra= lastSky.skyra, tskydec= lastSky.skydec;
        lastSky.skyra= tskyra+step; lastSky.skydec = tskydec;      lastSky.sinSkyd= Math.Sin(lastSky.skydec); lastSky.cosSkyd= Math.Cos(lastSky.skydec); double x2, y2; raDecToXY(ra, dec, out x2, out y2); double d2= Math.Abs(x2-x)*Math.Abs(x2-x)+Math.Abs(y2-y)*Math.Abs(y2-y);
        lastSky.skyra= tskyra-step; lastSky.skydec= tskydec;      lastSky.sinSkyd= Math.Sin(lastSky.skydec); lastSky.cosSkyd= Math.Cos(lastSky.skydec); double x3, y3; raDecToXY(ra, dec, out x3, out y3); double d3= Math.Abs(x3-x)*Math.Abs(x3-x)+Math.Abs(y3-y)*Math.Abs(y3-y);
        lastSky.skyra= tskyra;      lastSky.skydec= tskydec+step; lastSky.sinSkyd= Math.Sin(lastSky.skydec); lastSky.cosSkyd= Math.Cos(lastSky.skydec); double x4, y4; raDecToXY(ra, dec, out x4, out y4); double d4= Math.Abs(x4-x)*Math.Abs(x4-x)+Math.Abs(y4-y)*Math.Abs(y4-y);
        lastSky.skyra= tskyra;      lastSky.skydec= tskydec-step; lastSky.sinSkyd= Math.Sin(lastSky.skydec); lastSky.cosSkyd= Math.Cos(lastSky.skydec); double x5, y5; raDecToXY(ra, dec, out x5, out y5); double d5= Math.Abs(x5-x)*Math.Abs(x5-x)+Math.Abs(y5-y)*Math.Abs(y5-y);
        if (d2<=d3 && d2<=d4 && d2<=d5)      { d= d2; lastSky.skyra= tskyra+step; lastSky.skydec= tskydec;      continue; }
        else if (d3<=d2 && d3<=d4 && d3<=d5) { d= d3; lastSky.skyra= tskyra-step; lastSky.skydec= tskydec;      continue; }
        else if (d4<=d2 && d4<=d3 && d4<=d5) { d= d4; lastSky.skyra= tskyra;      lastSky.skydec= tskydec+step; continue; }
        else if (d5<=d2 && d5<=d3 && d5<=d4) { d= d5; lastSky.skyra= tskyra;      lastSky.skydec= tskydec-step; continue; }
    }
    lastSky.skyra= radnorm(lastSky.skyra); lastSky.skydec= radnorm(lastSky.skydec); lastSky.sinSkyd= Math.Sin(lastSky.skydec); lastSky.cosSkyd= Math.Cos(lastSky.skydec);
    //raDecToXY(ra, dec, x1, y1); qDebug() << "reverse lookup2 want ra/dec at x/y" << todeg(ra) << todeg(dec) << x << y << "found skyra/dec" << todeg(skyra) << todeg(skydec) << "for x/y d" << x1 << y1 << d;
    return true;
}

class TLastSky { public int x, y, w, h, viewAnglei, screenw; public double skyra, skydec, skyrot, scale, cosSkyd, sinSkyd, cosSkyrot, sinSkyrot; }; TLastSky lastSky= new TLastSky();

unsafe public void drawSky(uint *fb, int32_t x, int32_t y, int32_t w, int32_t h, int screenw, double skyra, double skydec, double skyrot, int viewAnglei)
{
    skyra= -skyra*MPI/12.0f; skydec= skydec*MPI/180.0f;
    double viewAngler= fullSkyAngles[viewAnglei];
   
    if (lastSky.skyra!=skyra) resetlimMagFind();
    if (lastSky.skydec!=skydec) resetlimMagFind();
    if (lastSky.skyrot!=skyrot) resetlimMagFind();
    if (lastSky.viewAnglei!=viewAnglei) resetlimMagFind();
    lastSky.skyra=skyra; lastSky.skydec=skydec; lastSky.skyrot=skyrot; lastSky.viewAnglei=viewAnglei;
    lastSky.scale= w/(2.0f*Math.Tan(viewAngler/2.0f)); 
    lastSky.cosSkyrot= Math.Cos(skyrot); lastSky.sinSkyrot= Math.Sin(skyrot);
    drawSky(fb, x, y, w, h, screenw);
}

unsafe public void drawSky(uint *fb, int32_t x, int32_t y, int32_t w, int32_t h, int screenw)
{ 
    nbDisplayObjs= 0;
    double viewAngler= fullSkyAngles[lastSky.viewAnglei];
    lastSky.sinSkyd= Math.Sin(lastSky.skydec); lastSky.cosSkyd= Math.Cos(lastSky.skydec);
    lastSky.screenw= screenw;
    lastSky.x= x; lastSky.y= y; lastSky.w= w; lastSky.h= h; 
    // skyra, double skydec, double skyrot

    int sl= 0;

    int32_t[] clip= new int32_t[4]; saveClip(ref clip, x, y, x+w, y+h);
    fillRect(fb, x, y, w, h, 0);

    int drawnStars= 0;
    for (uint32_t i=0; i<nbStars; i++)
    {
        while (i>CStars.starLink[sl].first) sl++;
        int32_t mag= stars[i].imag;
        double dec= stars[i].dec()-MPI/2.0f, ra= -stars[i].ra();
        // mag=10 means mag 3. mag=12.mag=4 and then 2 for 1...
        if (stars[i].hasMoreInfo==0 && i!=CStars.starLink[sl].first)
        {
            if (mag>limMag) continue;
            // add dec based filtering!
        }
        double fsx, fsy; if (!raDecToXY(ra, dec, out fsx, out fsy)) continue; int32_t sx= (int32_t)(fsx), sy= (int32_t)(fsy); 
        if (lightUp(fb, sx, sy, ref starPatterns[31-mag], 7))
        {
            drawnStars++;
            if (stars[i].hasMoreInfo!=0) 
                        addDspObj(sx, sy, i, 0);
        }
        while (CStars.starLink[sl].first==i)
        {
            uint32_t s2= (uint32_t)(i+CStars.starLink[sl++].delta); double dec2= stars[s2].dec()-MPI/2.0f, ra2= -stars[s2].ra(); double sx2, sy2; if (!raDecToXY(ra2, dec2, out sx2, out sy2)) continue;
            line(fb, fsx, fsy, sx2, sy2, 0xffff);
        }
    }
    if (findLimMag){ if (drawnStars<50 && limMag<31) limMag++; else findLimMag= false; }

    //Debug.WriteLine("displayed stars "+nbDisplayObjs.ToString());

    // Draw messier
    for (uint32_t i=0; i<messier.Length; i++)
    {
        double dec= messier[i].getdec(), ra= -messier[i].getra();
        int32_t sx, sy; if (!raDecToXY(ra, dec, out sx, out sy)) continue;
        int mag= 0;
        if (lightUp(fb, sx, sy, ref starPatterns[31-mag], 3))
          addDspObj(sx, sy, i, 1);
    }
    // Draw cadwell
    for (uint32_t i=0; i<caldwell.Length; i++)
    {
        double dec= caldwell[i].getdec(), ra= -caldwell[i].getra();
        int32_t sx, sy; if (!raDecToXY(ra, dec, out sx, out sy)) continue;
        int mag= 0;
        if (lightUp(fb, sx, sy, ref starPatterns[31-mag], 2))
          addDspObj(sx, sy, i, 2);
    }

    double[] angleStepsC= new double[] {dtr(5), dtr(5), dtr(5), dtr(5), dtr(2), dtr(1), dtr(0.5), dtr(0.1)};
    double angleStepsd= angleStepsC[lastSky.viewAnglei];
    // depending on FOV and dec, choose the proper ra stepping > angleStepsd...
    // if dec close to pi/2, then we can use the same step as for dec...
    // as it gets higher, and closer to zenith (dec=0 or PI), then we need to decreese the number of radians...
    // 2*tan(fov/2) is width of fov
    // 2*tan(rasteps/2)*sin(dec) is width of 1 fuseau
    // we want at most N fuseaux on the screen, so if f*N>fov, make fuseau bigger... (avoids /0)
    double[] angleStepsC2={dtr(0.1), dtr(0.5), dtr(1), dtr(2), dtr(5), dtr(10), dtr(20), dtr(30)};
    double angleStepsra= dtr(2);
    for (int i=0; i<8; i++)
    {
        angleStepsra= angleStepsC2[i]; if (angleStepsra<angleStepsd) continue;
        double fov= Math.Abs(2.0f*Math.Tan(viewAngler/2.0f)), fuseau= Math.Abs(2.0f*Math.Tan(angleStepsra/2.0f)*Math.Cos(lastSky.skydec));
        if (fuseau*10.0f>fov) break;
    }
    double mul= 1.0f;
    double ras= lastSky.skyra-viewAngler*mul-Math.IEEERemainder(lastSky.skyra-viewAngler*mul, angleStepsra);
    double decs= lastSky.skydec-viewAngler*mul-Math.IEEERemainder(lastSky.skydec-viewAngler*mul, angleStepsd);
    for (int i=0; i<20; i++)
    {
        if (i*angleStepsra>MPI) break;
        double x1, y1; bool b1= raDecToXY(ras+i*angleStepsra, decs, out x1, out y1);
        for (int j=1; j<20; j++)
        {
            double x2, y2; bool b2= raDecToXY(ras+i*angleStepsra, decs+j*angleStepsd, out x2, out y2);
            if (b1&&b2) line(fb, x1, y1, x2, y2, 31<<(5+6)); x1= x2; y1= y2; b1= b2;
        }
    }
    for (int i=0; i<20; i++)
    {
        double x1, y1; bool b1= raDecToXY(ras, i*angleStepsd+decs, out x1, out y1);
        for (int j=1; j<20; j++)
        {
            if (j*angleStepsra>MPI) break;
            double x2, y2; bool b2= raDecToXY(ras+j*angleStepsra, decs+i*angleStepsd, out x2, out y2);
            if (b1&&b2) line(fb, x1, y1, x2, y2, 31<<(5+6)); x1= x2; y1= y2; b1= b2;
        }
    }
    restoreClip(ref clip);
}

double calc_rad(double mag)
{
    if (mag >= 12) return .5;
    if (mag >= 10) return .5;
    if (mag >= 9 ) return .5;
    if (mag >= 8 ) return 1 ;
    if (mag >= 7 ) return 1 ;
    if (mag >= 6 ) return 2 ;
    if (mag >= 5 ) return 2 ;
    if (mag >= 4 ) return 3 ;
    if (mag >= 3 ) return 4 ;
    if (mag >= 2 ) return 5 ;
    if (mag >= 1 ) return 6 ;
    if (mag >= 0 ) return 7 ;
    if (mag >= -1) return 8 ;
    return 10;
}


//////////////////////////////////////////////
// Planetary stuff
double timeFromDate(int y, int m, int d, int h, int M, int s)
{
    return (double)(367*y - 7 * ( y + (m+9)/12 ) / 4 + 275*m/9 + d - 730530) + (h+M/60.0f+s/3600.0f)/24.0f;
}

//  N = longitude of the ascending node
//  i = inclination to the ecliptic (plane of the Earth's orbit)
//  w = argument of perihelion
//  a = semi-major axis, or mean distance from Sun
//  e = eccentricity (0=circle, 0-1=ellipse, 1=parabola)
//  M = mean anomaly (0 at perihelion; increases uniformly with time)
class TOrbitalElements { 
        public double N1, N2, i1, i2, w1, w2, a1, a2, e1, e2, M1, M2; 
        public TOrbitalElements(double N1,double  N2,double  i1,double  i2,double  w1,double  w2,double  a1, double a2, double e1, double e2, double M1, double M2)
        { 
            this.N1= N1;
            this.N2= N2;
            this.i1= i1;
            this.i2= i2;
            this.w1= w1;
            this.w2= w2;
            this.a1= a1;
            this.a2= a2;
            this.e1= e1;
            this.e2= e2;
            this.M1= M1;
            this.M2= M2;
        }
    };
class TOrbitalElementsAt { 
    public double N, i, w, a, e, M; 
    public TOrbitalElementsAt(ref TOrbitalElements o, double d)
    { 
      N= o.N1+o.N2*d; i=o.i1+o.i2*d; w= o.w1+o.w2*d;
      a= o.a1+o.a2*d; e= o.e1+o.e2*d; M= o.M1+o.M1*d;
    }
};
TOrbitalElements[] oe= new TOrbitalElements[] {
    // N                    i                  w                     a                   e                    M
    new TOrbitalElements (0f, 0f,                  0f, 0f,              282.9404f, 4.70935E-5f, 1f, 0f,               0.016709f, -1.151E-9f, 356.0470f, 0.9856002585f), // sun
    new TOrbitalElements (48.3313f, 3.24587E-5f,   7.0047f, 5.00E-8f,   29.1241f, 1.01444E-5f,  0.387098f,0f,         0.205635f, 5.59E-10f,  168.6562f, 4.0923344368f), // mercury
    new TOrbitalElements (76.6799f, 2.46590E-5f,   3.3946f, 2.75E-8f,   54.8910f, 1.38374E-5f,  0.723330f,0f,         0.006773f, 1.302E-9f,  48.0052f, 1.6021302244f), // Venus
    new TOrbitalElements ( 49.5574f, 2.11081E-5f,  1.8497f, -1.78E-8f,  286.5016f, 2.92961E-5f, 1.523688f,0f,         0.093405f, 2.516E-9f,  18.6021f, 0.5240207766f), // Mars
    new TOrbitalElements ( 100.4542f, 2.76854E-5f, 1.3030f, -1.557E-7f, 273.8777f, 1.64505E-5f, 5.20256f,0f,          0.048498f, 4.469E-9f,  19.8950f, 0.0830853001f), // Jupiter
    new TOrbitalElements ( 113.6634f, 2.38980E-5f, 2.4886f, -1.081E-7f, 339.3939f, 2.97661E-5f, 9.55475f,0f,          0.055546f, -9.499E-9f, 316.9670f, 0.0334442282f), // Saturne
    new TOrbitalElements ( 74.0005f, 1.3978E-5f,   0.7733f, 1.9E-8f,    96.6612f, 3.0565E-5f,   19.18171f, -1.55E-8f, 0.047318f, 7.45E-9f,   142.5905f, 0.011725806f), // Uranus
    new TOrbitalElements ( 131.7806f, 3.0173E-5f,  1.7700f, -2.55E-7f,  272.8461f, -6.027E-6f,  30.05826f, 3.313E-8f, 0.008606f, 2.15E-9f,   260.2471f, 0.005995147f) // Nepture
};

void sunPos(double d, ref double RA, ref double Dec)
{
    double ecl= 23.4393 - 3.563E-7 * d; // ecl angle
    TOrbitalElementsAt sun= new TOrbitalElementsAt(ref oe[0], d);
    double E= sun.M+sun.e*(180.0f/MPI)*Math.Sin(sun.M*degToRad)*(1.0+sun.e*Math.Cos(sun.M*degToRad)); //ccentric anomaly E from the mean anomaly M and from the eccentricity e (E and M in degrees):
    // compute the Sun's distance r and its true anomaly v from:
    double xv = Math.Cos(E)-sun.e;
    double yv = Math.Sqrt(1.0f-sun.e*sun.e) * Math.Sin(E);
    double v = Math.Atan2( yv, xv );
    double rs = Math.Sqrt( xv*xv + yv*yv );
    //compute the Sun's true longitude
    double lonsun = v + sun.w*degToRad;
    // Convert lonsun,r to ecliptic rectangular geocentric coordinates xs,ys:
    double xs = rs * Math.Cos(lonsun);
    double ys = rs * Math.Sin(lonsun);
    // (since the Sun always is in the ecliptic plane, zs is of course zero). xs,ys is the Sun's position in a coordinate system in the plane of the ecliptic. To convert this to equatorial, rectangular, geocentric coordinates, compute:
    double xe = xs;
    double ye = ys * Math.Cos(ecl);
    double ze = ys * Math.Sin(ecl);
    // Finally, compute the Sun's Right Ascension (RA) and Declination (Dec):
    RA  = Math.Atan2( ye, xe );
    Dec = Math.Atan2( ze, Math.Sqrt(xe*xe+ye*ye) );
}
void objPos(double d, int ob, ref double x, ref double y, ref double z)
{
    TOrbitalElementsAt o= new TOrbitalElementsAt(ref oe[ob], d);
    double E= (o.M+o.e)*degToRad*Math.Sin(o.M*degToRad)*(1.0+o.e*Math.Cos(o.M*degToRad)); //ccentric anomaly E from the mean anomaly M and from the eccentricity e (E and M in degrees):
    for (int i=0; i<5; i++)
        E= E - ( E - o.e * Math.Sin(E) - o.M*degToRad ) / ( 1 - o.e * Math.Cos(E) );
    double xv = o.a * ( Math.Cos(E) - o.e );
    double yv = o.a * ( Math.Sqrt(1.0 - o.e*o.e) * Math.Sin(E) );
    double v = Math.Atan2( yv, xv );
    double r = Math.Sqrt( xv*xv + yv*yv );
    x = r * ( Math.Cos(o.N*degToRad) * Math.Cos(v+o.w*degToRad) - Math.Sin(o.N*degToRad) * Math.Sin(v+o.w*degToRad) * Math.Cos(o.i*degToRad) );
    y = r * ( Math.Sin(o.N*degToRad) * Math.Cos(v+o.w*degToRad) + Math.Cos(o.N*degToRad) * Math.Sin(v+o.w*degToRad) * Math.Cos(o.i*degToRad) );
    z = r * ( Math.Sin(v+o.w*degToRad) * Math.Sin(o.i*degToRad) );
}
        //    Related orbital elements are:
        //
        //  w1 = N + w   = longitude of perihelion
        //  L  = M + w1  = mean longitude
        //  q  = a*(1-e) = perihelion distance
        //  Q  = a*(1+e) = aphelion distance
        //  P  = a ^ 1.5 = orbital period (years if a is in AU, astronomical units)
        //  T  = Epoch_of_M - (M(deg)/360_deg) / P  = time of perihelion
        //  v  = true anomaly (angle between position and perihelion)
        //  E  = eccentric anomaly
        //  AU=149.6 million km

};
};