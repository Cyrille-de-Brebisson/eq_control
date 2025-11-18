#ifndef _MINIFB_H_
#define _MINIFB_H_

#include <stdint.h>
#include <windows.h>
#include <malloc.h>
#include <string.h>

//////////////////////////////////////////////////////////////////////////////////////
// CSimpleFrameBuffer is a very simple graphical class that is designed to handle
// simple framebuffers and bitmaps.
// 
// This code it not intended to be fast, just to be small, independent and to work!
//
// It supports creation of a bitmap either as a memory based item or to use an existing framebuffer (direct to screen for example)
//
// It supports the following graphical operations:
// Pixel operations (pixel), filled rectangle (rect), Horizontal and Vertical lines (HLine & VLine)
// writing text using a 6*8 font (with zoom factor)
// bliting of a graphic object on another (blit)
// 
// Graphic operations support trensparency (encoded in the color, 0 is opaque, 255 trensparent)
// In the case of blits, an extra trensparency affecting the whole source bitmaps can be used
// 
// You can use the Color function to generate a color (with transparency)
//
// Assuming that the screen is 320*240 and that ScreenPointer is a pointer to the screen framebuffer,
// you can cerate a CSimpleFrameBuffer representing the screen by doing:
// CSimpleFrameBuffer screen(320, 240, (TSFBColor*)ScreenPointer);
// You can then use all the drawing primitives to draw on the screen.
//
// If you want do work 'off screen' to avoid flickering, then create a 2nd framebuffer to work on
// CSimpleFrameBuffer Offscreen(screen.w, screen.h);
// Do the drawing in Offscreen and call screen.blit(Offscreen); once finish to copy the offscreen buffer to the screen

class CSimpleFrameBuffer
{ public:
    // Color manipulation helpers
    typedef uint32_t TSFBColor; // color contains alpha channel! 0 is opaque, 255 trensparent
    static TSFBColor inline rgb565toargb888(uint16_t c) { uint32_t b= c&0x1f, g= (c>>5)&0x3f, r= (c>>(5+6))&0x1f; return (r<<3)+(r>>2) + (((g<<2)+(g>>4))<<8) + (((b<<3)+(b>>2))<<16); }
    static TSFBColor inline rgb565toargb888(uint16_t c, uint32_t alpha) { return rgb565toargb888(c)+(alpha<<24); }
    static TSFBColor inline Color(int r, int g, int b, int a=0) { return (a<<24)+(r<<16)+(g<<8)+b; }
    static TSFBColor const ClTrensparent=  0xff000000;
    static TSFBColor const ClBlack=  0x000000;
    static TSFBColor const ClWhite=  0xffffff;
    static TSFBColor const ClRed=    0xff0000;
    static TSFBColor const ClGreen=  0x00ff00;
    static TSFBColor const ClBlue=   0x0000ff;
    static TSFBColor const ClCyan=   0x00ffff;
    static TSFBColor const ClYellow= 0xffff00;
    static TSFBColor const ClHPBlue= 0x0099D7;

    static TSFBColor Doalpha(TSFBColor c1, TSFBColor c2) // return c1 combined with c2 depending on c2's opacity
    {
        uint32_t a1= c2>>24; uint32_t a2= 255-a1;   // a1= contribution of col 1, a2= contribution of col 2
        if (a1==0) return c2; if (a2==0) return c1; // fast exits!
        uint32_t r1= c1&0xff, r2= c2&0xff, g1= (c1&0xff00)>>8, g2= (c2&0xff00)>>8, b1= (c1&0xff0000)>>16, b2= (c2&0xff0000)>>16; // extract the composants
        r1= (r1*a1+r2*a2)>>8; g1= (g1*a1+g2*a2)>>8; b1= (b1*a1+b2*a2)>>8; // calculate result
        return r1+(g1<<8)+(b1<<16); // return final
    }

