// SPDX-License-Identifier: GPL-2.0-or-later
#include "shared_runtime.h"
#include "gl_context.h"
#include "gl_scene.h"
#include "presentation.h"
#include "settings_io.h"
#include "model2_scene.h"
#include "controller_controls.h"
#include "shared/vr4all_splash.h"
#include <glad/gl.h>
#ifdef _WIN32
#include <windows.h>
#include <unknwn.h>
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_OPENGL
#define CV_EXPORT extern "C" __declspec(dllexport)
#else
#include <jni.h>
#include <EGL/egl.h>
#include <dlfcn.h>
#include <unistd.h>
#define XR_USE_PLATFORM_ANDROID
#define XR_USE_GRAPHICS_API_OPENGL_ES
#define CV_EXPORT extern "C" __attribute__((visibility("default")))
#endif
#define XR_NO_PROTOTYPES
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

namespace vcop2 {
namespace {
using Clock=std::chrono::steady_clock;
std::thread worker;
std::atomic<bool> stopping{};
std::ofstream logfile;
#ifdef __ANDROID__
JavaVM* java_vm{};jobject activity{};
uint32_t emulation_thread{};
#endif
void log(const std::string& s){logfile<<s<<'\n';logfile.flush();}
void check(XrResult r,const char* what){if(XR_FAILED(r))throw std::runtime_error(std::string(what)+": OpenXR "+std::to_string(r));}
#define CV_XR_FUNCTIONS(F) \
 F(EnumerateInstanceExtensionProperties) F(CreateInstance) F(DestroyInstance) F(GetSystem) \
 F(CreateSession) F(DestroySession) F(EnumerateViewConfigurationViews) F(EnumerateSwapchainFormats) \
 F(CreateSwapchain) F(DestroySwapchain) F(EnumerateSwapchainImages) F(AcquireSwapchainImage) F(WaitSwapchainImage) F(ReleaseSwapchainImage) \
 F(CreateReferenceSpace) F(DestroySpace) F(PollEvent) F(BeginSession) F(EndSession) F(RequestExitSession) \
 F(WaitFrame) F(BeginFrame) F(EndFrame) F(LocateViews) F(LocateSpace) F(StringToPath) \
 F(CreateActionSet) F(DestroyActionSet) F(CreateAction) F(SuggestInteractionProfileBindings) F(AttachSessionActionSets) \
 F(CreateActionSpace) F(SyncActions) F(GetActionStateBoolean) F(GetActionStateFloat) F(GetActionStateVector2f) F(ApplyHapticFeedback)
#define DECLARE(name) PFN_xr##name xr##name{};
CV_XR_FUNCTIONS(DECLARE)
#undef DECLARE
PFN_xrGetInstanceProcAddr get_proc{};
void load_functions(XrInstance instance) {
#define LOAD(name) {PFN_xrVoidFunction f{};auto r=get_proc(instance,"xr" #name,&f);if(XR_SUCCEEDED(r))xr##name=reinterpret_cast<PFN_xr##name>(f);}
CV_XR_FUNCTIONS(LOAD)
#undef LOAD
}
void load_global_functions() {
    check(get_proc(XR_NULL_HANDLE,"xrEnumerateInstanceExtensionProperties",reinterpret_cast<PFN_xrVoidFunction*>(&xrEnumerateInstanceExtensionProperties)),"Load extension enumeration");
    check(get_proc(XR_NULL_HANDLE,"xrCreateInstance",reinterpret_cast<PFN_xrVoidFunction*>(&xrCreateInstance)),"Load instance creation");
}
glm::mat4 pose(const XrPosef& p) {
    return glm::translate(glm::mat4(1),{p.position.x,p.position.y,p.position.z})*
        glm::mat4_cast(glm::quat(p.orientation.w,p.orientation.x,p.orientation.y,p.orientation.z));
}
glm::mat4 projection(const XrFovf& f) {
    const float l=std::tan(f.angleLeft),r=std::tan(f.angleRight),b=std::tan(f.angleDown),t=std::tan(f.angleUp),n=.02f;
    glm::mat4 m(0);m[0][0]=2/(r-l);m[1][1]=2/(t-b);m[2][0]=(r+l)/(r-l);m[2][1]=(t+b)/(t-b);
    m[2][2]=-1;m[2][3]=-1;m[3][2]=-2*n;return m; // infinite far plane
}
void validate_graphics_version(XrVersion minimum,XrVersion maximum) {
    GLint major{},minor{};glGetIntegerv(GL_MAJOR_VERSION,&major);glGetIntegerv(GL_MINOR_VERSION,&minor);
    auto actual=XR_MAKE_VERSION(major,minor,0);
    log("Graphics API "+std::to_string(major)+"."+std::to_string(minor)+", runtime minimum "+std::to_string(XR_VERSION_MAJOR(minimum))+"."+std::to_string(XR_VERSION_MINOR(minimum)));
    if(actual<minimum||actual>maximum)throw std::runtime_error("OpenXR runtime requires another graphics API version; active context is "+std::to_string(major)+"."+std::to_string(minor));
}
void save_ppm(const std::string& name,int w,int h) {
    std::vector<uint8_t> pixels(size_t(w)*h*3);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    std::ofstream f(name,std::ios::binary);f<<"P6\n"<<w<<" "<<h<<"\n255\n";
    for(int y=h-1;y>=0;y--)f.write(reinterpret_cast<const char*>(pixels.data()+size_t(y)*w*3),w*3);
}
NativeLens display_lens(const NativeFrame& frame,const Settings&) {
    return native_camera_lens(frame);
}
void simulation(SharedRuntime& shared) {
    GLContext context;GLScene scene;const bool immersive=!std::getenv("VCOP2_SIM_REFERENCE");scene.set_immersive(immersive);GLuint framebuffer{},color{},depth{};
    glGenFramebuffers(1,&framebuffer);glGenTextures(1,&color);glGenRenderbuffers(1,&depth);
    uint64_t saved{},joined_sequence{};int width{},height{};shared.host_ready=true;
    InputPolicy diagnostic_policy;ServiceGunPolicy diagnostic_service_guns;Settings diagnostic_settings;
    auto started=Clock::now();auto last_state_log=started;
    log("Diagnostic OpenGL simulation ready; no headset session.");
    while(!stopping) {
        auto f=shared.scene.load();
        if(Clock::now()-last_state_log>=std::chrono::seconds(1)) {
            last_state_log=Clock::now();GameState state;{std::lock_guard lock(shared.mutex);state=shared.state;}
            log("Native state: valid "+std::to_string(state.valid)+", ready "+std::to_string(state.ready)+", playing "+std::to_string(state.playing)+", service "+std::to_string(state.service)+", players "+std::to_string(state.players[0].state)+"/"+std::to_string(state.players[1].state)+", frame "+std::to_string(f?f->sequence:0));
        }
        if(std::getenv("VCOP2_SIM_GUN_TEST")) {
            GameState state;{std::lock_guard lock(shared.mutex);state=shared.state;}
            double seconds=std::chrono::duration<double>(Clock::now()-started).count();
            Hands hands;for(auto& hand:hands){hand.tracked=true;hand.aim={.5,.5,true,true};hand.trigger=std::fmod(seconds,.4)<.12;}
            if(std::getenv("VCOP2_SIM_RELOAD_TEST")&&seconds>20&&std::fmod(seconds,3.)<.15)
                for(auto& hand:hands){hand.pump=true;hand.trigger=false;}
            if(std::getenv("VCOP2_SIM_AIM_CORNERS")) {
                if(!joined_sequence&&f&&state.players[0].joined()&&state.players[1].joined())joined_sequence=f->sequence;
                bool swapped=joined_sequence&&f&&f->sequence>=joined_sequence+120;
                hands[0].aim={.25,swapped?.8:.2,true,true};hands[1].aim={.75,swapped?.2:.8,true,true};
            }
            if(state.valid&&state.ready) {hands[1].start=!state.players[0].joined()&&std::fmod(seconds,2.)<.3;hands[0].start=state.players[0].joined()&&!state.players[1].joined()&&std::fmod(seconds,2.)<.3;}
            auto input=diagnostic_policy.update(seconds,hands,state,diagnostic_settings,true,false);
            {std::lock_guard lock(shared.mutex);shared.input=input;}
            for(int p=0;p<2;p++)if(input.shots[p])log("Diagnostic accepted player "+std::to_string(p+1)+" shot "+std::to_string(state.players[p].shots));
        }
        if(std::getenv("VCOP2_SIM_SERVICE_TEST")) {
            shared.service_active=true;
            double seconds=std::chrono::duration<double>(Clock::now()-started).count();Hands hands;
            double coordinate=int(seconds)%3==0?.1:int(seconds)%3==1?.9:.5;
            for(auto& hand:hands){hand.tracked=true;hand.aim={coordinate,coordinate,true,true};hand.trigger=seconds>8.8&&std::fmod(seconds,1.)<.1;}
            auto input=diagnostic_service_guns.update(hands,diagnostic_settings,true);
            std::lock_guard lock(shared.mutex);shared.input=input;
        }
        if(f && f->complete) {
            if(width!=int(f->width)||height!=int(f->height)) {
                width=f->width;height=f->height;glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
                glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);
                glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
            }
            glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glViewport(0,0,width,height);scene.prepare(f,std::getenv("VCOP2_SIM_WIDER_VIEW")!=nullptr);
            // Neutral camera used only to validate the live C++/engine integration.
            auto lens=immersive?native_camera_lens(*f):presentation_lens(*f);
            XrFovf fov{float(std::atan(-lens.cx/lens.fx)),float(std::atan((f->width-lens.cx)/lens.fx)),
                float(std::atan(lens.cy/lens.fy)),float(std::atan((lens.cy-f->height)/lens.fy))};
            if(immersive)fov={-glm::quarter_pi<float>(),glm::quarter_pi<float>(),glm::quarter_pi<float>(),-glm::quarter_pi<float>()};
            double offset=0;if(const char* value=std::getenv("VCOP2_SIM_OFFSET_X")){char* end{};double parsed=std::strtod(value,&end);if(end!=value&&!*end&&std::isfinite(parsed))offset=std::clamp(parsed,-.25,.25);}
            scene.draw(projection(fov)*glm::translate(glm::mat4(1),glm::vec3(-offset,0,0)),1.f,1.8f);
            if(f->sequence>=saved+120) {
                saved=f->sequence;
                const char* dir=std::getenv("VCOP2_SIM_DIR");if(dir)save_ppm(std::string(dir)+"/live-"+std::to_string(saved)+".ppm",width,height);
                if(dir&&std::getenv("VCOP2_SIM_WIDER_VIEW")&&std::getenv("VCOP2_SIM_COMPARE")) {
                    scene.prepare(f,false);scene.draw(projection(fov)*glm::translate(glm::mat4(1),glm::vec3(-offset,0,0)),1.f,1.8f);
                    save_ppm(std::string(dir)+"/original-"+std::to_string(saved)+".ppm",width,height);scene.prepare(f,true);
                }
                GameState state;{std::lock_guard lock(shared.mutex);state=shared.state;}
                log("Rendered native scene "+std::to_string(saved)+", "+std::to_string(f->polygons.size())+" Model 2 polygons, ready "+std::to_string(state.ready)+", playing "+std::to_string(state.playing)+", service "+std::to_string(state.service)+", players "+std::to_string(state.players[0].state)+"/"+std::to_string(state.players[1].state));
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(11));
    }
    shared.host_ready=false;glDeleteRenderbuffers(1,&depth);glDeleteTextures(1,&color);glDeleteFramebuffers(1,&framebuffer);
}

struct Host {
    SharedRuntime& shared;
    GLContext context;
    GLScene scene;
    Presentation ui;
    vr::xr::LaunchSplash launch_splash;
    XrInstance instance{};XrSystemId system{};XrSession session{};XrSpace local{};XrActionSet actions{};
    XrAction aim{},grip_pose{},trigger{},start{},pump{},menu{},haptic{},stick{},grip{},click{},menu_hold{};
    XrPath hands[2]{};XrSpace aim_space[2]{},grip_space[2]{};bool grip_logged[2]{};
    struct Chain {XrSwapchain swapchain{};uint32_t width{},height{};std::vector<GLuint> colors;GLuint fbo{},depth{};} eyes[2];
    XrSessionState session_state{};bool running{},origin_set{};glm::mat4 origin{1};
    PFN_xrPerfSettingsSetPerformanceLevelEXT performance{};
#ifdef __ANDROID__
    PFN_xrSetAndroidApplicationThreadKHR set_thread{};
#endif
    Settings settings;InputPolicy policy;ShortcutPolicy shortcuts;OperatorPolicy operator_controls;ServiceGunPolicy service_guns;MenuHoldPolicy menu_hold_policy;ButtonLatch service_click;glm::mat4 head_pose{1};
    Host(SharedRuntime& s):shared(s){const char* file=std::getenv("VCOP2_SETTINGS");settings=load_settings(file?file:"vcop2-settings.ini");scene.set_immersive(true);shared.camera_fov=settings.wider_view?settings.view_fov:0.f;log("World presentation: true camera-space, HMD clipping, scale "+std::to_string(settings.world_scale)+", camera FOV "+std::to_string(shared.camera_fov.load()));log(std::string("Game zoom: ")+(settings.game_zoom?"On":"Off")+" (session start)");}
    ~Host() {
        shared.host_ready=false;
        for(auto& eye:eyes) {if(eye.depth)glDeleteRenderbuffers(1,&eye.depth);if(eye.fbo)glDeleteFramebuffers(1,&eye.fbo);if(eye.swapchain)xrDestroySwapchain(eye.swapchain);}
        for(auto space:aim_space)if(space)xrDestroySpace(space);
        for(auto space:grip_space)if(space)xrDestroySpace(space);
        if(local)xrDestroySpace(local);if(session)xrDestroySession(session);if(actions)xrDestroyActionSet(actions);if(instance)xrDestroyInstance(instance);
    }
    XrPath path(const char* name){XrPath p{};check(xrStringToPath(instance,name,&p),name);return p;}
    XrAction action(const char* name,XrActionType type,bool sub=true) {
        XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};std::strncpy(info.actionName,name,sizeof(info.actionName)-1);std::strncpy(info.localizedActionName,name,sizeof(info.localizedActionName)-1);
        info.actionType=type;info.countSubactionPaths=sub?2:0;info.subactionPaths=sub?hands:nullptr;
        XrAction a{};check(xrCreateAction(actions,&info,&a),name);return a;
    }
    void init() {
#ifdef _WIN32
        auto module=LoadLibraryW(L"openxr_loader.dll");if(!module)module=LoadLibraryW(L"libopenxr_loader.dll");
        if(!module)throw std::runtime_error("OpenXR loader DLL is missing");
        get_proc=reinterpret_cast<PFN_xrGetInstanceProcAddr>(GetProcAddress(module,"xrGetInstanceProcAddr"));
#else
        auto module=dlopen("libopenxr_loader.so",RTLD_NOW|RTLD_LOCAL);
        if(!module)throw std::runtime_error("OpenXR loader is missing");
        get_proc=reinterpret_cast<PFN_xrGetInstanceProcAddr>(dlsym(module,"xrGetInstanceProcAddr"));
#endif
        if(!get_proc)throw std::runtime_error("Invalid OpenXR loader");load_global_functions();
        std::vector<const char*> extensions;
#ifdef _WIN32
        extensions.push_back(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME);
#else
        if(!java_vm||!activity)throw std::runtime_error("Android Activity is unavailable");
        PFN_xrInitializeLoaderKHR initialize{};
        check(get_proc(XR_NULL_HANDLE,"xrInitializeLoaderKHR",reinterpret_cast<PFN_xrVoidFunction*>(&initialize)),"Loader initialization function");
        XrLoaderInitInfoAndroidKHR loader{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};loader.applicationVM=java_vm;loader.applicationContext=activity;
        check(initialize(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&loader)),"Initialize Android loader");
        extensions.push_back(XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME);extensions.push_back(XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME);
#endif
        uint32_t extension_count{};
        check(xrEnumerateInstanceExtensionProperties(nullptr,0,&extension_count,nullptr),"Extension count");
        std::vector<XrExtensionProperties> supported(extension_count,{XR_TYPE_EXTENSION_PROPERTIES});
        check(xrEnumerateInstanceExtensionProperties(nullptr,extension_count,&extension_count,supported.data()),"Extensions");
        bool sustained=std::any_of(supported.begin(),supported.end(),[](const auto& e){return std::strcmp(e.extensionName,XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME)==0;});
        if(sustained)extensions.push_back(XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME);
#ifdef __ANDROID__
        bool thread_settings=std::any_of(supported.begin(),supported.end(),[](const auto& e){return std::strcmp(e.extensionName,XR_KHR_ANDROID_THREAD_SETTINGS_EXTENSION_NAME)==0;});
        if(thread_settings)extensions.push_back(XR_KHR_ANDROID_THREAD_SETTINGS_EXTENSION_NAME);
#endif
        XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};std::strcpy(info.applicationInfo.applicationName,"Virtua Cop 2 VR");
        std::strcpy(info.applicationInfo.engineName,"MAME VR4ALL");info.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,34);
        info.enabledExtensionCount=extensions.size();info.enabledExtensionNames=extensions.data();
