// newScope.cpp : Defines the entry point for the application.
//

#include "..\main\system.h"

#include "framework.h"
#include "newScope.h"

#define MAX_LOADSTRING 100

#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#include <string>
#include <algorithm>
#include <cmath>
#include <vector>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

#undef max
#undef min

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
// Labels
HWND hLabel1 = nullptr;
HWND hLabel2 = nullptr;
HWND hLabel3 = nullptr;
HWND hLabelRaMoves = nullptr, hLabelDecMoves = nullptr;

// Scope image window and bitmap
HWND hScopeWnd = nullptr;
HBITMAP hScopeHbmOld = nullptr;

// Geometry image window and bitmap
HWND hGeometryWnd = nullptr;
HBITMAP hGeometryHbmOld = nullptr;

//screen window and bitmap
HWND hScreenWnd = nullptr;

// Camera controls (yaw, pitch, user scale)
std::atomic<double> g_camYaw{ -30.0 };
std::atomic<double> g_camPitch{ 25.0 };
std::atomic<double> g_camScale{ 1.0 };

// Camera control HWNDs
HWND hTrkYaw = nullptr;
HWND hTrkPitch = nullptr;
HWND hTrkScale = nullptr;
HWND hLblCamYaw = nullptr;
HWND hLblCamPitch = nullptr;
HWND hLblCamScale = nullptr;
// Scope control trackbars
HWND hTrkLat = nullptr;
HWND hTrkM1 = nullptr;
HWND hTrkM2 = nullptr;
HWND hLblLat = nullptr;
HWND hLblM1 = nullptr;
HWND hLblM2 = nullptr;

// GDI+ token
ULONG_PTR g_gdiplusToken = 0;

