#include "ofMain.h"
#include "ofApp.h"

// openFrameworks wraps GLFW for desktop windows.  This project usually does not
// need to talk to GLFW directly, but on Linux we use one GLFW initialization hint
// before openFrameworks creates its window.
#if defined(TARGET_LINUX) && !defined(TARGET_EMSCRIPTEN)
#include <GLFW/glfw3.h>
#endif

int main() {
#if defined(TARGET_LINUX) && !defined(TARGET_EMSCRIPTEN) && defined(GLFW_PLATFORM) && defined(GLFW_PLATFORM_X11)
    // openFrameworks 0.12.1's Linux window class still calls X11-native GLFW
    // functions internally, such as glfwGetX11Display().  On KDE/Wayland,
    // GLFW 3.4 may otherwise auto-select its Wayland backend because
    // XDG_SESSION_TYPE=wayland.  That would make those X11 calls invalid and
    // crash at runtime.  This hint asks GLFW to create an X11 window instead;
    // under a Wayland desktop that means the app runs through XWayland.
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif

    // ofGLFWWindowSettings is the openFrameworks configuration object for the
    // native desktop window.  Web/Emscripten builds use ofGLESWindowSettings
    // instead because they render through WebGL.
#if defined(TARGET_EMSCRIPTEN)
    ofGLESWindowSettings settings;
    settings.glesVersion = 3;
#else
    ofGLFWWindowSettings settings;
#endif

    // Start in a 16:9 window that is large enough to show the layered waves, but
    // leave it resizable so the FBO and wave math can adapt to any monitor size.
    settings.setSize(1280, 720);
    settings.windowMode = OF_WINDOW;
    settings.resizable = true;
    settings.title = "Sinuous Waves";

#if !defined(TARGET_EMSCRIPTEN)
    // Request a programmable OpenGL 3.2 context.  The sketch does not use custom
    // shaders, but this is a common modern baseline for openFrameworks desktop
    // apps and matches the renderer path used by ofMesh and FBO drawing.
    settings.setGLVersion(3, 2);
#endif

    // Create the actual OS window, attach our ofApp instance to it, and hand
    // control to openFrameworks' main loop.  After ofRunMainLoop(), openFrameworks
    // calls setup() once, then update()/draw() every frame until the app exits.
    auto window = ofCreateWindow(settings);
    ofRunApp(window, std::make_shared<ofApp>());
    ofRunMainLoop();
}