#ifdef __ANDROID__
        XrInstanceCreateInfoAndroidKHR android{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};android.applicationVM=java_vm;android.applicationActivity=activity;info.next=&android;
#endif
        check(xrCreateInstance(&info,&instance),"CreateInstance");load_functions(instance);
        if(sustained)check(get_proc(instance,"xrPerfSettingsSetPerformanceLevelEXT",reinterpret_cast<PFN_xrVoidFunction*>(&performance)),"Load performance settings");
#ifdef __ANDROID__
        if(thread_settings)check(get_proc(instance,"xrSetAndroidApplicationThreadKHR",reinterpret_cast<PFN_xrVoidFunction*>(&set_thread)),"Load Android thread settings");
#endif
        XrSystemGetInfo sys{XR_TYPE_SYSTEM_GET_INFO};sys.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;check(xrGetSystem(instance,&sys,&system),"GetSystem");
#ifdef _WIN32
        PFN_xrGetOpenGLGraphicsRequirementsKHR requirements{};check(get_proc(instance,"xrGetOpenGLGraphicsRequirementsKHR",reinterpret_cast<PFN_xrVoidFunction*>(&requirements)),"OpenGL requirements function");
        XrGraphicsRequirementsOpenGLKHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};check(requirements(instance,system,&req),"OpenGL requirements");
        validate_graphics_version(req.minApiVersionSupported,req.maxApiVersionSupported);
        XrGraphicsBindingOpenGLWin32KHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};binding.hDC=static_cast<HDC>(context.display());binding.hGLRC=static_cast<HGLRC>(context.context());