    // Actuall bitmap stuff....
    int w, h;      // bitmap witdh and height, in case you need them...
    TSFBColor *fb; // bitmap framebuffer (freed at the end unless NoFreefb=true)
    bool NoFreefb; // if true, then does not free the fb at destruction of object
                   // Constructor for offscreen bitmap
    CSimpleFrameBuffer(int w, int h): w(w), h(h), fb((TSFBColor*)malloc(w*h*sizeof(TSFBColor))), NoFreefb(false), logstrs(NULL), printbuf(NULL) {}
    // Constructor when the object does not own the framebuffer. For example for the screen iteself!
    CSimpleFrameBuffer(int w, int h, TSFBColor *fb): w(w), h(h), fb(fb), NoFreefb(true), logstrs(NULL), printbuf(NULL) {}
    ~CSimpleFrameBuffer() { if (!NoFreefb) free(fb); free(logstrs); free(printbuf); }
    // If you ever change the size/framebuffer from outside, you can use this function
    template <typename T> void update(int w, int h, T *fb) { this->w= w; this->h= h; this->fb= (TSFBColor *)fb; }

    // Drawing primitives
    void inline pixel(int x, int y, TSFBColor color) { if (x>=0 && y>=0 && x<w && y<h) fb[x+y*w]= Doalpha(fb[x+y*w], color); } // set pixel. includes alpha
    TSFBColor inline pixel(int x, int y) const { return fb[x+y*w]; } // get pixel
    void vline(int x, int y, int h, TSFBColor color) { h= h+y; while (y<h) pixel(x, y++, color); }
    void hline(int x, int w, int y, TSFBColor color) { w= w+x; while (x<w) pixel(x++, y, color); }
    void line(int x1, int y1, int x2, int y2, TSFBColor color);
    void rect(int x, int y, int w, int h, TSFBColor color) { while (--h>=0) hline(x, w, y++, color); }
    struct TRect { 
        int x, y, w, h; 
        TRect(int x, int y, int w, int h): x(x), y(y), w(w), h(h) {} 
        bool contains(int X, int Y) { return X>=x && X<x+w && Y>=y && Y<y+h; }
    };
    void rect(TRect const &r, TSFBColor color) { rect(r.x, r.y, r.w, r.h, color); }
    void lrect(int x, int y, int w, int h, TSFBColor color) // border of a rectangle (4 lines)
    { 
        hline(x, w, y, color); hline(x, w, y+h-1, color);
        vline(x, y+1, h-2, color); vline(x+w-1, y+1, h-2, color); 
    }
    static unsigned long long const font8[]; // This is the 256 character, 6*8 font data. Each long long represents 1 character, with 1 bit per pixel and 8 bits per line
                                             // Write some text on the screen. size is a zoom factor...
    template <typename T> int text(int x, int y, int maxwidth, T const *t, TSFBColor color=ClBlack, TSFBColor eraseColor= ClWhite, int size=1)
    {
        while (t!=NULL && *t!='\0' && maxwidth>=6*size)                 // while there is enough room for one more character
        {
            unsigned long long ca= font8[*t++];           // get character pixmap
            for (int l=8; --l>=0;)                        // character is 8 pixel high
            { // display the 6 pixels for this line of the character
                rect(x+0*size, y+l*size, size, size, (ca&0x10)?color:eraseColor);
                rect(x+1*size, y+l*size, size, size, (ca&0x20)?color:eraseColor);
                rect(x+2*size, y+l*size, size, size, (ca&0x40)?color:eraseColor);
                rect(x+3*size, y+l*size, size, size, (ca&0x80)?color:eraseColor);
                rect(x+4*size, y+l*size, size, size, (ca&0x1)?color:eraseColor);
                rect(x+5*size, y+l*size, size, size, (ca&0x2)?color:eraseColor);
                ca>>= 8;                                                       // next line of the character
            }
            x+= size*6; maxwidth-= size*6;
        } // next character
          // we will now erase the end (from here to w)
        rect(x, y, maxwidth, size*8, eraseColor);
        return x;
    }
    // write center text
    template <typename T> int centertext(int x, int y, T const *t, TSFBColor color=ClBlack, TSFBColor eraseColor= ClWhite, int size=1)
    {
        int len= t==NULL ? 0 : (int(strlen(t))*6*size);
        return text(x-len/2, y, len, t, color, eraseColor, size);
    }
    // write right allign text
    template <typename T> int righttext(int x, int y, T const *t, TSFBColor color=ClBlack, TSFBColor eraseColor= ClWhite, int size=1)
    {
        int len= t==NULL ? 0 : (strlen(t)*6*size);
        return text(x-len, y, len, t, color, eraseColor, size);
    }
    // Draws s on bitmap at position. alpha can be provided
    void blit(int x, int y, CSimpleFrameBuffer const *s, uint32_t alpha=0) // 0 is opaque
    {
        int sw= (s->w+x<=w) ? w : w-x; // calculate the number of columns
        int sh= (s->h+y<=h) ? h : h-y; // and row to copy (with clipping)
        alpha<<= 24;
        for (int Y= 0; Y<sh; Y++) for (int X= 0; X<sw; X++) pixel(x+X, y+Y, alpha+(s->pixel(X, Y))&0xffffff); // copy bitmap!
    }
    // This is a fast framebuffer copy used to transfert a frame buffer from one bitmap to another of the same size. The intended use is for double buffering of screen data...
    void blit(CSimpleFrameBuffer const *s) { if (s->w==w || s->h==h) return; memcpy(fb, s->fb, w*h*sizeof(TSFBColor)); }