void DrawStats();
void DrawScreen()
{
    RECT rc;
    GetClientRect(hScreenWnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    using namespace Gdiplus;
    Bitmap bmp(w, h, PixelFormat32bppARGB);
    Graphics g(&bmp);
    Pen pen(Color(0,0,0));

    g.Clear(Color::White);
    for (int y=0; y<32; y++)
        for (int x=0; x<128; x++)
            if (display_pixel(x,y)) g.DrawRectangle(&pen, x*2, y*2, 2, 2);

    // Convert to HBITMAP and set into static control
    HBITMAP hBmp = NULL;
    bmp.GetHBITMAP(Color::Black, &hBmp);
    if (hBmp)
    {
        // replace previous
        HBITMAP old = (HBITMAP)SendMessageW(hScreenWnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmp);
        if (old) DeleteObject(old);
    }
}

// Update labels function
void DrawScopePos(double azimuth, double altitude);
void DrawTelescopeGeometry(double angle1_deg, double angle2_deg, double angle3_deg);

void UpdateLabels(HWND hWnd)
{
    wchar_t buf[64];
    swprintf_s(buf, L"lst:%.5f motors: %.2f/%.1f", scope.lst, scope.motor1angle, scope.motor2angle);
    SetWindowTextW(hLabel1, buf);
    swprintf_s(buf, L"actual ra/dec: %.1f/%.1f", scope.raActual, scope.decActual);
    SetWindowTextW(hLabel2, buf);
    swprintf_s(buf, L"desired ra/dec: %.1f/%.1f", scope.desiredRa, scope.desiredDec);
    SetWindowTextW(hLabel3, buf);

    swprintf_s(buf, L"RA: inc:%.1f Move:%.3f Track:%.3f Guide:%.3f Sid:%.3f", scope.incrementalRa, scope.raMoveRate, scope.raTrackRate, scope.raGuideRate, scope.raSideralRate);
    SetWindowTextW(hLabelRaMoves, buf);
    swprintf_s(buf, L"Dec: inc:%.1f Move:%.3f Track:%.3f Guide:%.3f", scope.incrementalDec, scope.decMoveRate, scope.decTrackRate, scope.decGuideRate);
    SetWindowTextW(hLabelDecMoves, buf);


    DrawScopePos(scope.az, scope.alt);
    DrawTelescopeGeometry(scope.latitude, scope.motor1angle, scope.motor2angle);
    DrawScreen();
    swprintf_s(buf, L"Lat: %0.0f\u00B0", scope.latitude); SetWindowTextW(hLblLat, buf);
    swprintf_s(buf, L"M1: %0.2f\u00B0", scope.motor1angle); SetWindowTextW(hLblM1, buf);
    swprintf_s(buf, L"M2: %0.2f\u00B0", scope.motor2angle); SetWindowTextW(hLblM2, buf);
    SendMessageW(hTrkLat, TBM_SETPOS, TRUE, (LPARAM)(int)(scope.latitude + 90.0));
    SendMessageW(hTrkM1, TBM_SETPOS, TRUE, (LPARAM)(int)(scope.motor1angle + 90.0));
    SendMessageW(hTrkM2, TBM_SETPOS, TRUE, (LPARAM)(int)(scope.motor2angle + 180.0));
    DrawStats();
}

TStat m2pos{"m2pos", "stp"}, m2dstPos{"m2dst", "stp"}, m2maxSpd{"m2maxSpd", "stp/s"}, m2requestedSpeed{"m2requestedSpeed", "stp/s"}, m2currentSpeed{"m2currentSpeed", "stp/s"};
TStat m1pos{"m1pos", "stp"}, m1dstPos{"m1dst", "stp"}, m1maxSpd{"m1maxSpd", "stp/s"}, m1requestedSpeed{"m1requestedSpeed", "stp/s"}, m1currentSpeed{"m1currentSpeed", "stp/s"};
TStat statincrementalDec{"iDec", "d"}, statdecMoveRate{"rDec", "d/s"}, statdesiredDec{"dDec", "d"};
TStat statincrementalRa{"iRa", "d"}, statRaMoveRate{"rRa", "d/s"}, statdesiredRa{"dRa", "d"};
CData stats;

// Draw a 2D projection of the 3D telescope geometry
void DrawTelescopeGeometry(double angle1_deg, double angle2_deg, double angle3_deg)
{
    if (!hGeometryWnd) return;
    RECT rc;
    GetClientRect(hGeometryWnd, &rc);
    int W = rc.right - rc.left;
    int H = rc.bottom - rc.top;
    if (W < 40 || H < 40) return;

    using namespace Gdiplus;
    Bitmap bmp(W, H, PixelFormat32bppARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.Clear(Color::White);

    // model sizes (meters scaled to pixels)
    float scale = std::min(W, H) * 0.0025f; // heuristic
    float L1 = 300.0f * scale; // primary cylinder length
    float L2 = 280.0f * scale; // cuboid length
    float L3 = 700.0f * scale; // secondary cylinder length
    float R = 60.0f * scale;   // cylinder radius
    float cubW = R * 1.0f, cubH = R * 1.0f;

    // helper: 3D vector
    struct V3 { float x,y,z; };
    auto vadd = [](V3 a, V3 b){ return V3{a.x+b.x,a.y+b.y,a.z+b.z}; };
    auto vsub = [](V3 a, V3 b){ return V3{a.x-b.x,a.y-b.y,a.z-b.z}; };
    auto vmul = [](V3 a, float s){ return V3{a.x*s,a.y*s,a.z*s}; };
    auto vlen = [](V3 a){ return sqrtf(a.x*a.x + a.y*a.y + a.z*a.z); };
    auto vnorm = [&](V3 a){ float l=vlen(a); if (l==0) return V3{0,0,0}; return V3{a.x/l,a.y/l,a.z/l}; };
    auto vcross = [](V3 a, V3 b){ return V3{a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; };
    auto vdot = [](V3 a, V3 b){ return a.x*b.x + a.y*b.y + a.z*b.z; };
    auto vrotx= [&](V3 v, float a) { return vnorm(V3{v.x, v.y*cosf(a)+v.z*sinf(a), v.y*sinf(a)-v.z*cosf(a)}); };
    auto vroty= [&](V3 v, float a) { return vnorm(V3{v.x*cosf(a)+v.z*sinf(a), v.y, v.x*sinf(a)-v.z*cosf(a)}); };
    auto vrotz= [&](V3 v, float a) { return vnorm(V3{v.x*cosf(a)+v.y*sinf(a), v.x*sinf(a)-v.y*cosf(a), v.z}); };
    auto vrot= [&](V3 v, float a, V3 vrot) {
        vrot= vnorm(vrot);
        // v= v*cos(a) + (vrot*v)*sin(a) + vrot*(vrot.v)*(1-cos(a)) 
        return vadd( vmul(v, cosf(a)), vadd(vmul(vcross(vrot, v), sin(a)), vmul(vrot, vdot(vrot, v)*(1-cosf(a)))));
        };
    auto vneg= [](V3 v) { return V3(-v.x, -v.y, -v.z); };

    // build axis A for cylinder1: axis tilted by angle1 relative to horizontal.
    // Here, Y is vertical, X is to the "right" and Z is depth....
    float a1 = (float)(angle1_deg * 3.14159265358979323846 / 180.0f);
    V3 A1 = vrotz({1,0,0}, a1);
    V3 A2 = vrotz({0,1,0}, a1);
    V3 A3 = vrotz({0,0,1}, a1);

    // world positions along axis: origin at center-left so geometry centered
    V3 start = V3{ 0, 0, 0 };
    V3 cyl1_start = start;
    V3 cyl1_end = vadd(cyl1_start, vmul(A1, L1));
    start.x= cyl1_start.x= -cyl1_end.x; cyl1_end.x= 0; // x center on end of az axis
    start.y= cyl1_start.y= -cyl1_end.y; cyl1_end.y= 0; // x center on end of az axis

    // view/projection: use a simple orthographic projection after rotating the model
    // camera parameters are user-controlled via g_camYaw/g_camPitch/g_camScale
    double viewYawDeg = g_camYaw.load();
    double viewPitchDeg = g_camPitch.load();
    float vy = (float)(viewYawDeg * 3.14159265358979323846 / 180.0);
    float vp = (float)(viewPitchDeg * 3.14159265358979323846 / 180.0);
    float cyv = cosf(vy), syv = sinf(vy);
    float cyp = cosf(vp), syp = sinf(vp);

    // compute a base scale so the whole model fits the control, then apply user scale
    float baseViewScale = float((std::min(W, H) * 0.8f) / (L1+L2+L3)*1.5); // estiation
    float viewScale = float(baseViewScale * g_camScale.load());

    auto project = [&](V3 p)->PointF
    {
        // yaw (around Y): rotate X/Z
        float x1 = p.x * cyv + p.z * syv;
        float z1 = -p.x * syv + p.z * cyv;
        // pitch (around X): rotate Y/Z
        float y1 = p.y * cyp - z1 * syp;
        // orthographic projection
        float sx = x1 * viewScale;
        float sy = y1 * viewScale;
        return PointF(W/2.0f + sx, H/2.0f - sy);
    };

    struct TFace { float z; std::vector<V3> pts3; Color brush; Color pen; 
      TFace(std::vector<V3> pts3, Color brush, Color pen): pts3(pts3), brush(brush), pen(pen)
      {
          z=0; for(auto p: pts3) z += p.z; z/= pts3.size();
      }
    };
    std::vector<TFace> Dfaces;

    // helper to draw a filled polygon from 3D points
    auto drawFace= [&](TFace &f)
    {
        // SolidBrush(Color) and Pen(Color, 1);
        std::vector<PointF> pts2; pts2.reserve(f.pts3.size());
        for (auto &p: f.pts3) pts2.push_back(project(p));
        SolidBrush b= SolidBrush(f.brush);
        g.FillPolygon(&b, pts2.data(), (INT)pts2.size());
        Pen p= Pen(f.pen, 1); // or should it be 1.5???
        g.DrawPolygon(&p, pts2.data(), (INT)pts2.size());
    };
    auto drawFilled3D = [&](const std::vector<V3> &pts3, const Color &brush, const Color &pen, bool imediate=false)
        {
            TFace f=TFace(pts3, brush, pen);
            if (imediate) drawFace(f); 
            else Dfaces.push_back(f);
        };

    Color pen= Color(255,0,0,0); //, 1.5f);
    Color gray= (Color(200,200,200,200));
    Color blue= (Color(180,180,255,255));
    Color light= (Color(220,220,220,255));

    // build circle points for cylinder ends
    int segs = 36;
    std::vector<V3> circ1_start, circ1_end, circ2_start, circ2_end;
    // For cylinder1, cross-section basis is u,v (perp to A)
    for (int i=0;i<segs;i++)
    {
        float t = (2.0f * 3.14159265358979323846f) * (float)i / (float)segs;
        V3 offset1 = vadd( vmul(A2, cosf(t)*R), vmul(A3, sinf(t)*R) );
        circ1_start.push_back( vadd(cyl1_start, offset1) );
        circ1_end.push_back( vadd(cyl1_end, offset1) );
    }

    // position ground so that cylinder1 lowest point touches it
    float groundY = cyl1_start.y; // ground at cylinder1 bottom
    float ext = L1 * 1.0f;
    std::vector<V3> groundQuad = { V3{-ext+start.x, groundY, -ext}, V3{ext+start.x, groundY, -ext}, V3{ext+start.x, groundY, ext}, V3{-ext+start.x, groundY, ext} };
    Color groundBrush= (Color(160, 34, 139, 34)); // semi-transparent forest green
    Color groundPen= (Color(200, 20, 120, 20));
    drawFilled3D(groundQuad, groundBrush, groundPen, true);
    // grid lines on ground for reference
    Pen gridGreen(Color(200, 20, 100, 20), 1.0f);
    int gridSteps = 8;
    for (int i=0;i<=gridSteps;i++)
    {
        float t = -ext + (2*ext) * (float)i / (float)gridSteps;
        // line parallel to X (vary Z)
        std::vector<V3> line1 = { V3{-ext+start.x, groundY, t}, V3{ext+start.x, groundY, t} };
        std::vector<V3> lpts1 = line1;
        // project and draw
        std::vector<PointF> l2(2);
        l2[0] = project(lpts1[0]); l2[1] = project(lpts1[1]);
        g.DrawLine(&gridGreen, l2[0], l2[1]);
        // line parallel to Z (vary X)
        std::vector<V3> line2 = { V3{t+start.x, groundY, -ext}, V3{t+start.x, groundY, ext} };
        std::vector<PointF> l22(2);
        l22[0] = project(line2[0]); l22[1] = project(line2[1]);
        g.DrawLine(&gridGreen, l22[0], l22[1]);
    }

    // draw cylinder (simple approach: draw side polygons and end caps)
    // cylinder1 side (as quad strips)
    for (int i=0;i<segs;i++)
    {
        int ni = (i+1)%segs;
        std::vector<V3> quad = { circ1_start[i], circ1_start[ni], circ1_end[ni], circ1_end[i] };
        drawFilled3D(quad, gray, pen);
    }
    // end caps
    drawFilled3D(circ1_start, blue, pen);
    drawFilled3D(circ1_end, blue, pen);


    // Cuboid: its touching face is perpendicular to A (face normal = A).
    // Cuboid long axis B lies in plane perpendicular to A and is rotated by angle2 around A.
    float a2 = (float)(angle2_deg * 3.14159265358979323846 / 180.0f);
    V3 B1= vrot(vneg(A2), a2, A1);
    V3 B2= vrot(A1, a2, A1);
    V3 B3= vrot(A3, a2, A1);

    V3 cub_start = vadd(cyl1_end, vmul(A1, cubW/2));
    cub_start= vsub(cub_start, vmul(B1, float(L2*0.4)));
    V3 cub_end = vadd(cub_start, vmul(B1, L2));

    // cuboid: construct 8 corners using basis {B (length), P (width), A (thickness)}
    V3 halfW = vmul(B2, cubW/2.0f);
    V3 halfH = vmul(B3, cubH/2.0f);
    std::vector<V3> corners(8);
    corners[0] = vadd(vadd(cub_start, halfW), halfH);
    corners[1] = vadd(vsub(cub_start, halfW), halfH);
    corners[2] = vsub(vsub(cub_start, halfW), halfH);
    corners[3] = vsub(vadd(cub_start, halfW), halfH);
    corners[4] = vadd(vadd(cub_end, halfW), halfH);
    corners[5] = vadd(vsub(cub_end, halfW), halfH);
    corners[6] = vsub(vsub(cub_end, halfW), halfH);
    corners[7] = vsub(vadd(cub_end, halfW), halfH);

    drawFilled3D({corners[0], corners[1],corners[2], corners[3]}, light, pen);
    drawFilled3D({corners[4], corners[5],corners[6], corners[7]}, light, pen);
    drawFilled3D({corners[0], corners[4],corners[7], corners[3]}, light, pen);
    drawFilled3D({corners[1], corners[5],corners[6], corners[2]}, light, pen);
    drawFilled3D({corners[0], corners[1],corners[5], corners[4]}, light, pen);
    drawFilled3D({corners[3], corners[2],corners[6], corners[7]}, light, pen);

    // Cylinder2 axis aligns with B (attached at cub_end)
    V3 cyl2_start = vadd(cub_end, vmul(B1, R)); // cylinder rotates on this point. Need to find it' asix which is 90) to B and has a rotation on it...
    // build orthonormal basis perpendicular to B: pick arbitrary up then project
    float a3 = (float)(angle3_deg * 3.14159265358979323846 / 180.0f);
    V3 C1= vrot(vneg(B2), a3, B1);
    V3 C2= vrot(B1, a3, B1);
    V3 C3= vrot(B3, a3, B1);
    cyl2_start= vadd(cyl2_start, vmul(C1, -L3/2));
    V3 cyl2_end = vadd(cyl2_start, vmul(C1, L3));

    // For cylinder2, need basis perpendicular to B
    for (int i=0;i<segs;i++)
    {
        float t = (2.0f * 3.14159265358979323846f) * (float)i / (float)segs;
        V3 offset2 = vadd( vmul(C2, cosf(t)*R), vmul(C3, sinf(t)*R) );
        circ2_start.push_back( vadd(cyl2_start, offset2) );
        circ2_end.push_back( vadd(cyl2_end, offset2) );
    }

    // cylinder2: same approach for sides and end caps
    for (int i=0;i<segs;i++)
    {
        int ni=(i+1)%segs;
        std::vector<V3> quad = { circ2_start[i], circ2_start[ni], circ2_end[ni], circ2_end[i] };
        drawFilled3D(quad, gray, pen);
    }
    drawFilled3D(circ2_start, Color(255,0,0), pen);
    drawFilled3D(circ2_end, Color(0,0,0), pen);


    // depth sort faces by average z
    std::sort(Dfaces.begin(), Dfaces.end(), [](const TFace &a, const TFace &b){ return a.z < b.z; });
    for (auto &f:Dfaces) drawFace(f);

    // // draw textual angles
    // FontFamily family(L"Segoe UI");
    // Font font(&family, 10.0f, FontStyleRegular, UnitPixel);
    // SolidBrush textBrush(Color(255,0,0,0));
    // wchar_t buf[128];
    // swprintf_s(buf, L"angle1: %0.1f\u00B0", angle1_deg);
    // g.DrawString(buf, -1, &font, PointF(6,6), &textBrush);
    // swprintf_s(buf, L"angle2: %0.1f\u00B0", angle2_deg);
    // g.DrawString(buf, -1, &font, PointF(6,24), &textBrush);
    // swprintf_s(buf, L"angle3: %0.1f\u00B0", angle3_deg);
    // g.DrawString(buf, -1, &font, PointF(6,42), &textBrush);

    // convert to HBITMAP and set into static control
    HBITMAP hBmp = NULL;
    bmp.GetHBITMAP(Color::White, &hBmp);
    if (hBmp)
    {
        HBITMAP old = (HBITMAP)SendMessageW(hGeometryWnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmp);
        if (old) DeleteObject(old);
        hGeometryHbmOld = hBmp;
    }
}

// Draw scope position using GDI+ (translated from provided C# code)
void DrawScopePos(double azimuth, double altitude)
{
    if (!hScopeWnd) return;
    RECT rc;
    GetClientRect(hScopeWnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w < 40 || h < 40) return;

    altitude = std::max(0.0, std::min(90.0, altitude));
    azimuth = fmod(azimuth, 360.0);
    if (azimuth < 0) azimuth += 360.0;

    float cx = w / 2.0f, cy = h / 2.0f;
    float R = std::min(w, h) / 2.0f - 16.0f;

    auto project = [cx, cy](float az, float r) -> Gdiplus::PointF {
        const bool EastLeft = true;
        double a = az * 3.14159265358979323846 / 180.0;
        float sign = EastLeft ? -1.0f : 1.0f;
        return Gdiplus::PointF(cx + sign * r * (float)sin(a), cy - r * (float)cos(a));
    };

    using namespace Gdiplus;
    Bitmap bmp(w, h, PixelFormat32bppARGB);
    Graphics g(&bmp);
    Pen gridPen(Color(255, 128, 128, 128), 1.0f);
    Pen crossPen(Color(80, 128, 128, 128), 1.0f);
    crossPen.SetDashStyle(DashStyleDot);
    Pen scopePen(Color(255, 255, 0, 0), 2.0f);
    SolidBrush scopeBrush(Color(100, 255, 0, 0));
    SolidBrush textBrush(Color(255, 211, 211, 211));
    FontFamily family(L"Segoe UI");
    Font font(&family, 8.0f, FontStyleRegular, UnitPixel);
    StringFormat centered;
    centered.SetAlignment(StringAlignmentCenter);
    centered.SetLineAlignment(StringAlignmentCenter);

    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.Clear(Color::Black);

    // Horizon and 45° circles
    g.DrawEllipse(&gridPen, cx - R, cy - R, 2*R, 2*R);
    g.DrawEllipse(&gridPen, cx - R/2, cy - R/2, R, R);

    // Cardinal cross
    g.DrawLine(&crossPen, cx - R, cy, cx + R, cy);
    g.DrawLine(&crossPen, cx, cy - R, cx, cy + R);

    // Cardinal labels
    const wchar_t* names[4] = { L"N", L"E", L"S", L"W" };
    for (int i=0;i<4;i++)
    {
        PointF p = project(i*90.0f, R + 9.0f);
        g.DrawString(names[i], -1, &font, p, &centered, &textBrush);
    }
    g.DrawString(L"45°", -1, &font, PointF(cx + 3, cy - R/2 - 12), &textBrush);

    float rr = R * (90.0f - (float)altitude) / 90.0f;
    PointF pt = project((float)azimuth, rr);

    float angle = rr < 0.5f ? 0.0f : (float)(atan2(pt.Y - cy, pt.X - cx) * 180.0 / 3.14159265358979323846);
    float minor = std::max(6.0f, R * 0.06f);
    float major = minor + R * 0.30f * (float)cos(altitude * 3.14159265358979323846 / 180.0);
    float t = minor; minor = major; major = t;

    g.TranslateTransform(pt.X, pt.Y);
    g.RotateTransform(angle);
    g.FillEllipse(&scopeBrush, -major/2, -minor/2, major, minor);
    g.DrawEllipse(&scopePen, -major/2, -minor/2, major, minor);
    g.ResetTransform();

    // Draw az/alt text
    wchar_t buf[64];
    swprintf_s(buf, L"Az %0.1f°", azimuth);
    g.DrawString(buf, -1, &font, PointF(0,0), &textBrush);
    swprintf_s(buf, L"Alt %0.1f°", altitude);
    g.DrawString(buf, -1, &font, PointF(0, h - font.GetHeight(&g)), &textBrush);

    // Convert to HBITMAP and set into static control
    HBITMAP hBmp = NULL;
    bmp.GetHBITMAP(Color::Black, &hBmp);
    if (hBmp)
    {
        // replace previous
        HBITMAP old = (HBITMAP)SendMessageW(hScopeWnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmp);
        if (old) DeleteObject(old);
    }
}

void exportData()
{
    std::ostringstream output;
    size_t rowCount = 0;
    for (auto const* stat : stats) { if (stat == nullptr) continue; rowCount = (std::max)(rowCount, stat->v.size()); }
    bool firstColumn = true;
    for (auto const* stat : stats) 
    {
        if (stat == nullptr) continue;
        if (!firstColumn) output << '\t';
        firstColumn = false;
        if (stat->title != nullptr) output << stat->title;
        if (stat->unit != nullptr && stat->unit[0] != '\0') output << " (" << stat->unit << ')';
    }
    output << "\r\n";

    for (size_t row = 0; row < rowCount; ++row) 
    {
        firstColumn = true;
        for (auto const* stat : stats) 
        {
            if (stat == nullptr) continue;
            if (!firstColumn) output << '\t';
            firstColumn = false;
            if (row < stat->v.size()) output << std::setprecision(17) << stat->v[row];
        }
        output << "\r\n";
    }
    std::string text = output.str();

    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1); if (memory == nullptr) return;

    void* clipboardData = GlobalLock(memory);
    if (clipboardData == nullptr) { GlobalFree(memory); return; }

    std::memcpy(clipboardData, text.c_str(), text.size() + 1);
    GlobalUnlock(memory);

    if (!OpenClipboard(nullptr)) { GlobalFree(memory); return; }
    if (!EmptyClipboard() || SetClipboardData(CF_TEXT, memory) == nullptr) GlobalFree(memory);
    CloseClipboard();
}

#include <map>
#include <limits>

HWND statsWnd= nullptr;
bool stopStats= false;

void DrawStats()
{
    CData &d= stats;
    if (!statsWnd) return;
    RECT rc;
    GetClientRect(statsWnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w < 40 || h < 40) return;

    using namespace Gdiplus;
    Bitmap bmp(w, h, PixelFormat32bppARGB);
    Graphics g(&bmp);

    // High quality rendering configuration
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // Clear background
    SolidBrush bgBrush(Color(255, 20, 20, 25));
    g.FillRectangle(&bgBrush, 0, 0, w, h);

    if (d.empty()) return;

    // Define plot area padding
    const REAL padLeft   = 20.0f;
    const REAL padRight  = 20.0f;
    const REAL padTop    = 40.0f;
    const REAL padBottom = 10.0f;

    REAL plotW = (REAL)w - padLeft - padRight;
    REAL plotH = (REAL)h - padTop - padBottom;
    if (plotW <= 0.0f || plotH <= 0.0f) return;

    // --- STEP 1: Compute Y-axis Min/Max per Unit Type ---
    struct ScaleRange {
        double minVal = std::numeric_limits<double>::infinity();
        double maxVal = -std::numeric_limits<double>::infinity();
    };
    std::map<std::string, ScaleRange> unitScales;

    size_t maxPoints = 0; 
    for (size_t i = 0; i < d.size(); ++i) {
        std::string unitStr = d[i]->unit ? d[i]->unit : "";
        maxPoints = (std::max)(maxPoints, d[i]->v.size());

        for (double val : d[i]->v) {
            if (val < unitScales[unitStr].minVal) unitScales[unitStr].minVal = val;
            if (val > unitScales[unitStr].maxVal) unitScales[unitStr].maxVal = val;
        }
    }

    // Standardize ranges (avoid div by zero when min == max)
    for (auto &pair : unitScales) {
        if (pair.second.minVal > pair.second.maxVal) {
            pair.second.minVal = 0.0;
            pair.second.maxVal = 1.0;
        } else if (pair.second.minVal == pair.second.maxVal) {
            pair.second.minVal -= 1.0;
            pair.second.maxVal += 1.0;
        }
    }

    // --- STEP 2: Draw Plot Grid & Axes ---
    Pen gridPen(Color(80, 255, 255, 255), 1.0f);
    gridPen.SetDashStyle(DashStyleDash);
    Pen axisPen(Color(255, 180, 180, 180), 1.5f);

    // Horizontal grid lines (4 divisions)
    for (int i = 0; i <= 4; ++i) {
        REAL y = padTop + (plotH / 4.0f) * i;
        g.DrawLine(&gridPen, padLeft, y, padLeft + plotW, y);
    }

    // Outer bounding box for plot area
    g.DrawRectangle(&axisPen, padLeft, padTop, plotW, plotH);

    // --- STEP 3: Pre-generate Distinct Colors per Dataset ---
    // Color palette generator using HSV color space
    auto GetHSVColor = [](float hue, float sat = 0.85f, float val = 0.95f) -> Color {
        float c = val * sat;
        float x = c * (1.0f - std::abs(std::fmod(hue / 60.0f, 2.0f) - 1.0f));
        float m = val - c;
        float r = 0, g = 0, b = 0;

        if (hue < 60)       { r = c; g = x; b = 0; }
        else if (hue < 120) { r = x; g = c; b = 0; }
        else if (hue < 180) { r = 0; g = c; b = x; }
        else if (hue < 240) { r = 0; g = x; b = c; }
        else if (hue < 300) { r = x; g = 0; b = c; }
        else                { r = c; g = 0; b = x; }

        return Color(255, (BYTE)((r + m) * 255), (BYTE)((g + m) * 255), (BYTE)((b + m) * 255));
        };

    std::vector<Color> seriesColors;
    for (size_t i = 0; i < d.size(); ++i) {
        float hue = (float)(i * 360.0 / (d.size() > 0 ? d.size() : 1));
        seriesColors.push_back(GetHSVColor(hue));
    }

    // --- STEP 4: Render Lines & Points ---
    Font textFont(L"Arial", 8.0f);
    SolidBrush textBrush(Color(255, 200, 200, 200));

    if (maxPoints > 1) {
        REAL xStep = plotW / (REAL)(maxPoints - 1);

        for (size_t i = 0; i < d.size(); ++i) {
            auto const &vec = d[i]->v;
            if (vec.size() < 2) continue;

            std::string unitStr = d[i]->unit ? d[i]->unit : "";
            double minV = unitScales[unitStr].minVal;
            double maxV = unitScales[unitStr].maxVal;
            double range = maxV - minV;

            Pen linePen(seriesColors[i], 2.0f);
            std::vector<PointF> pts;
            pts.reserve(vec.size());

            // Compute aligned screen coordinates
            for (size_t j = 0; j < vec.size(); ++j) {
                REAL x = padLeft + j * xStep; // Vertical alignment across all vectors
                REAL normY = (REAL)((vec[j] - minV) / range);
                REAL y = padTop + plotH * (1.0f - normY); // Invert Y-axis
                pts.push_back(PointF(x, y));
            }

            // Draw series line path
            g.DrawLines(&linePen, pts.data(), (INT)pts.size());

            // Optional: Draw small nodes at each data point
            // SolidBrush pointBrush(seriesColors[i]);
            // for (auto const &pt : pts) g.FillEllipse(&pointBrush, pt.X - 2.5f, pt.Y - 2.5f, 5.0f, 5.0f);
        }
    }

    // --- STEP 5: Render Header Legend ---
    REAL legendX = padLeft;
    REAL legendY = 8.0f;
    Font legendFont(L"Arial", 8.0f, FontStyleBold);

    for (size_t i = 0; i < d.size(); ++i) {
        SolidBrush legBrush(seriesColors[i]);
        g.FillRectangle(&legBrush, legendX, legendY + 2.0f, 10.0f, 10.0f);

        std::string labelStr = (d[i]->title ? d[i]->title : "Series");
        if (d[i]->unit && d[i]->unit[0] != '\0') {
            labelStr += " (";
            labelStr += d[i]->unit;
            labelStr += ")";
        }

        std::wstring wLabel(labelStr.begin(), labelStr.end());
        g.DrawString(wLabel.c_str(), -1, &legendFont, PointF(legendX + 14.0f, legendY), &textBrush);

        // Advance legend item X offset
        RectF boundingBox;
        g.MeasureString(wLabel.c_str(), -1, &legendFont, PointF(0, 0), &boundingBox);
        legendX += 14.0f + boundingBox.Width + 15.0f;

        // Wrap legend line if wider than window width
        if (legendX > (REAL)w - padRight - 80.0f) {
            legendX = padLeft;
            legendY += 14.0f;
        }
    }

    // Convert to HBITMAP and set into static control
    HBITMAP hBmp = NULL;
    bmp.GetHBITMAP(Color::Black, &hBmp);
    if (hBmp)
    {
        // replace previous
        HBITMAP old = (HBITMAP)SendMessageW(statsWnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmp);
        if (old) DeleteObject(old);
    }

}

// Button control IDs
#define IDC_BTN_CLICK1 2001
#define IDC_BTN_CLICK2 2002
#define IDC_BTN_CLICK3 2003

// Buttons
HWND hBtnClick1 = nullptr;
HWND hBtnClick2 = nullptr;
HWND hBtnDown1 = nullptr;
HWND hBtnDown2 = nullptr;
HWND hBtnDown3 = nullptr;
HWND hBtnDown4 = nullptr;
HWND hBtnUp = nullptr;

// Original window procs for subclassed buttons
WNDPROC prevBtnDown1Proc = nullptr;
WNDPROC prevBtnDown2Proc = nullptr;
WNDPROC prevBtnDown3Proc = nullptr;
WNDPROC prevBtnDown4Proc = nullptr;
WNDPROC prevBtnUpProc = nullptr;

// Handlers called when buttons are activated
void OnClickButton1()
{
    //g_val1.fetch_add(1);
}
void OnClickButton2()
{
    //g_val2.fetch_add(1);
}
void OnMouseDown1()
{
    //g_val3.fetch_add(1);
}
void OnMouseDown2()
{
    //g_val1.fetch_add(5);
}
void OnMouseUp()
{
    //g_val2.fetch_add(5);
}

// Subclass procs to catch mouse down/up on specific buttons
LRESULT CALLBACK BtnDown1Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_LBUTTONDOWN) scope.moveRa(4);
    if (msg == WM_LBUTTONUP) scope.abortslew();
    return CallWindowProc(prevBtnDown1Proc, hwnd, msg, wp, lp);
}
LRESULT CALLBACK BtnDown2Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_LBUTTONDOWN) scope.moveRa(-4);
    if (msg == WM_LBUTTONUP) scope.abortslew();
    return CallWindowProc(prevBtnDown2Proc, hwnd, msg, wp, lp);
}
LRESULT CALLBACK BtnDown3Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_LBUTTONDOWN) scope.moveDec(4);
    if (msg == WM_LBUTTONUP) scope.abortslew();
    return CallWindowProc(prevBtnDown3Proc, hwnd, msg, wp, lp);
}
LRESULT CALLBACK BtnDown4Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_LBUTTONDOWN) scope.moveDec(-4);
    if (msg == WM_LBUTTONUP) scope.abortslew();
    return CallWindowProc(prevBtnDown4Proc, hwnd, msg, wp, lp);
}