#else
        PFN_xrGetOpenGLESGraphicsRequirementsKHR requirements{};check(get_proc(instance,"xrGetOpenGLESGraphicsRequirementsKHR",reinterpret_cast<PFN_xrVoidFunction*>(&requirements)),"GLES requirements function");
        XrGraphicsRequirementsOpenGLESKHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};check(requirements(instance,system,&req),"GLES requirements");
        validate_graphics_version(req.minApiVersionSupported,req.maxApiVersionSupported);
        XrGraphicsBindingOpenGLESAndroidKHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
        binding.display=context.display();binding.context=context.context();binding.config=context.config();
#endif
        XrSessionCreateInfo si{XR_TYPE_SESSION_CREATE_INFO};si.next=&binding;si.systemId=system;check(xrCreateSession(instance,&si,&session),"CreateSession");
        XrReferenceSpaceCreateInfo ri{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};ri.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;ri.poseInReferenceSpace.orientation.w=1;
        check(xrCreateReferenceSpace(session,&ri,&local),"CreateReferenceSpace");
        uint32_t count{};check(xrEnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&count,nullptr),"View count");
        if(count!=2)throw std::runtime_error("Runtime must provide stereo views");
        std::vector<XrViewConfigurationView> views(count,{XR_TYPE_VIEW_CONFIGURATION_VIEW});check(xrEnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,count,&count,views.data()),"Views");
        check(xrEnumerateSwapchainFormats(session,0,&count,nullptr),"Format count");std::vector<int64_t> formats(count);check(xrEnumerateSwapchainFormats(session,count,&count,formats.data()),"Formats");