    // the print stuff is a sprintf which does not rely on an external lib....
    char *printbuf;
    char *print(char const *format, ...); // printing is limited to 1024 bytes and is placed in printbuf object global variable
    char *print(char const *format, va_list argp);  // printing is limited to 1024 bytes and is placed in printbuf object global variable
    char *logstrs;
    // This will write stuff on the screen like a terminal...
    void log(char const *format, ...);
    void log2(char const *format);
};
char *hpsprintf(char *buf, int bufsize, char const *format, ...);

template <typename T> T* ntrealloc(T* p, int s)
{
    if (s == 0 && p == nullptr) return nullptr;
    if (s == 0) { free(p); return nullptr; }
    if (p == nullptr) return (T*)malloc(s * sizeof(T));
    return (T*)realloc(p, s * sizeof(T));
}

template <typename T>
struct THPList
{
    T* list = nullptr; int Size = 0;            // list pointer and nb elements...

    bool inline IsEmpty() const { return !Size; }
    void SetSize(int new_size) { for (int i = new_size; i < Size; i++) list = ntrealloc(list, new_size); if (new_size > Size) memset(list + Size, 0, (new_size - Size) * sizeof(T)); Size = new_size; }

    int add(T const& o) { list = ntrealloc(list, Size + 1); list[Size] = o; Size++; return Size - 1; } // Add one object at the end. return it's index
    int add() { list = ntrealloc(list, Size + 1); memset(list + Size, 0, sizeof(T)); Size++; return Size - 1; } // add one slot at the end. zero it, return it's index
    int AddN(T const& o, int N) { int old_size = Size; list = ntrealloc(list, Size += N); while (N) list[Size - N--] = o; return old_size; } // Add N copies of o at the end. return index of first added; assumes N>=0
    int AddN(int N) { list = ntrealloc(list, Size += N); memset(list + Size - N, 0, N * sizeof(T)); return Size - N; } // Add N objects at the end. return index of first added; assumes N>=0
    int AddN(T const* o, int N) { list = ntrealloc(list, Size += N); memcpy(list + Size - N, o, N * sizeof(T)); return Size - N; } // Add N objects at the end. return index of first added; assumes N>=0
    int insert(int pos) { list = ntrealloc(list, ++Size); memmove(list + pos + 1, list + pos, sizeof(T) * (Size - 1 - pos)); memset(list + pos, 0, sizeof(T)); return pos; } // insert spot for element at pos. zero it out, return the index...
    int insert(T const& o, int pos) { list = ntrealloc(list, ++Size); memmove(list + pos + 1, list + pos, sizeof(T) * (Size - 1 - pos)); list[pos] = o; return pos; } // insert element at pos. return it's index
    int InsertN(int pos, T const& o, int N) { list = ntrealloc(list, Size += N); memmove(list + pos + N, list + pos, sizeof(T) * (Size - (pos + N))); while (N--) list[pos + N] = o; return pos; } // insert N copies of o starting at pos, return index of first copy; assumes N>=0
    void Delete(int pos) { memmove(list + pos, list + pos + 1, (--Size - pos) * sizeof(T)); list = ntrealloc(list, Size); } // remove an object at pos from list
    void RemoveN(int pos, int N) { Size -= N; memmove(list + pos, list + pos + N, (Size - pos) * sizeof(T)); } // removes N consecutive objects, starting at pos, from list; assumes N>=0
    int RemoveN(T* o, int pos, int N) { if (N + pos > Size) N = Size - pos; if (N <= 0) return 0; memcpy(o, list + pos, N * sizeof(T)); RemoveN(pos, N); return N; } // removes N consecutive objects, starting at pos, from list; assumes N>=0. returns the number of removed items.
    void swap(int i, int j) { if (i == j) return; SwapEm(list[i], list[j]); } // exchange 2 elements in the list
    void swap2(int i, int j) { if (i == j) return; SwapEm2(list[i], list[j]); } // exchange 2 elements in the list. Does not use Object copy, uses memory copy...
    void SwapN(int i, int j, int N) { if (i == j) return; while (N--) { SwapEm(list[i++], list[j++]); } } // exchange 2N elements in the list (i with j, i+1 with j+1, ...); assumes N>=0, assumes |i-j| is 0 or >=N (no partial overlap of ranges)
    void move(int src, int dst) {
        if (src == dst) return;
        T a = this->list[src];
        if (src < dst) memcpy(this->list + src, this->list + src + 1, sizeof(T) * (dst - src));
        else memmove(this->list + dst + 1, this->list + dst, sizeof(T) * (src - dst));
        this->list[dst] = a;
    } // move element from src to dst. move the list up or down as needed...
    T pop(int i = 1) { T ret = list[Size - 1]; list = ntrealloc(list, --Size); return ret; } // remove the last element from the list...