#define CBKEY(name) \
WNDPROC prev##name = nullptr; \
LRESULT CALLBACK name##Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) { \
    if (msg == WM_LBUTTONDOWN) kbdMask|= name;                           \
    if (msg == WM_LBUTTONUP) kbdMask&= ~name;                            \
    return CallWindowProc(prev##name, hwnd, msg, wp, lp);                \
}
CBKEY(keyMenu)
CBKEY(keyGo)
CBKEY(keyLeft)
CBKEY(keySpd)
CBKEY(keyUp)
CBKEY(keyDown)
CBKEY(keyEsc)
CBKEY(keySync)
CBKEY(keyRight)


// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_NEWSCOPE, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Initialize GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    if (Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL) != Gdiplus::Ok)
    {
        // continue without GDI+ if failed
        g_gdiplusToken = 0;
    }

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    initSystem();
    // scope.init(libsgp4::DateTime::Now().ToJulian()); // init scope!
    // CreateThread(nullptr, 0, HWTask, nullptr, 0, nullptr); 

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_NEWSCOPE));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_NEWSCOPE));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_NEWSCOPE);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable
   // Create main window with an explicit default size so trackbars and other controls are visible
   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, 700, 1200, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd) return FALSE;

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);


   // Create 2 buttons that call a function on click
   // hBtnClick2 = CreateWindowW(L"BUTTON", L"Click 2", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 220, 50, 100, 30, hWnd, (HMENU)IDC_BTN_CLICK2, hInstance, nullptr);

   // UI button and screen