#ifdef __ANDROID__
        const int candidates[]={GL_RGBA8};
#else
        const int candidates[]={GL_SRGB8_ALPHA8,GL_RGBA8};
#endif
        int64_t format=0;for(auto candidate:candidates)if(std::find(formats.begin(),formats.end(),candidate)!=formats.end()){format=candidate;break;}
        if(!format)throw std::runtime_error("Runtime lacks an RGBA8 eye format");
        for(int i=0;i<2;i++) {
            auto& eye=eyes[i];eye.width=views[i].recommendedImageRectWidth;eye.height=views[i].recommendedImageRectHeight;
            XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};ci.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;ci.format=format;ci.sampleCount=1;ci.width=eye.width;ci.height=eye.height;ci.faceCount=ci.arraySize=ci.mipCount=1;
            check(xrCreateSwapchain(session,&ci,&eye.swapchain),"CreateSwapchain");check(xrEnumerateSwapchainImages(eye.swapchain,0,&count,nullptr),"Image count");
#ifdef _WIN32
            std::vector<XrSwapchainImageOpenGLKHR> images(count,{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
#else
            std::vector<XrSwapchainImageOpenGLESKHR> images(count,{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});
#endif
            check(xrEnumerateSwapchainImages(eye.swapchain,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())),"Images");for(auto im:images)eye.colors.push_back(im.image);
            glGenFramebuffers(1,&eye.fbo);glGenRenderbuffers(1,&eye.depth);glBindRenderbuffer(GL_RENDERBUFFER,eye.depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,eye.width,eye.height);
        }
        hands[0]=path("/user/hand/left");hands[1]=path("/user/hand/right");
        XrActionSetCreateInfo ai{XR_TYPE_ACTION_SET_CREATE_INFO};std::strcpy(ai.actionSetName,"cabinet");std::strcpy(ai.localizedActionSetName,"Cabinet");check(xrCreateActionSet(instance,&ai,&actions),"ActionSet");
        aim=action("aim",XR_ACTION_TYPE_POSE_INPUT);trigger=action("trigger",XR_ACTION_TYPE_FLOAT_INPUT);start=action("start",XR_ACTION_TYPE_BOOLEAN_INPUT);
        grip_pose=action("gun_grip",XR_ACTION_TYPE_POSE_INPUT);
        pump=action("pump",XR_ACTION_TYPE_BOOLEAN_INPUT);menu=action("menu",XR_ACTION_TYPE_BOOLEAN_INPUT);stick=action("stick",XR_ACTION_TYPE_VECTOR2F_INPUT);haptic=action("recoil",XR_ACTION_TYPE_VIBRATION_OUTPUT);
        grip=action("grip",XR_ACTION_TYPE_FLOAT_INPUT);click=action("stick_click",XR_ACTION_TYPE_BOOLEAN_INPUT);menu_hold=action("menu_hold",XR_ACTION_TYPE_BOOLEAN_INPUT);
        auto suggest=[&](const char* profile,int kind) {
            std::vector<XrActionSuggestedBinding> bindings;
            auto bind=[&](XrAction a,const std::string& p){bindings.push_back({a,path(p.c_str())});};
            for(auto hand:{"left","right"}) {
                std::string root=std::string("/user/hand/")+hand;bool left=std::strcmp(hand,"left")==0;
                bind(aim,root+"/input/aim/pose");bind(trigger,root+"/input/trigger/value");bind(haptic,root+"/output/haptic");
                bind(grip_pose,root+"/input/grip/pose");
                bind(stick,root+(kind==2?"/input/trackpad":"/input/thumbstick"));
                bind(grip,root+(kind==0?"/input/squeeze/value":kind==1?"/input/squeeze/force":"/input/squeeze/click"));
                if(kind!=2)bind(click,root+"/input/thumbstick/click");
                bind(start,root+(kind==0?(left?"/input/x/click":"/input/a/click"):kind==1?"/input/a/click":"/input/menu/click"));
                bind(pump,root+(kind<2?(kind==0&&left?"/input/y/click":"/input/b/click"):"/input/trackpad/click"));
            }
            if(kind<2)bind(menu,kind==0?"/user/hand/left/input/menu/click":"/user/hand/left/input/trackpad/force");
            else bind(menu_hold,"/user/hand/left/input/menu/click");
            XrInteractionProfileSuggestedBinding bi{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};bi.interactionProfile=path(profile);bi.countSuggestedBindings=bindings.size();bi.suggestedBindings=bindings.data();
            check(xrSuggestInteractionProfileBindings(instance,&bi),profile);
        };
        suggest("/interaction_profiles/oculus/touch_controller",0);