    T* TakeList() { T* ret = list; list = NULL; Size = 0; return ret; } // give away possession of the list content. remove it from *this, and reset *this
    void SetList(T* l, int size) { SetSize(0); this->list = l; this->Size = size; }
    void SetTo(int new_size, T* new_list) { SetSize(0); list = new_list; Size = new_size; }  // take possession of new_list; remove old contents of *this; *this takes on ownership of *new_list (responsibility to free(); assumes new_list is from the malloc heap)
    void TakeFrom(THPList<T>& take) { SetSize(0); this->Size = take.Size; this->list = take.TakeList(); } // Take the list from another list.

                                                                                                                                   // list access...
    T inline& at(int i) { return list[i]; }
    T inline& operator [](int i) { return list[i]; }
    T const inline& operator [](int i) const { return list[i]; }
    T inline& last() { return list[Size - 1]; }
    T const inline& last() const { return list[Size - 1]; }
    bool IsIndex(int i) { return 0 <= i && i < Size; }

    // template to get pointer to the object...
    template<typename T2> T2 inline* ptr(T2& obj) { return &obj; } //turn reference into pointer!
    template<typename T2> T2 inline* ptr(T2* obj) { return obj; }  //obj is already pointer, return it!

                                                                    // Find the index of an object. Object must had an id or a IsNameFor member for this to work!
    int find(T const& o) { for (int i = Size; --i >= 0;) if (list[i] == o) return i; return -1; }
    int find(int id) { for (int i = Size; --i >= 0;) if (ptr(list[i])->id == id) return i; return -1; }
    int findIsIdFor(int id) { for (int i = Size; --i >= 0;) if (ptr(list[i])->IsIdFor(id)) return i; return -1; } // Must define MatchIs
    int find(wchar_t const* name) { for (int i = Size; --i >= 0;) if (ptr(list[i])->IsNameFor(name)) return i; return -1; }
    int findWchar(wchar_t const* name) { for (int i = Size; --i >= 0;) if (wcscmp2(list[i], name) == 0) return i; return -1; }
    bool findO(int id, T& o) { int i = find(id);        if (i < 0) return false; o = list[i]; return true; }
    bool findO(wchar_t const* name, T& o) { int i = find(name);   if (i < 0) return false; o = list[i]; return true; }
    T findO(int id) { int i = find(id);        if (i < 0) return NULL; return list[i]; }
    T findOIsIdFor(int id) { int i = findIsIdFor(id); if (i < 0) return NULL; return list[i]; }
    T findO(wchar_t const* name) { int i = find(name);      if (i < 0) return NULL; return list[i]; }