#define KeyBtn(name, x, y) prevkey##name= (WNDPROC)SetWindowLongPtrW(CreateWindowW(L"BUTTON", L#name, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, x*55, y*25, 50, 20, hWnd, nullptr, hInstance, nullptr), GWLP_WNDPROC, (LONG_PTR)key##name##Proc);
   KeyBtn(Menu,  0, 0);
   KeyBtn(Go,    0, 1);
   KeyBtn(Left,  0, 2);
   KeyBtn(Spd,   1, 0);
   KeyBtn(Up,    1, 1);
   KeyBtn(Down,  1, 2);
   KeyBtn(Esc,   2, 0);
   KeyBtn(Sync,  2, 1);
   KeyBtn(Right, 2, 2);
   hScreenWnd = CreateWindowW(L"STATIC", NULL, WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_CENTERIMAGE, 3*55, 0, 128*2, 32*2, hWnd, nullptr, hInstance, nullptr);


   // prevBtnDown1Proc = (WNDPROC)SetWindowLongPtrW(CreateWindowW(L"BUTTON", L"Dec-", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 100, 110, 50, 20, hWnd, nullptr, hInstance, nullptr), GWLP_WNDPROC, (LONG_PTR)BtnDown1Proc);
   // prevBtnDown2Proc = (WNDPROC)SetWindowLongPtrW(CreateWindowW(L"BUTTON", L"Dec+", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 100, 150, 50, 20, hWnd, nullptr, hInstance, nullptr), GWLP_WNDPROC, (LONG_PTR)BtnDown2Proc);
   // prevBtnDown3Proc = (WNDPROC)SetWindowLongPtrW(CreateWindowW(L"BUTTON", L"Ra-", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 50, 130, 50, 20, hWnd, nullptr, hInstance, nullptr), GWLP_WNDPROC, (LONG_PTR)BtnDown3Proc);
   // prevBtnDown4Proc = (WNDPROC)SetWindowLongPtrW(CreateWindowW(L"BUTTON", L"Ra+", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 150, 130, 50, 20, hWnd, nullptr, hInstance, nullptr), GWLP_WNDPROC, (LONG_PTR)BtnDown4Proc);

   int y= 3*25+10-25;

   // lst+motor positions
   hLabel1 = CreateWindowW(L"STATIC", L"0", WS_CHILD | WS_VISIBLE, 0, y+=25, 250, 20, hWnd, nullptr, hInstance, nullptr);
   // actual and desired ra/dec
   hLabel2 = CreateWindowW(L"STATIC", L"0", WS_CHILD | WS_VISIBLE, 0, y+=25, 250, 20, hWnd, nullptr, hInstance, nullptr);
   hLabelRaMoves = CreateWindowW(L"STATIC", L"0", WS_CHILD | WS_VISIBLE, 260, y, 400, 20, hWnd, nullptr, hInstance, nullptr);

   hLabel3 = CreateWindowW(L"STATIC", L"0", WS_CHILD | WS_VISIBLE, 0, y+=25, 250, 20, hWnd, nullptr, hInstance, nullptr);
   hLabelDecMoves = CreateWindowW(L"STATIC", L"0", WS_CHILD | WS_VISIBLE, 260, y, 400, 20, hWnd, nullptr, hInstance, nullptr);

   // Initialize common controls for trackbars
   INITCOMMONCONTROLSEX icex;
   icex.dwSize = sizeof(icex);
   icex.dwICC = ICC_BAR_CLASSES;
   InitCommonControlsEx(&icex);


   // az/alt pos graphic
   hScopeWnd = CreateWindowW(L"STATIC", NULL, WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_CENTERIMAGE, 340, y+25, 240, 180, hWnd, nullptr, hInstance, nullptr);
   // scope graphic
   hGeometryWnd = CreateWindowW(L"STATIC", NULL, WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_CENTERIMAGE, 340, y+180+25, 320, 240, hWnd, nullptr, hInstance, nullptr);
   // stats
   statsWnd = CreateWindowW(L"STATIC", NULL, WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_CENTERIMAGE, 0, y+180+25+340, 640, 300, hWnd, nullptr, hInstance, nullptr);
   stats.push_back(&m1pos); stats.push_back(&m1dstPos); stats.push_back(&m1maxSpd); stats.push_back(&m1requestedSpeed); stats.push_back(&m1currentSpeed);
   //stats.push_back(&m2pos); stats.push_back(&m2dstPos); stats.push_back(&m2maxSpd); stats.push_back(&m2requestedSpeed); stats.push_back(&m2currentSpeed);
   //stats.push_back(&statincrementalDec); stats.push_back(&statdecMoveRate); stats.push_back(&statdesiredDec); 
   //stats.push_back(&statincrementalRa); stats.push_back(&statRaMoveRate); stats.push_back(&statdesiredRa); 
   hBtnClick1 = CreateWindowW(L"BUTTON", L"Pause", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, y+180+25+340-30, 100, 30, hWnd, (HMENU)IDC_BTN_CLICK1, hInstance, nullptr);
   hBtnClick1 = CreateWindowW(L"BUTTON", L"Clear", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 100, y+180+25+340-30, 100, 30, hWnd, (HMENU)IDC_BTN_CLICK2, hInstance, nullptr);
   hBtnClick1 = CreateWindowW(L"BUTTON", L"Export", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 200, y+180+25+340-30, 100, 30, hWnd, (HMENU)IDC_BTN_CLICK3, hInstance, nullptr);

   // Create scope controls: latitude and motor angles
   // Latitude: -90..90 -> 0..180
   hTrkLat = CreateWindowExW(0, TRACKBAR_CLASS, NULL, WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 0, y+=25, 150, 30, hWnd, (HMENU)3011, hInstance, NULL);
   SendMessageW(hTrkLat, TBM_SETRANGE, TRUE, MAKELONG(0, 180));
   SendMessageW(hTrkLat, TBM_SETPOS, TRUE, (LPARAM)(int)(scope.latitude + 90.0));
   hLblLat = CreateWindowW(L"STATIC", L"Lat", WS_CHILD | WS_VISIBLE, 160, y, 100, 20, hWnd, NULL, hInstance, NULL);

   // Motor1 angle: -360..360 -> 0..720
   hTrkM1 = CreateWindowExW(0, TRACKBAR_CLASS, NULL, WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 0, y+=30, 150, 30, hWnd, (HMENU)3012, hInstance, NULL);
   SendMessageW(hTrkM1, TBM_SETRANGE, TRUE, MAKELONG(0, 180));
   SendMessageW(hTrkM1, TBM_SETPOS, TRUE, (LPARAM)(int)(scope.motor1angle + 90.0));
   hLblM1 = CreateWindowW(L"STATIC", L"M1", WS_CHILD | WS_VISIBLE, 160, y, 100, 20, hWnd, NULL, hInstance, NULL);

   // Motor2 angle: -360..360 -> 0..720
   hTrkM2 = CreateWindowExW(0, TRACKBAR_CLASS, NULL, WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 0, y+=30, 150, 30, hWnd, (HMENU)3013, hInstance, NULL);
   SendMessageW(hTrkM2, TBM_SETRANGE, TRUE, MAKELONG(0, 360));
   SendMessageW(hTrkM2, TBM_SETPOS, TRUE, (LPARAM)(int)(scope.motor2angle + 180.0));
   hLblM2 = CreateWindowW(L"STATIC", L"M2", WS_CHILD | WS_VISIBLE, 160, y, 100, 20, hWnd, NULL, hInstance, NULL);

   // Create camera control trackbars and labels on left side
   // Yaw: -180..180 -> trackbar 0..360
   hTrkYaw = CreateWindowExW(0, TRACKBAR_CLASS, NULL, WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 0, y+=40, 150, 30, hWnd, (HMENU)3001, hInstance, NULL);
   SendMessageW(hTrkYaw, TBM_SETRANGE, TRUE, MAKELONG(0, 360));
   SendMessageW(hTrkYaw, TBM_SETPOS, TRUE, (LPARAM)(int)(g_camYaw.load() + 180.0));
   hLblCamYaw = CreateWindowW(L"STATIC", L"Yaw", WS_CHILD | WS_VISIBLE, 160, y, 100, 20, hWnd, NULL, hInstance, NULL);

   // Pitch: -90..90 -> 0..180
   hTrkPitch = CreateWindowExW(0, TRACKBAR_CLASS, NULL, WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 0, y+=30, 150, 30, hWnd, (HMENU)3002, hInstance, NULL);
   SendMessageW(hTrkPitch, TBM_SETRANGE, TRUE, MAKELONG(0, 180));
   SendMessageW(hTrkPitch, TBM_SETPOS, TRUE, (LPARAM)(int)(g_camPitch.load() + 90.0));
   hLblCamPitch = CreateWindowW(L"STATIC", L"Pitch", WS_CHILD | WS_VISIBLE, 160, y, 100, 20, hWnd, NULL, hInstance, NULL);

   // Scale: 10..400 -> 0..390 mapped to 0.10..4.00
   hTrkScale = CreateWindowExW(0, TRACKBAR_CLASS, NULL, WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 0, y+=30, 150, 30, hWnd, (HMENU)3003, hInstance, NULL);
   SendMessageW(hTrkScale, TBM_SETRANGE, TRUE, MAKELONG(10, 400));
   SendMessageW(hTrkScale, TBM_SETPOS, TRUE, (LPARAM)(int)(g_camScale.load() * 100.0));
   hLblCamScale = CreateWindowW(L"STATIC", L"Scale", WS_CHILD | WS_VISIBLE, 160, y, 100, 20, hWnd, NULL, hInstance, NULL);
   

   // Start a timer to update labels every 100ms
   SetTimer(hWnd, 1, 100, NULL);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_HSCROLL:
        {
            // handle trackbar changes
            if ((HWND)lParam == hTrkYaw)
            {
                int pos = (int)SendMessageW(hTrkYaw, TBM_GETPOS, 0, 0);
                double yaw = pos - 180.0;
                g_camYaw.store(yaw);
                wchar_t buf[64];
                swprintf_s(buf, L"Yaw: %0.1f\u00B0", yaw);
                if (hLblCamYaw) SetWindowTextW(hLblCamYaw, buf);
            }
            else if ((HWND)lParam == hTrkPitch)
            {
                int pos = (int)SendMessageW(hTrkPitch, TBM_GETPOS, 0, 0);
                double pitch = pos - 90.0;
                g_camPitch.store(pitch);
                wchar_t buf[64];
                swprintf_s(buf, L"Pitch: %0.1f\u00B0", pitch);
                if (hLblCamPitch) SetWindowTextW(hLblCamPitch, buf);
            }
            else if ((HWND)lParam == hTrkScale)
            {
                int pos = (int)SendMessageW(hTrkScale, TBM_GETPOS, 0, 0);
                double scale = (double)pos / 100.0; // map 100 -> 1.0
                g_camScale.store(scale);
                wchar_t buf[64];
                swprintf_s(buf, L"Scale: %0.2fx", scale);
                if (hLblCamScale) SetWindowTextW(hLblCamScale, buf);
            } else if ((HWND)lParam == hTrkLat)
            {
                int pos = (int)SendMessageW(hTrkLat, TBM_GETPOS, 0, 0);
                double scale = (double)pos -90;
                scope.latitude= scale;
                wchar_t buf[64]; swprintf_s(buf, L"Lat: %0.4f\u00B0", scope.latitude); SetWindowTextW(hLblLat, buf);
            } else if ((HWND)lParam == hTrkM1)
            {
                int pos = (int)SendMessageW(hTrkM1, TBM_GETPOS, 0, 0);
                double scale = (double)pos - 90;
                HWSet(0, scale); scope.syncToMotors();
                wchar_t buf[64]; swprintf_s(buf, L"M1: %0.3f\u00B0", scope.motor1angle); SetWindowTextW(hLblM1, buf);
            } else if ((HWND)lParam == hTrkM2)
            {
                int pos = (int)SendMessageW(hTrkM2, TBM_GETPOS, 0, 0);
                double scale = (double)pos -180;
                HWSet(1, scale); scope.syncToMotors();
                wchar_t buf[64]; swprintf_s(buf, L"M2: %0.3f\u00B0", scope.motor2angle); SetWindowTextW(hLblM2, buf);
            }
            

            // refresh geometry immediately
            DrawTelescopeGeometry(scope.latitude, scope.motor1angle, scope.motor2angle);
        }
        break;
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDC_BTN_CLICK1:
                // Click button 1
                stopStats=!stopStats; // OnClickButton1();
                break;
            case IDC_BTN_CLICK2:
                // Click button 2
                for (auto a: stats) a->v.clear();
                break;
            case IDC_BTN_CLICK3:
                // Click button 3 (stat data export)
                exportData();
                break;
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_TIMER:
        {
            // Timer tick every 1 second - update labels from globals
            UpdateLabels(hWnd);
        }
        break;
    case WM_DESTROY:
        // Restore original button procs if subclassed
        if (hBtnDown1 && prevBtnDown1Proc) SetWindowLongPtrW(hBtnDown1, GWLP_WNDPROC, (LONG_PTR)prevBtnDown1Proc);
        if (hBtnDown2 && prevBtnDown2Proc) SetWindowLongPtrW(hBtnDown2, GWLP_WNDPROC, (LONG_PTR)prevBtnDown2Proc);
        if (hBtnUp && prevBtnUpProc) SetWindowLongPtrW(hBtnUp, GWLP_WNDPROC, (LONG_PTR)prevBtnUpProc);
        KillTimer(hWnd, 1);
        // Destroy scope bitmap if present
        if (hScopeHbmOld)
        {
            DeleteObject(hScopeHbmOld);
            hScopeHbmOld = nullptr;
        }
        if (hGeometryHbmOld)
        {
            DeleteObject(hGeometryHbmOld);
            hGeometryHbmOld = nullptr;
        }

        // Shutdown GDI+
        if (g_gdiplusToken)
            Gdiplus::GdiplusShutdown(g_gdiplusToken);

        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