#ifndef __ANDROID__
        suggest("/interaction_profiles/valve/index_controller",1);suggest("/interaction_profiles/htc/vive_controller",2);suggest("/interaction_profiles/microsoft/motion_controller",3);
#endif
        XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};attach.countActionSets=1;attach.actionSets=&actions;check(xrAttachSessionActionSets(session,&attach),"Attach actions");
        for(int i=0;i<2;i++) {XrActionSpaceCreateInfo as{XR_TYPE_ACTION_SPACE_CREATE_INFO};as.action=aim;as.subactionPath=hands[i];as.poseInActionSpace.orientation.w=1;check(xrCreateActionSpace(session,&as,&aim_space[i]),"Aim space");as.action=grip_pose;check(xrCreateActionSpace(session,&as,&grip_space[i]),"Grip space");}
        shared.host_ready=true;log("OpenXR session created; waiting for headset.");
    }
    bool button(XrAction a,int hand) {XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=a;get.subactionPath=hands[hand];XrActionStateBoolean s{XR_TYPE_ACTION_STATE_BOOLEAN};check(xrGetActionStateBoolean(session,&get,&s),"Button state");return s.isActive&&s.currentState;}
    void controls(XrTime time,const std::shared_ptr<const NativeFrame>& frame) {
        XrActiveActionSet set{actions,XR_NULL_PATH};XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};sync.countActiveActionSets=1;sync.activeActionSets=&set;check(xrSyncActions(session,&sync),"Sync actions");
        Hands inputs{};ExtraControls extras;PresentationInput display;if(frame){const auto lens=native_camera_lens(*frame);if(lens.valid())display.reticle_focal={float(lens.fx),float(lens.fy)};}display.toggle=button(menu,0);display.head=glm::inverse(origin)*head_pose;
        auto now=std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
        ui.prepare(settings,&shared);
        auto inv=glm::inverse(origin);
        for(int i=0;i<2;i++) {
            auto& h=inputs[i];h.start=button(start,i);h.pump=button(pump,i);
            XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=trigger;get.subactionPath=hands[i];XrActionStateFloat t{XR_TYPE_ACTION_STATE_FLOAT};check(xrGetActionStateFloat(session,&get,&t),"Trigger state");h.trigger=t.isActive&&t.currentState>.55f;
            XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};check(xrLocateSpace(aim_space[i],local,time,&loc),"Locate hand");h.tracked=(loc.locationFlags&(XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT))==(XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT);
            display.poses[i]=inv*pose(loc.pose);
            XrSpaceLocation palm{XR_TYPE_SPACE_LOCATION};check(xrLocateSpace(grip_space[i],local,time,&palm),"Locate grip");
            const bool palm_valid=palm.locationFlags&XR_SPACE_LOCATION_POSITION_VALID_BIT;
            h.tracked=h.tracked&&palm_valid;
            if(palm_valid)display.grip_positions[i]=glm::vec3(inv*glm::vec4(palm.pose.position.x,palm.pose.position.y,palm.pose.position.z,1));
            if(h.tracked&&!grip_logged[i]){
                auto delta=glm::vec3(glm::inverse(display.poses[i])*glm::vec4(*display.grip_positions[i],1));
                log("Gun palm offset from aim: hand "+std::to_string(i)+", meters "+std::to_string(delta.x)+" "+std::to_string(delta.y)+" "+std::to_string(delta.z));grip_logged[i]=true;
            }
            XrActionStateGetInfo sg{XR_TYPE_ACTION_STATE_GET_INFO};sg.action=stick;sg.subactionPath=hands[i];XrActionStateVector2f stick_state{XR_TYPE_ACTION_STATE_VECTOR2F};check(xrGetActionStateVector2f(session,&sg,&stick_state),"Stick state");
            if(stick_state.isActive&&std::abs(stick_state.currentState.x)+std::abs(stick_state.currentState.y)>std::abs(display.stick.x)+std::abs(display.stick.y))display.stick={stick_state.currentState.x,stick_state.currentState.y};
            if(stick_state.isActive)extras.stick[i]={stick_state.currentState.x,stick_state.currentState.y};
            XrActionStateGetInfo gg{XR_TYPE_ACTION_STATE_GET_INFO};gg.action=grip;gg.subactionPath=hands[i];XrActionStateFloat squeezed{XR_TYPE_ACTION_STATE_FLOAT};check(xrGetActionStateFloat(session,&gg,&squeezed),"Grip state");
            extras.grip[i]=squeezed.isActive&&squeezed.currentState>.7f;extras.click[i]=button(click,i);
        }
        bool focused=session_state==XR_SESSION_STATE_FOCUSED&&!launch_splash.active();
        bool held=button(menu_hold,0);auto hold=menu_hold_policy.update(now,held,focused);
        if(held)inputs[0].start=false;if(hold.start)inputs[0].start=true;if(hold.toggle)display.toggle=true;
        if(service_click.edge(extras.click[1],focused&&ui.service()&&!ui.open())){ui.toggle_service_guns();shared.release_inputs();}
        const bool previous_zoom=settings.game_zoom;
        auto shortcut=shortcuts.update(now,extras,inputs,settings,focused&&!ui.open()&&!ui.service()&&!display.toggle);
        if(settings.game_zoom!=previous_zoom)log(std::string("Game zoom: ")+(settings.game_zoom?"On":"Off")+" (grip)");
        if(shortcut.recenter)origin_set=false;
        if(shortcut.laser)settings.laser=!settings.laser;
        if(shortcut.save||shortcut.laser){const char* file=std::getenv("VCOP2_SETTINGS");std::ofstream output(file?file:"vcop2-settings.ini");if(output)write_settings(output,settings);}
        // A grip tap changes zoom before aim conversion, so a simultaneous
        // shot and this frame's drawing use the same camera magnification.
        for(int i=0;i<2;i++)if(inputs[i].tracked&&frame&&frame->complete) {
            auto& h=inputs[i];h.aim={0,0,true,false};
            auto m=pitched_gun_pose(display.gun_pose(i),settings.gun_angle);auto o=ui.muzzle(m,settings,i),d=glm::normalize(glm::vec3(m*glm::vec4(0,0,-1,0)));
            const auto lens=display_lens(*frame,settings);
            const auto hit=trace_scene_aim({{o.x,o.y,o.z},{d.x,d.y,d.z}},*frame,settings.world_scale,settings.panel_distance,settings.wider_view,&lens,true,frame_zoom(*frame,settings.game_zoom));
            display.aim_targets[i]=hit.target;
            if(hit.native.valid)h.aim={hit.native.u,hit.native.v,hit.native.valid,hit.native.inside};
        }
        if(extras.grip[0]||extras.grip[1])display.stick={};
        GameState state;
        {std::lock_guard lock(shared.mutex);state=shared.state;}
        display.hands=inputs;display.focused=focused;ui.update(display,settings,shared,now);shared.camera_fov=settings.wider_view?settings.view_fov:0.f;
        shared.service_active=ui.service();
        if(ui.recenter_requested())origin_set=false;
        auto input=policy.update(now,inputs,state,settings,focused,ui.open()||ui.service());
        auto gun_test=service_guns.update(inputs,settings,focused&&ui.service()&&ui.service_guns()&&!ui.open());
        if(ui.service()&&ui.service_guns())input=gun_test;
        auto buttons=operator_controls.update(now,extras,inputs,focused&&ui.service()&&!ui.open(),ui.service_guns(),settings.p1_hand);
        if(buttons)shared.operator_pending.fetch_or(buttons);
        ui.feedback(input,now);
        {std::lock_guard lock(shared.mutex);shared.input=input;}
        for(int i=0;i<2;i++)if(input.shots[i]&&input.hand[i]>=0&&settings.haptics>0) {
            log("Native accepted shot: player "+std::to_string(i+1)+", count "+std::to_string(state.players[i].shots));
            XrHapticActionInfo hi{XR_TYPE_HAPTIC_ACTION_INFO};hi.action=haptic;hi.subactionPath=hands[input.hand[i]];
            XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};vibration.duration=30000000;vibration.frequency=XR_FREQUENCY_UNSPECIFIED;vibration.amplitude=settings.haptics;
            check(xrApplyHapticFeedback(session,&hi,reinterpret_cast<const XrHapticBaseHeader*>(&vibration)),"Shot haptic");
        }
        shared.pause_requested=ui.open()||!focused;
    }
    void run() {
        double controls_ms{},prepare_ms{},draw_ms{};uint32_t timing_frames{};
        while(!stopping) {
            XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
            while(xrPollEvent(instance,&event)==XR_SUCCESS) {
                if(event.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                    auto& e=*reinterpret_cast<XrEventDataSessionStateChanged*>(&event);session_state=e.state;log("Session state "+std::to_string(e.state));
                    if(e.state!=XR_SESSION_STATE_FOCUSED) {
                        policy.suspend();shortcuts.suspend();operator_controls.suspend();service_guns.suspend();menu_hold_policy.suspend();service_click.suspend();ui.suspend();shared.pause_requested=true;
                        shared.release_inputs();
                    }
                    if(e.state==XR_SESSION_STATE_READY) {
                        XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};begin.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;check(xrBeginSession(session,&begin),"BeginSession");running=true;
#ifdef __ANDROID__
                        if(set_thread) {
                            // SDL runs MAME on the caller of vcop2_xr_start;
                            // this worker owns the graphics device and OpenXR.
                            auto app=set_thread(session,XR_ANDROID_THREAD_TYPE_APPLICATION_MAIN_KHR,emulation_thread);
                            auto render=set_thread(session,XR_ANDROID_THREAD_TYPE_RENDERER_MAIN_KHR,uint32_t(gettid()));
                            log("XR thread roles: emulation "+std::to_string(emulation_thread)+" result "+std::to_string(app)+", renderer "+std::to_string(gettid())+" result "+std::to_string(render));
                        }
#endif
                        if(performance) {
                            auto cpu=performance(session,XR_PERF_SETTINGS_DOMAIN_CPU_EXT,XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT);
                            auto gpu=performance(session,XR_PERF_SETTINGS_DOMAIN_GPU_EXT,XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT);
                            log("Sustained performance request CPU "+std::to_string(cpu)+", GPU "+std::to_string(gpu));
                        }
                    }
                    if(e.state==XR_SESSION_STATE_STOPPING) {check(xrEndSession(session),"EndSession");running=false;shared.pause_requested=true;}
                    if(e.state==XR_SESSION_STATE_EXITING||e.state==XR_SESSION_STATE_LOSS_PENDING) {shared.quit_requested=true;return;}
                }
                event={XR_TYPE_EVENT_DATA_BUFFER};
            }
            if(!running) {std::this_thread::sleep_for(std::chrono::milliseconds(20));continue;}
            XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};XrFrameState state{XR_TYPE_FRAME_STATE};check(xrWaitFrame(session,&wait,&state),"WaitFrame");
            XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};check(xrBeginFrame(session,&begin),"BeginFrame");
            auto frame=shared.scene.load();
            static uint64_t frame_count{};
            if(++frame_count%300==0&&frame)log("XR frames "+std::to_string(frame_count)+", native frame "+std::to_string(frame->sequence)+", Model 2 polygons "+std::to_string(frame->polygons.size())+", native focal "+std::to_string(native_camera_lens(*frame).fx)+", native panel "+std::to_string(frame->raster_only)+", sky "+std::to_string(frame->sky_width)+"x"+std::to_string(frame->sky_height));
            XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;locate.displayTime=state.predictedDisplayTime;locate.space=local;
            XrViewState vs{XR_TYPE_VIEW_STATE};XrView views[2]={{XR_TYPE_VIEW},{XR_TYPE_VIEW}};uint32_t count{};
            check(xrLocateViews(session,&locate,&vs,2,&count,views),"LocateViews");
            bool valid=(vs.viewStateFlags&(XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT))==(XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT);
            head_pose=pose(views[0].pose);head_pose[3]=(pose(views[0].pose)[3]+pose(views[1].pose)[3])*.5f;
            if(valid&&!origin_set) {auto head=pose(views[0].pose);auto forward=glm::vec3(head*glm::vec4(0,0,-1,0));auto center=(glm::vec3(pose(views[0].pose)[3])+glm::vec3(pose(views[1].pose)[3]))*.5f;
                origin=glm::translate(glm::mat4(1),center)*glm::rotate(glm::mat4(1),std::atan2(-forward.x,-forward.z),glm::vec3(0,1,0));origin_set=true;}
            auto early_prepare_start=Clock::now();if(state.shouldRender&&valid)scene.prepare(frame,settings.wider_view);
            auto early_prepare_end=Clock::now();
            auto control_start=Clock::now();controls(state.predictedDisplayTime,frame);
            auto control_end=Clock::now();
            XrCompositionLayerProjectionView submitted[2]={{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}};
            if(state.shouldRender&&valid) {
                auto prepare_start=Clock::now();scene.prepare(frame,settings.wider_view);if(frame)scene.set_presentation_lens(display_lens(*frame,settings));
                auto prepare_end=Clock::now();
                GameState native_state;{std::lock_guard lock(shared.mutex);native_state=shared.state;}
                for(int i=0;i<2;i++) {
                    auto& eye=eyes[i];uint32_t index{};XrSwapchainImageAcquireInfo ac{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};check(xrAcquireSwapchainImage(eye.swapchain,&ac,&index),"Acquire eye");
                    XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wi.timeout=XR_INFINITE_DURATION;check(xrWaitSwapchainImage(eye.swapchain,&wi),"Wait eye");
                    glBindFramebuffer(GL_FRAMEBUFFER,eye.fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,eye.colors[index],0);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,eye.depth);glViewport(0,0,eye.width,eye.height);
                    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);glClearColor(.02f,.02f,.025f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
                    auto vp=projection(views[i].fov)*glm::inverse(pose(views[i].pose))*origin;
                    if(!launch_splash.active())scene.draw(vp,settings.world_scale,settings.panel_distance,frame?frame_zoom(*frame,settings.game_zoom):1.f,effective_hud_size(settings.hud_size,settings.wider_view,native_state.valid&&native_state.playing&&!native_state.service&&!ui.service()));
                    double now=std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
                    ui.draw(vp,settings,native_state,frame&&frame->complete&&presentation_lens(*frame).valid()&&native_state.valid&&(native_state.ready||native_state.service),now,launch_splash.active(),frame.get());
                    glFlush();XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};check(xrReleaseSwapchainImage(eye.swapchain,&release),"Release eye");
                    submitted[i].pose=views[i].pose;submitted[i].fov=views[i].fov;submitted[i].subImage.swapchain=eye.swapchain;submitted[i].subImage.imageRect={{0,0},{int32_t(eye.width),int32_t(eye.height)}};
                }
                controls_ms+=std::chrono::duration<double,std::milli>(control_end-control_start).count();
                prepare_ms+=std::chrono::duration<double,std::milli>((prepare_end-prepare_start)+(early_prepare_end-early_prepare_start)).count();
                draw_ms+=std::chrono::duration<double,std::milli>(Clock::now()-prepare_end).count();
                if(++timing_frames==300){log("CPU frame ms: controls "+std::to_string(controls_ms/300)+", prepare "+std::to_string(prepare_ms/300)+", stereo submit "+std::to_string(draw_ms/300)+", wider view "+(settings.wider_view?"on":"off"));controls_ms=prepare_ms=draw_ms=0;timing_frames=0;}
            }
            XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};layer.space=local;layer.viewCount=2;layer.views=submitted;
            const XrCompositionLayerBaseHeader* layers[]={reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer)};
            XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};end.displayTime=state.predictedDisplayTime;end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;end.layerCount=state.shouldRender&&valid?1:0;end.layers=layers;check(xrEndFrame(session,&end),"EndFrame");
            if(state.shouldRender&&valid&&session_state==XR_SESSION_STATE_FOCUSED&&launch_splash.active()) {
                launch_splash.presented(std::chrono::nanoseconds(state.predictedDisplayPeriod));
                if(!launch_splash.active())log("VR4ALL splash finished");
            }
        }
        if(running)xrRequestExitSession(session);
    }
};
}
}
CV_EXPORT bool vcop2_xr_start(vcop2::SharedRuntime* shared) {
    using namespace vcop2;
    if(!shared||shared->abi!=SharedRuntime::Abi||worker.joinable()){
        std::cerr<<"Virtua Cop 2 XR rejected start: expected ABI "<<SharedRuntime::Abi<<", received "<<(shared?shared->abi:0)<<", worker active "<<worker.joinable()<<'\n';return false;
    }
    stopping=false;
#ifdef __ANDROID__
    emulation_thread=uint32_t(gettid());
#endif
    worker=std::thread([shared] {
        const char* file=std::getenv("VCOP2_XR_LOG");logfile.open(file?file:"vcop2-xr.log",std::ios::trunc);
        try {
#ifdef __ANDROID__
            JNIEnv* env{};if(!java_vm||java_vm->AttachCurrentThread(&env,nullptr)!=JNI_OK)throw std::runtime_error("Cannot attach headset thread to Java VM");
#endif
            if(std::getenv("VCOP2_XR_SIM"))simulation(*shared);
            else {Host host(*shared);host.init();host.run();}
        } catch(const std::exception& e) {log(e.what());shared->host_ready=false;shared->quit_requested=true;}
        log("Host stopped.");logfile.close();
#ifdef __ANDROID__
        if(java_vm)java_vm->DetachCurrentThread();
#endif
    });return true;
}
CV_EXPORT void vcop2_xr_stop() {
    using namespace vcop2;stopping=true;if(worker.joinable())worker.join();
#ifdef __ANDROID__
    JNIEnv* env{};
    if(java_vm&&activity&&java_vm->GetEnv(reinterpret_cast<void**>(&env),JNI_VERSION_1_6)==JNI_OK)env->DeleteGlobalRef(activity);
    activity=nullptr;
#endif
}
#ifdef __ANDROID__
CV_EXPORT void vcop2_xr_android_init(void* vm,void* activity) {
    using namespace vcop2;java_vm=static_cast<JavaVM*>(vm);JNIEnv* env{};
    if(java_vm&&java_vm->GetEnv(reinterpret_cast<void**>(&env),JNI_VERSION_1_6)==JNI_OK) {
        if(vcop2::activity)env->DeleteGlobalRef(vcop2::activity);
        vcop2::activity=env->NewGlobalRef(static_cast<jobject>(activity));
    }
}
#endif