    // find and remove object from list... see above for more info...
    bool FindDelete(T const& o) { int i = find(o);      if (i < 0) return false; Delete(i); return true; }
    bool FindDelete(wchar_t const* name) { int i = find(name);   if (i < 0) return false; Delete(i); return true; }
    bool FindDelete(int id) { int i = find(id);     if (i < 0) return false; Delete(i); return true; }
    int FindReplace(T const& o1, T const& o2) { int i = find(o1); if (i < 0) return add(o2); list[i] = o2; return i; }
    int FindReplace(wchar_t const* name, T const& o) { int i = find(name);   if (i < 0) return add(o); list[i] = o; return i; }
    int FindReplace(int id, T const& o) { int i = find(id); if (i < 0) return add(o); list[i] = o; return i; }
    int copy(THPList<T> const& l) { SetSize(l.Size); for (int i = l.Size; --i >= 0;) list[i] = l.list[i]; return Size; }
    int copy2(THPList<T> const& l) { SetSize(l.Size); memcpy(list, l.list, Size * sizeof(T)); return Size; }

    // Quick sort. THIS IS SELDOM TESTED, SO DO NOT TRUST IT AT THIS POINT!
    // compare returns <0 if o1 is < than o2. 0 if equal and >0 if larger
    // p is your own to deal with/use
    // set stop to true to stop the sorting. Will return true if this happend.
    template <typename T2> bool qsort(int (*compare)(T& o1, T& o2, T2 p, bool& stop), T2 p, int bottom = 0, int top = -1)
    {
        // Input validation
        if (bottom < 0) bottom = 0; if (bottom >= this->Size) bottom = this->Size - 1;
        bool stop = false;
        if (top == -1 || top >= this->Size) top = this->Size - 1;
        while (true) // Forever loop (does the terminal recursion part of the sort)
        {
            int n = top - bottom; // Deal with cases with 0, 1 or 2 items
            if (n <= 0) return false;
            if (n == 1) { if (compare(this->list[bottom], this->list[top], p, stop) > 0) this->swap2(bottom, top); return stop; }
            // Partition around the pivot... This assumes that a copy of T can be done easily...
            int mid = (bottom + top) / 2;
            int i = bottom - 1, j = top + 1;
            while (true)
            {
                while (compare(this->list[++i], this->list[mid], p, stop) < 0 && !stop); if (stop) return true;
                while (compare(this->list[--j], this->list[mid], p, stop) > 0 && !stop); if (stop) return true;
                if (i >= j) break;
                if (i != j) { SwapEm2(this->list[i], this->list[j]); if (i == mid) mid = j; else if (j == mid) mid = i; }
            }
            if (this->qsort<T2>(compare, p, bottom, j)) return true;
            bottom = i;
        }
    }
    bool inline qsort(int (*compare)(T& o1, T& o2, int unused), int bottom = 0, int top = -1) { return qsort<int>(compare, 0, bottom, top); }
};

