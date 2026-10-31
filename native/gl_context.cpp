// SPDX-License-Identifier: GPL-2.0-or-later
#include "gl_context.h"
#include <glad/gl.h>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <EGL/egl.h>
#endif
namespace vcop2 {
GLContext::GLContext() {
#ifdef _WIN32
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"Virtua Cop 2VRGL";
    RegisterClassW(&wc);
    auto window=CreateWindowW(wc.lpszClassName,L"Virtua Cop 2 VR",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,wc.hInstance,nullptr);
    if(!window)throw std::runtime_error("Cannot create OpenGL window");
    window_=window;auto dc=GetDC(window);display_=dc;
    PIXELFORMATDESCRIPTOR p{};p.nSize=sizeof(p);p.nVersion=1;p.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
    p.iPixelType=PFD_TYPE_RGBA;p.cColorBits=32;p.cDepthBits=24;
    int format=ChoosePixelFormat(dc,&p);
    if(!format || !SetPixelFormat(dc,format,&p))throw std::runtime_error("Cannot set OpenGL pixel format");
    auto bootstrap=wglCreateContext(dc);
    if(!bootstrap || !wglMakeCurrent(dc,bootstrap))throw std::runtime_error("Cannot create OpenGL bootstrap context");
    using CreateContext=HGLRC(WINAPI*)(HDC,HGLRC,const int*);
    auto create=reinterpret_cast<CreateContext>(wglGetProcAddress("wglCreateContextAttribsARB"));
    HGLRC context{};
    for(auto version:{46,45,43,33}) {
        int attribs[]={0x2091,version/10,0x2092,version%10,0x9126,1,0};
        if(create&&(context=create(dc,nullptr,attribs)))break;
    }
    wglMakeCurrent(nullptr,nullptr);wglDeleteContext(bootstrap);
    if(!context || !wglMakeCurrent(dc,context))throw std::runtime_error("OpenGL 3.3 is required");
    context_=context;
    auto load=[](const char* name)->GLADapiproc {
        auto p=wglGetProcAddress(name);
        if(!p || p==PROC(1) || p==PROC(2) || p==PROC(3) || p==PROC(-1))
            p=GetProcAddress(GetModuleHandleW(L"opengl32.dll"),name);
        return reinterpret_cast<GLADapiproc>(p);
    };
    if(!gladLoadGL(load))throw std::runtime_error("Cannot load OpenGL functions");
#else
    auto display=eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if(!eglInitialize(display,nullptr,nullptr))throw std::runtime_error("Cannot initialize EGL");
    EGLint attribs[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,
        EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
    EGLConfig config{};EGLint count{};
    if(!eglChooseConfig(display,attribs,&config,1,&count)||!count)throw std::runtime_error("No GLES3 EGL config");
    EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    auto context=eglCreateContext(display,config,EGL_NO_CONTEXT,ca);
    EGLint pa[]={EGL_WIDTH,16,EGL_HEIGHT,16,EGL_NONE};auto surface=eglCreatePbufferSurface(display,config,pa);
    if(context==EGL_NO_CONTEXT||surface==EGL_NO_SURFACE||!eglMakeCurrent(display,surface,surface,context))throw std::runtime_error("Cannot create GLES3 context");
    display_=display;surface_=surface;context_=context;config_=config;
    if(!gladLoadGLES2(reinterpret_cast<GLADloadfunc>(eglGetProcAddress)))throw std::runtime_error("Cannot load GLES functions");
#endif
}
GLContext::~GLContext() {
#ifdef _WIN32
    wglMakeCurrent(nullptr,nullptr);if(context_)wglDeleteContext(static_cast<HGLRC>(context_));
    if(window_) {ReleaseDC(static_cast<HWND>(window_),static_cast<HDC>(display_));DestroyWindow(static_cast<HWND>(window_));}
#else
    if(display_) {eglMakeCurrent(display_,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
        if(context_)eglDestroyContext(display_,context_);if(surface_)eglDestroySurface(display_,surface_);eglTerminate(display_);}
#endif
}
}