#include "../main/libsgp4/CoordGeodetic.h"
#include "../main/libsgp4/CoordTopocentric.h"
#include "../main/libsgp4/Observer.h"
#include "../main/libsgp4/SGP4.h"

int tstsgp4()
{
    libsgp4::Observer obs(51.507406923983446, -0.12773752212524414, 0.05);
    libsgp4::Tle tle = libsgp4::Tle("UK-DMC 2                ",
        "1 35683U 09041C   12289.23158813  .00000484  00000-0  89219-4 0  5863",
        "2 35683  98.0221 185.3682 0001499 100.5295 259.6088 14.69819587172294");
    libsgp4::SGP4 sgp4(tle);

    std::cout << tle << std::endl;

    for (int i = 0; i < 10; ++i)
    {
        try
        {
            libsgp4::DateTime dt = tle.Epoch().AddMinutes(i * 10);
            libsgp4::Eci eci = sgp4.FindPosition(dt);
            libsgp4::CoordTopocentric topo = obs.GetLookAngle(eci);
            libsgp4::CoordGeodetic geo = eci.ToGeodetic();

            std::cout << dt << " " << topo << " " << geo << std::endl;
        }
        catch (libsgp4::SatelliteException& e)
        {
            std::cerr << "Satellite error: " << e.what() << std::endl;
        }
        catch (libsgp4::DecayedException& e)
        {
            std::cerr << "Satellite decayed: " << e.what() << std::endl;
            std::cerr << "Position: " << e.Position() << std::endl;
        }
    }

    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            tstsgp4();
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}




const uint64_t timeFrequency= []{ LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f.QuadPart; }();
uint32_t nowms()
{
    LARGE_INTEGER cnt; QueryPerformanceCounter(&cnt);
    return (uint32_t)((cnt.QuadPart*1000)/timeFrequency);
}
uint32_t Milisecond() { return nowms(); }
uint64_t nowus()
{
    LARGE_INTEGER cnt; QueryPerformanceCounter(&cnt);
    return (cnt.QuadPart*1000000)/timeFrequency;
}

void vTaskDelay(int ms) { Sleep(ms); }