enum {
    KB_MOD_SHIFT        = 0x0001,
    KB_MOD_CONTROL      = 0x0002,
    KB_MOD_ALT          = 0x0004,
    KB_MOD_SUPER        = 0x0008,
    KB_MOD_CAPS_LOCK    = 0x0010,
    KB_MOD_NUM_LOCK     = 0x0020
};

typedef enum {
    WF_RESIZABLE          = 0x01,
    WF_FULLSCREEN         = 0x02,
    WF_FULLSCREEN_DESKTOP = 0x04,
    WF_BORDERLESS         = 0x08,
    WF_ALWAYS_ON_TOP      = 0x10,
} mfb_window_flags;

class TThread { public:
    TThread()  { th= CreateThread(nullptr, 0, cb, this, CREATE_SUSPENDED, nullptr); }
    bool done= false;
    ~TThread()
    {
        TerminateThread(th, 0);
        WaitForSingleObject(th, 100);
        CloseHandle(th);
        done= true;
    }
    HANDLE th;
    static DWORD WINAPI cb(void *p)
    {
        TThread *This= (TThread*)p;
        This->doit();
        This->done= true;
        return 0;
    }
    virtual void doit() { }
    void resume() { ResumeThread(th); }
};

class CFBWindow : public CSimpleFrameBuffer { public:
    // Event callbacks
    virtual void active(bool isActive) {}
    virtual void resize(int width, int height) {}
    virtual bool close() { return true; } // return false to disallow closing!
    virtual void keyboard(int key, uint32_t mod, bool isPressed) {}
    virtual void char_input(unsigned int code) {}
    virtual void mouse_button(int button, uint32_t mod, bool isPressed) {}
    virtual void mouse_move(int x, int y) {}
    virtual void mouse_wheel(int deltav, int deltah, uint32_t mod) {}

    // Create a window that is used to display the buffer sent into the mfb_update function, returns 0 if fails
    CFBWindow(const wchar_t *title, int width, int height, unsigned flags= 0, int fbw= 0, int fbh= 0);
    // Close the window
    ~CFBWindow() { destroy_window_data(); windows.FindDelete(this);  }
    void destroy_window_data();
    
    bool update_display(); // Update the display
    bool update_events();  // update windows events

    // FPS
    int fps = 0, frameCount= 0;
    uint64_t nextFrame= 0;
    void setFps(int fps);
    bool wait_sync();

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    HWND                hWnd;
    WNDCLASS            wc;
    HDC                 hdc;
    struct { BITMAPINFO bitmapInfo; RGBQUAD other[2]; };
    long    s_window_style = WS_POPUP | WS_SYSMENU | WS_CAPTION;
    uint32_t window_width, window_height;
    int dst_offset_x, dst_offset_y, dst_width, dst_height;
    void set_viewport(int offset_x, int offset_y, int width, int height)
    { dst_offset_x= offset_x, dst_offset_y= offset_y, dst_width= width, dst_height= height; }
    void defaultViewport() { set_viewport(0, 0, window_width, window_height); }
    bool is_active= true, mouse_inside= true;
    bool closed= false;

    void loadFont(wchar_t const *n, int s1=8, int s2=13);

    void get_monitor_scale(float &scale_x, float &scale_y);

    virtual void exec() { }
    class ExecThread : public TThread { public:
        ExecThread(CFBWindow *w): w(w) { }
        CFBWindow *w;
        void doit() { w->exec(); }
    } execThread;

    static THPList<CFBWindow*> windows;
    static void run();
};



#endif
