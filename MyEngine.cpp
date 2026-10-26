//============================================================
//  MyEngine
//  © 2026 FervidFiend. All rights reserved.
//
//  File: MyEngine.cpp
//  Description: Core engine interface
//
//  Permission is granted to use this source code for the
//  purpose of creating games or interactive applications.
//  Redistribution, sublicensing, or publication of this
//  engine or its source code, in whole or in part, is
//  prohibited without prior written consent.
//
//  Any game or application created using this engine must
//  include attribution to "MyEngine" and FervidFiend in
//  the credits or accompanying documentation.
//
//============================================================



#include "MyEngine.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#define NANOSVG_IMPLEMENTATION
#include <nanosvg/nanosvg.h>
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvg/nanosvgrast.h>

#include <charconv>
#include <string_view>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_DRIVER_H
#include FT_MODULE_H



namespace extras {
    OpenAL* openAL = nullptr;

    bool optimizeGLCalls = false; // skips redundant unbindings : disable for debug mode (convert some cpu time to optimize number of opengl calls) (roughly 10% slower for 50%+ reduction in gl calls)
    bool draw3DMirrored = false; // flips left to right (aka mirror)
    bool drawYDown = true; 

    bool useInverseZBuffer = false;

    bool initialized = false;
    GLFWwindow* ActiveWindow = nullptr;

    PerfLogger perfLogger;

    GLint LastSrcAlpha = -1;
    GLint LastDstAlpha = -1;
    GLint CurrentSrcAlpha = -1;
    GLint CurrentDstAlpha = -1;

    std::vector<DepthModes> LastActiveDepthStack;
    DepthModes CurrentActiveDepth = DepthModes::UNINITIALIZED;

    Shader Default2DShader = Shader("Default2DShader");
    Shader Default3DShader = Shader("Default3DShader");
    Shader DefaultSkyboxShader = Shader("DefaultSkyboxShader");
    Shader DefaultTextShader = Shader("DefaultTextShader");
    Shader BitmapTextShader = Shader("BitmapTextShader");
    Shader DefaultLineShader = Shader("DefaultLineShader");
    Shader Default2DBlurShader = Shader("Default2DBlurShader");
    Shader Default2DGeometryGlassShader = Shader("Default2DGeometryGlassShader");

    std::vector<Shader*> LastActiveShaderStack;
    Shader* ActiveShader = nullptr;
    int targetFps = 0;
    bool pauseOnWindowDrag = true;
    std::chrono::steady_clock::time_point nextFrameStartTime{};
    bool frameScheduleInitialized = false;
    bool WindowSizeChanged = false;
    bool WindowFrozen = false;

    size_t maxTextChars = 0;
    float fontScale = 1.0;
    Font* defaultFont;

    Text* defaultTextRenderer;

    double rawFreezeStartTime = glfwGetTime();
    //double startTime = glfwGetTime();
    double lastRawFrameTime = glfwGetTime();
    double lastFrameTime = glfwGetTime();

    double totalTimeFrozen = 0;

    double rawFrameDeltaTime = 0;
    double frameDeltaTime = 0;

    std::map<int, bool> keyState;
    std::map<int, std::map<int, bool>> buttonState;
    std::map<int, bool> lastMouseState;
    std::map<int, bool> mouseState;
    glm::vec2 lastMousePosition = { 0, 0 };
    glm::vec2 mouseWheelOffset;

    glm::vec2 CurrentMousePosition = { 0, 0 };
    glm::vec2 CurrentMouseScreenPosition = { 0, 0 };

    Camera2D* DefaultCamera = nullptr;
    Camera2D* activeCamera2D = nullptr;
    Camera3D* activeCamera3D = nullptr;

    int CurrentWindowWidth;
    int CurrentWindowHeight;

    bool WindowFocused = false;
    bool CaptureMouse = false;

    std::vector<keyCallbackData> keyCallbacksData;
    std::vector<unsigned int> codepointsData;

    void (*renderFunction)() = nullptr;
}

Rect::Rect() : position(glm::vec2(0, 0)), size(glm::vec2(0, 0)) {

}

Rect::Rect(glm::vec2 position, glm::vec2 size) : position(position), size(size) {

}

Rect::Rect(float x, float y, float sizeX, float sizeY) : position(glm::vec2(x, y)), size(glm::vec2(sizeX, sizeY)) {

}

glm::vec2 Rect::GetCenter() const {
    return position + (size * 0.5f);
}

glm::vec2 Rect::GetBottomLeft() {
    return position;
}

glm::vec2 Rect::GetBottomRight() {
    return position + glm::vec2(size.x, 0);
}

glm::vec2 Rect::GetTopLeft() {
    return position + glm::vec2(0, size.y);
}

glm::vec2 Rect::GetTopRight() {
    return position + size;
}


//Box::Box() : position(glm::vec3(0, 0, 0)), size(glm::vec3(0, 0, 0)) {
//
//}
//
//Box::Box(glm::vec3 position, glm::vec3 size) : position(position), size(size) {
//
//}







// Namespace to avoid conflicts
namespace CleanupHandler {
    BOOL WINAPI ConsoleHandler(DWORD event) {
        if (event == CTRL_CLOSE_EVENT || event == CTRL_SHUTDOWN_EVENT || event == CTRL_LOGOFF_EVENT) {
            OutputDebugString(L"Console window is closing! Performing cleanup...\n");

            delete extras::openAL;

            return TRUE; // Indicate that we handled the event
        }
        return FALSE;
    }
}

void setSourceDistanceModel(ALuint source, float referenceDistance, float rolloffFactor, float maxDistance) {
    alSourcef(source, AL_REFERENCE_DISTANCE, referenceDistance);
    alSourcef(source, AL_ROLLOFF_FACTOR, rolloffFactor);
    alSourcef(source, AL_MAX_DISTANCE, maxDistance);
}

// Function to translate OpenAL error codes to strings
const char* alErrorToString(ALenum error) {
    switch (error) {
    case AL_NO_ERROR:
        return "AL_NO_ERROR: No error";
    case AL_INVALID_NAME:
        return "AL_INVALID_NAME: Invalid name parameter";
    case AL_INVALID_ENUM:
        return "AL_INVALID_ENUM: Invalid parameter";
    case AL_INVALID_VALUE:
        return "AL_INVALID_VALUE: Invalid enum parameter value";
    case AL_INVALID_OPERATION:
        return "AL_INVALID_OPERATION: Illegal call";
    case AL_OUT_OF_MEMORY:
        return "AL_OUT_OF_MEMORY: Out of memory";
    default:
        return "Unknown OpenAL error";
    }
}

void printFormattedExtensions(const char* header, const char* extensions) {
    std::cout << header << std::endl;
    if (!extensions || extensions[0] == '\0') {
        return; // Early exit if no extensions to print
    }

    std::string extensionList = extensions;
    int lineLength = 0;

    const int indentation = 4;// indentation of 4 spaces
    const int maximumWidth = 80;

    // Start with indentation 
    std::cout << std::string(indentation, ' ');
    lineLength += indentation;

    // Replace spaces with commas and handle line wrapping
    for (char c : extensionList) {
        if (c == ' ') {
            if (lineLength >= maximumWidth) {
                std::cout << std::endl << std::string(indentation, ' '); // Indent new line
                lineLength = indentation; // Reset line length after indentation
            }
            else {
                std::cout << ", ";
                lineLength += 2; // Add for ", "
            }
        }
        else {
            std::cout << c;
            lineLength++;
        }
    }
    std::cout << std::endl;
}

void displayDevices(const char* type, const char* list) {
    const ALCchar* current = list;

    std::cout << "List of all available " << type << " devices:\n";
    if (*current == '\0') { // Check if the list is empty
        std::cout << "  None\n";
        return; // Exit the function if no devices are available
    }

    // Iterate through the null-separated device list
    while (*current != '\0') {
        std::cout << "  " << current << "\n";
        current += std::strlen(current) + 1; // Move to the start of the next device string
    }
}

void CheckALCErrors(ALCdevice* device) {
    ALenum error;
    error = alcGetError(device);
    if (error != ALC_NO_ERROR)
        std::cout << std::endl << "ALC Error " << error << " occurred: " << alcGetString(device, error) << std::endl;
}

// check for al errors
bool CheckALErrors() {
    if (extras::openAL == nullptr) {
        throw std::runtime_error("openAL is not initalized!");
    }

    ALenum error = alGetError();
    if (error != AL_NO_ERROR) {
        std::cerr << "OpenAL Error: " << alErrorToString(error) << std::endl;
        return true;
    }
    return false;
}

std::string prevDeviceList = "";

bool devicesChanged() {
    bool changed = false;
    if (alcIsExtensionPresent(nullptr, "ALC_enumeration_EXT") == AL_TRUE) {
        bool canEnumerateAll = alcIsExtensionPresent(nullptr, "ALC_enumerate_all_EXT") == AL_TRUE;
        const char* deviceList = (char*)alcGetString(nullptr, (canEnumerateAll ? ALC_ALL_DEVICES_SPECIFIER : ALC_DEVICE_SPECIFIER));
        std::string currDeviceList = std::string(deviceList);
        if (currDeviceList != prevDeviceList) {
            if (currDeviceList == "") {
                std::cerr << "All audio output devices lost!" << std::endl;
            }
            displayDevices("output", deviceList);
            changed = true;
        }
        prevDeviceList = deviceList;
    }

    return changed;
}

void listAllAudioOutputDevices() {
    if (alcIsExtensionPresent(nullptr, "ALC_enumeration_EXT") == AL_TRUE) {
        bool canEnumerateAll = alcIsExtensionPresent(nullptr, "ALC_enumerate_all_EXT") == AL_TRUE;
        const char* deviceList = (char*)alcGetString(nullptr, (canEnumerateAll ? ALC_ALL_DEVICES_SPECIFIER : ALC_DEVICE_SPECIFIER));
        displayDevices("output", deviceList);
    }
}

void listAllAudioInputDevices() {
    if (alcIsExtensionPresent(nullptr, "ALC_enumeration_EXT") == AL_TRUE) {
        const char* deviceList = alcGetString(nullptr, ALC_CAPTURE_DEVICE_SPECIFIER);
        displayDevices("input", deviceList);
    }
}

void printDefaultAudioOutputDevice(ALCdevice* device) {
    bool canEnumerateAll = alcIsExtensionPresent(nullptr, "ALC_enumerate_all_EXT") == AL_TRUE;
    const char* defaultDevice = (char*)alcGetString(device, (canEnumerateAll ? ALC_DEFAULT_ALL_DEVICES_SPECIFIER : ALC_DEFAULT_DEVICE_SPECIFIER));
    std::cout << "default output device: " << defaultDevice << std::endl;
    CheckALCErrors(device);
}

void printDefaultAudioInputDevice(ALCdevice* device) {
    bool captureSupported = alcIsExtensionPresent(device, "ALC_EXT_CAPTURE") == AL_TRUE;
    if (captureSupported) {
        const char* defaultDevice = (char*)alcGetString(device, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER);
        std::cout << "default input device:  " << defaultDevice << std::endl;
        CheckALCErrors(device);
    }
    std::cout << "capture support: " << ((captureSupported) ? "yes" : "no") << std::endl;
}

void printMiscALInfo(ALCdevice* device) {
    ALCint data[2] = { 0 };
    alcGetIntegerv(device, ALC_FREQUENCY, 1, &data[0]);
    std::cout << "mixer frequency: " << data[0] << " hz" << std::endl;
    CheckALCErrors(device);

    alcGetIntegerv(device, ALC_REFRESH, 1, &data[1]);
    if (data[1] != 0) std::cout << "Refresh rate: " << data[0] / data[1] << " Hz" << std::endl; // Prevent division by zero
    else std::cout << "Refresh rate: Undefined (division by zero)" << std::endl;
    CheckALCErrors(device);

    data[0] = 0;
    alcGetIntegerv(device, ALC_MONO_SOURCES, 1, &data[0]);
    if (alcGetError(device) == AL_NO_ERROR) {
        std::cout << "supported sources; mono: " << data[0];

        data[0] = 0;
        alcGetIntegerv(device, ALC_STEREO_SOURCES, 1, &data[0]);
        std::cout << ", stereo: " << data[0] << std::endl;
        CheckALCErrors(device);
    }
}

void printALCInfo(ALCdevice* device) {
    ALCint version[2] = { 0 };

    alcGetIntegerv(device, ALC_MAJOR_VERSION, 1, &version[0]);
    alcGetIntegerv(device, ALC_MINOR_VERSION, 1, &version[1]);
    std::cout << "ALC Version: " << version[0] << "." << version[1] << std::endl;
    CheckALCErrors(device);

    const char* extensions = alcGetString(device, ALC_EXTENSIONS);
    if (extensions) {
        printFormattedExtensions("ALC Extensions:", extensions);
    }
    else {
        std::cout << "ALC Extensions: None" << std::endl;
    }
    CheckALCErrors(device);
}

void printOpenALInfo() {
    const char* vendor = alGetString(AL_VENDOR);
    ALenum error = alGetError();
    if (error != AL_NO_ERROR) {
        std::cout << "Error retrieving OpenAL vendor string: #" << error << ": " << alGetString(error) << std::endl;
    }
    else {
        std::cout << "OpenAL vendor string: " << vendor << std::endl;
    }

    const char* renderer = alGetString(AL_RENDERER);
    error = alGetError();
    if (error != AL_NO_ERROR) {
        std::cout << "Error retrieving OpenAL renderer string: #" << error << ": " << alGetString(error) << std::endl;
    }
    else {
        std::cout << "OpenAL renderer string: " << renderer << std::endl;
    }

    const char* version = alGetString(AL_VERSION);
    error = alGetError();
    if (error != AL_NO_ERROR) {
        std::cout << "Error retrieving OpenAL version string: #" << error << ": " << alGetString(error) << std::endl;
    }
    else if (!version) {
        std::cout << "Querying AL_VERSION returned a nullptr pointer!" << std::endl;
    }
    else {
        std::cout << "OpenAL version string: " << version << std::endl;
    }

    const char* extensions = alGetString(AL_EXTENSIONS);
    error = alGetError();
    if (error != AL_NO_ERROR) {
        std::cout << "Error retrieving OpenAL extensions: #" << error << ": " << alGetString(error) << std::endl;
    }
    else {
        printFormattedExtensions("OpenAL extensions:", extensions);
    }
}

OpenAL::OpenAL() {
    SetupOpenAL();
}

OpenAL::~OpenAL() {
    CloseOpenAL();
}

void OpenAL::SetupOpenAL() {
    std::cout << "============OpenAL Setup============" << std::endl;
    // Set the console control handler to handle visual studio console close events
    if (!SetConsoleCtrlHandler(CleanupHandler::ConsoleHandler, TRUE)) {
        std::cerr << "Error: Could not set control handler." << std::endl;
        exit(EXIT_FAILURE);
    }

    listAllAudioOutputDevices();
    listAllAudioInputDevices();

    device = alcOpenDevice(nullptr); // Open default device
    if (!device) {
        std::cerr << "Unable to open default device" << std::endl;
        return;
        //exit(EXIT_FAILURE);
    }

    context = alcCreateContext(device, nullptr);
    if (!context) {
        std::cerr << "Failed to create context" << std::endl;
        alcCloseDevice(device);
        exit(EXIT_FAILURE);
    }
    alcMakeContextCurrent(context);
    CheckALCErrors(device);

    printDefaultAudioOutputDevice(device);
    printDefaultAudioInputDevice(device);
    printMiscALInfo(device);
    printALCInfo(device);
    printOpenALInfo();

    if (alcIsExtensionPresent(nullptr, "ALC_enumeration_EXT") == AL_FALSE) {
        std::cerr << "Can not detect audio connection changes!" << std::endl;
    }

    //// Check for EFX extension
    //if (!alcIsExtensionPresent(CleanupHandler::device, "ALC_EXT_EFX")) {
    //    throw std::runtime_error("EFX extension not supported.");
    //}
    //
    //// Set volume distance model
    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
    ////alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
    //setListenerProperties(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    openAlLoaded = true;
}

void OpenAL::CloseOpenAL() {
    closing = true;

    // Clean up sounds
    for (SoundEffect* s : loadedSounds) {
        if (s) s->CleanupSound();
    }
    loadedSounds.clear();

    // Clean up music - No more "Inaccessible" errors!
    for (MusicSound* m : loadedMusicSounds) {
        if (m) m->CleanupMusic();
    }
    loadedMusicSounds.clear();

    // Final hardware shutdown
    if (context) {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(context);
        context = nullptr;
    }
    if (device) {
        alcCloseDevice(device);
        device = nullptr;
    }
    openAlLoaded = false;
}

void OpenAL::setListenerProperties(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up) {
    alListener3f(AL_POSITION, position.x, position.y, position.z);
    listenerPosition = position;
    ALfloat orientation[] = { forward.x, forward.y, forward.z, up.x, up.y, up.z };
    alListenerfv(AL_ORIENTATION, orientation);
}

// Interactions and Overall Workflow
// 
// Local space(or Object space) -> to World Space : 
// World Space to View Space : Objects are positioned in the world and transformed to the camera's view space using the view matrix.                                                    (determines the camera-relative positions of objects)
// View Space to Clip Space : The projection matrix takes these coordinates and transforms them to clip space, determining what falls inside the camera's frustum.                      (determines visibility)
// Clip Space to NDC to Window Coordinates : After clipping and perspective division, coordinates are in NDC and are mapped to actual screen pixels through the glViewport settings.    (determines where on screen)


// Camera2D class Functions
Camera2D::Camera2D(glm::vec2 offset, glm::vec2 target, float rotation, glm::vec2 zoom) : offset(offset), target(target), rotation(rotation), zoom(zoom), currentViewportSize(glm::vec2(extras::CurrentWindowWidth, extras::CurrentWindowHeight)) {}

//glm::mat4 Camera2D::GetViewMatrix() const {
//    glm::mat4 view = glm::mat4(1.0f);
//    view = glm::translate(view, glm::vec3(offset, 0.0f));
//    view = glm::rotate(view, glm::radians(-rotation), glm::vec3(0.0f, 0.0f, 1.0f));
//    //view = glm::scale(view, glm::vec3(zoom.x, zoom.y, 1.0f));
//    view = glm::translate(view, glm::vec3(-target, 0.0f));
//    return view;
//}
//
//glm::mat4 Camera2D::GetProjectionMatrix() const {
//    return glm::ortho(0.0f, currentViewportSize.x / zoom.x, 0.0f, currentViewportSize.y / zoom.y, -1000.0f, 1000.0f);
//}
//
//glm::mat4 Camera2D::GetInverseViewMatrix() const {
//    glm::mat4 view = glm::mat4(1.0f);
//    view = glm::translate(view, glm::vec3(target, 0.0f));
//    //view = glm::scale(view, glm::vec3(1.0f / zoom.x, 1.0f / zoom.y, 1.0f));
//    view = glm::rotate(view, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f));
//    view = glm::translate(view, glm::vec3(-offset, 0.0f));
//    return view;
//}
//
//glm::mat4 Camera2D::GetInverseProjectionMatrix() const {
//    return glm::ortho(0.0f, currentViewportSize.x * zoom.x, 0.0f, currentViewportSize.y * zoom.y, -1000.0f, 1000.0f);
//}

glm::mat4 Camera2D::GetViewMatrix() const {
    glm::mat4 view = glm::mat4(1.0f);
    view = glm::translate(view, glm::vec3(offset, 0.0f));
    view = glm::rotate(view, glm::radians(-rotation), glm::vec3(0.0f, 0.0f, 1.0f));
    view = glm::scale(view, glm::vec3(zoom.x, zoom.y, 1.0f));
    view = glm::translate(view, glm::vec3(-target, 0.0f));
    if (rotated) {
        view = glm::rotate(view, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));  // for top down
    }
    //view[0] = glm::round(view[0]);
    //view[1] = glm::round(view[1]);
    //view[2] = glm::round(view[2]);
    //view[3] = glm::floor(view[3]);
    return view;
}

glm::mat4 Camera2D::GetProjectionMatrix() const {
    //return glm::ortho(0.0f, float(extras::CurrentWindowWidth), 0.0f, float(extras::CurrentWindowHeight), -1000.0f, 1000.0f);
    if (extras::drawYDown) {
        return glm::ortho(0.0f, currentViewportSize.x, currentViewportSize.y, 0.0f, -1000.0f, 1000.0f);
    }
    return glm::ortho(0.0f, currentViewportSize.x, 0.0f, currentViewportSize.y, -1000.0f, 1000.0f);
}

glm::mat4 Camera2D::GetInverseViewMatrix() const {
    glm::mat4 view = glm::mat4(1.0f);
    view = glm::translate(view, glm::vec3(target, 0.0f));
    view = glm::scale(view, glm::vec3(1.0f / zoom.x, 1.0f / zoom.y, 1.0f));
    view = glm::rotate(view, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f));
    view = glm::translate(view, glm::vec3(-offset, 0.0f));
    return view;
}

glm::mat4 Camera2D::GetInverseProjectionMatrix() const {
    return glm::inverse(GetProjectionMatrix());
}

void Camera2D::CenterViewportInWindow() {
    offset = currentViewportSize / 2.0f;
    target = currentViewportSize / 2.0f;
}




TopDown2DCameraController::TopDown2DCameraController() {
    camera.CenterViewportInWindow();
}

void TopDown2DCameraController::Update(int button) {
    glm::vec2 md = GetMouseDelta();
    float mw = GetMouseWheelMove();

    if (mw > 0) {
        camera.zoom *= 1.5;
    }
    else if (mw < 0) {
        camera.zoom /= 1.5;
    }

    if (IsMouseButtonDown(button)) {
        camera.target -= GetMouseToScreen(md, &camera) / camera.zoom;
    }
}



// Camera3D class Functions
Camera3D::Camera3D(glm::vec3 position, glm::vec3 target, glm::vec3 offset, glm::vec3 worldUp, float fovy, float nearClip, float farClip)
    : position(position), target(target), offset(offset), worldUp(worldUp), fovy(fovy), nearClip(nearClip), farClip(farClip) {

    // 1. Compute the direction the camera is looking
    // Handle the edge case where position and target are identical
    if (position == target) {
        forward = glm::vec3(0.0f, 0.0f, -1.0f);
    }
    else {
        forward = glm::normalize(target - position);
    }

    // 2. Compute the correct RIGHT vector using worldUp
    // Cross product gives a vector perpendicular to both forward and worldUp
    right = glm::normalize(glm::cross(forward, worldUp));

    // 3. Compute the true camera UP vector
    // Cross product between right and forward gives the exact local up direction
    up = glm::normalize(glm::cross(right, forward));
}

glm::mat4 Camera3D::GetViewMatrix() const {
    return glm::lookAt(position, target, up);
}

glm::mat4 Camera3D::GetProjectionMatrix() const {
    if (extras::CurrentWindowWidth == 0 || extras::CurrentWindowHeight == 0) { // window is minimized or smthn idk
        return glm::mat4(1.0f);
    }

    float aspect = float(extras::CurrentWindowWidth) / float(extras::CurrentWindowHeight);
    glm::mat4 projection;
    if (extras::useInverseZBuffer) {
        float f = 1.0f / tan(glm::radians(fovy) / 2.0f);
        projection = glm::mat4(0.0f);

        projection[0][0] = f / aspect;
        projection[1][1] = f;
        projection[2][2] = 0.0f;          // Far plane at 0
        projection[2][3] = -1.0f;         // Standard RH projection
        projection[3][2] = nearClip;      // Near plane at 1 (after W-divide)
    }
    else {
        projection = glm::perspective(glm::radians(fovy), aspect, nearClip, farClip);
    }
    if (extras::draw3DMirrored) {
        projection[0][0] *= -1;
    }
    return projection;
}

void Camera3D::update(glm::quat orientation, glm::vec3 newPos) {
    position = newPos + offset;

    forward = orientation * glm::vec3(0, 0, -1);
    up = orientation * glm::vec3(0, 1, 0);
    right = orientation * glm::vec3(1, 0, 0);

    target = position + forward;
}


// Define static members for RNG
std::random_device RandomDevice::rd;
std::mt19937_64 RandomDevice::gen(RandomDevice::rd());

void RandomDevice::reseed(uint64_t seed) {
    gen.seed(seed);
}


Clock::Clock() {
    restart();
}

double Clock::getRawElapsedTime() {
    double frameTime = GetRawTime() - rawStartTime; // Calculate the time difference between now and the last recorded time
    return frameTime; // Return the calculated frame time
}

double Clock::getElapsedTime() {
    double frameTime = GetTime() - startTime; // Calculate the time difference between now and the last recorded time
    return frameTime; // Return the calculated frame time
}

void Clock::restart() {
    rawStartTime = GetRawTime();
    startTime = GetTime();
}


double PerfLogger::GetFps() {
    if (times.empty() || total <= 0.0) return 0.0;
    return double(times.size()) / total;
}

void PerfLogger::Update(double dt) {
    // Add new frame time
    times.push_back(dt);
    total += dt;

    // Maintain a rolling buffer close to 1 second
    while (total - times.front() > 1.0) {
        total -= times.front();
        times.erase(times.begin());
    }
}

void PerfLogger::PrintStatistics() {
    if (times.empty()) return;

    // Calculate statistics using a sorted copy so live samples stay chronological.
    std::vector<double> sortedTimes = times;
    std::sort(sortedTimes.begin(), sortedTimes.end());
    double average = total / double(sortedTimes.size());
    double avgFPS = 1.0 / average;
    double minMS = sortedTimes.front() * 1000.0;
    double maxFPS = 1.0 / (minMS / 1000.0);

    // Calculating percentiles
    double p1MS = sortedTimes[sortedTimes.size() * 0.01] * 1000.0; // 1% percentile
    double p1FPS = 1.0 / (p1MS / 1000.0);
    double p99MS = sortedTimes[sortedTimes.size() * 0.99] * 1000.0; // 99% percentile
    double p99FPS = 1.0 / (p99MS / 1000.0);

    double maxMS = sortedTimes.back() * 1000.0;
    double minFPS = 1.0 / (maxMS / 1000.0);

    double avgMS = average * 1000.0;

    // Printing the results
    std::cout << "\n--- Performance Statistics ---\n";
    std::cout << "Min MS: " << minMS << ", Max FPS: " << maxFPS << "\n";
    std::cout << "1% Avg MS: " << p1MS << ", 1% Avg FPS: " << p1FPS << "\n";
    std::cout << "99% Avg MS: " << p99MS << ", 99% Avg FPS: " << p99FPS << "\n";
    std::cout << "Max MS: " << maxMS << ", Min FPS: " << minFPS << "\n";
    std::cout << "Avg MS: " << avgMS << ", Avg FPS: " << avgFPS << "\n";
    std::cout << "-------------------------------\n";
}



// external call to extras::perfLogger.GetFps();
double GetFps() {
    return extras::perfLogger.GetFps();
}


// Initialize GLFW
void SetupGLFW() {
    std::cout << "============GLFW Setup============" << std::endl;
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        exit(EXIT_FAILURE);
    }
}

#ifdef USEGLEW
// Initialize GLEW
void SetupGLEW() {
    std::cout << "============GLEW Setup============" << std::endl;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cout << "glewInit failed: " << glewGetErrorString(err) << std::endl;
        std::cerr << "Failed to initialize GLEW\n";
        exit(EXIT_FAILURE);
    }
}
#endif

#ifdef USEGLAD
// Initalize GLAD
void SetupGLAD() {
    std::cout << "============GLAD Setup============" << std::endl;
    int version = gladLoadGL();
    if (version == 0) {
        std::cout << "gladLoadGL failed: " << getGLErrorString(0) << std::endl;
        std::cerr << "Failed to initialize GLAD\n";
        exit(EXIT_FAILURE);
    }
}
#endif

// Setup OpenCL
//void SetupCL() {
//    std::cout << "============OpenCL Setup============" << std::endl;
//    cl::Device device = cl::Device::getDefault();
//    cl::Context context = createSharedContext(device);
//    cl::CommandQueue queue(context, device);
//}



void InitLib() {
    SetupGLFW();
    std::cout << "GLFW Version: " << glfwGetVersionString() << "\n";
    extras::initialized = true;
}

std::atomic<bool> modalLoopRunning(false);
//std::mutex callbackMutex;

void CustomCallback() {
    //std::lock_guard<std::mutex> lock(callbackMutex);
    //std::cout << "Custom callback triggered during modal loop" << std::endl;

    // Custom callback logic
    if (extras::WindowFrozen == false) {
        extras::WindowFrozen = true;
        extras::rawFreezeStartTime = GetRawTime();
        extras::rawFreezeStartTime -= 0.051;
    }
}

void MonitorModalLoop() {
    Clock modalLoopClock;
    while (extras::ActiveWindow != nullptr) {//!WindowShouldClose()) {
        if (modalLoopRunning) {
            if (modalLoopClock.getElapsedTime() > 0.05) { // modal (event) loop took longer than 0.05s to respond, window is likely frozen, call callback
                CustomCallback();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));  // Adjust the sleep duration as needed
                modalLoopClock.restart();
            }
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));  // More frequent checks when not in modal loop
            modalLoopClock.restart();
        }
    }
}

void printOpenGLInfo() {
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "OpenGL Vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
}

void printSystemInfo() {
    GLint maxImageUnits;
    glGetIntegerv(GL_MAX_IMAGE_UNITS, &maxImageUnits);
    std::cout << "Maximum supported image units: " << maxImageUnits << std::endl;
}

void printActiveBufferSpecs() {
    GLint redBits, greenBits, blueBits, alphaBits, depthBits, stencilBits;

    // Get color buffer information
    glGetIntegerv(GL_RED_BITS, &redBits);
    glGetIntegerv(GL_GREEN_BITS, &greenBits);
    glGetIntegerv(GL_BLUE_BITS, &blueBits);
    glGetIntegerv(GL_ALPHA_BITS, &alphaBits);

    // Get depth buffer information
    glGetIntegerv(GL_DEPTH_BITS, &depthBits);

    // Get stencil buffer information
    glGetIntegerv(GL_STENCIL_BITS, &stencilBits);

    // Print out the buffer details
    std::cout << "Color buffer bits (RGBA): " << redBits << ", " << greenBits << ", "
        << blueBits << ", " << alphaBits << std::endl;
    std::cout << "Depth buffer bits: " << depthBits << std::endl;
    std::cout << "Stencil buffer bits: " << stencilBits << std::endl;

    // Additional details can be added here such as double buffering, stereo view, etc.
    GLboolean doubleBuffer;
    glGetBooleanv(GL_DOUBLEBUFFER, &doubleBuffer);
    std::cout << "Double buffering is " << (doubleBuffer ? "enabled" : "disabled") << std::endl;

    GLboolean stereo;
    glGetBooleanv(GL_STEREO, &stereo);
    std::cout << "Stereo is " << (stereo ? "enabled" : "disabled") << std::endl;
}

// Start the thread to monitor modal loop and run custom callback
std::thread modalLoopThread;

GLFWwindow* InitWindow(int width, int height, const char* title) {
    if (!extras::initialized) {
        InitLib();
    }

    // Request a depth buffer
    //glfwWindowHint(GLFW_DEPTH_BITS, 0); // works : can only set to 0 or system default

    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    // Create a GLFW window
    GLFWwindow* window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        const char* description;
        int code = glfwGetError(&description);
        if (code != GLFW_NO_ERROR) {
            std::cout << description << std::endl;
        }
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(window);
    SetCurrentActiveWindow(window);

#ifdef USEGLEW
    SetupGLEW(); // default opengl loader
#endif
#ifdef USEGLAD
    SetupGLAD(); // alternative opengl loader
#endif

    // Successfully loaded OpenGL
    printOpenGLInfo();
    printSystemInfo();
    printActiveBufferSpecs();

    if (enet_initialize() != 0) {
        fprintf(stderr, "An error occurred while initializing ENet.\n");
        exit(EXIT_FAILURE);
    }

    // Initialize FreeImage
    //FreeImage_Initialise();

    //perfLogger = PerfLogger;

    //extras::TargetViewportWidth = width;
    //extras::TargetViewportHeight = height;
    //extras::CurrentViewportWidth = width;
    //extras::CurrentViewportHeight = height;

    extras::CurrentWindowWidth = width;
    extras::CurrentWindowHeight = height;

    modalLoopThread = std::thread(MonitorModalLoop);

    // set window icon
    Image icon("resources/icon.png", false);
    glfwSetWindowIcon(extras::ActiveWindow, 1, &icon.image);

    glfwSetWindowFocusCallback(window, window_focus_callback);
    glfwSetScrollCallback(window, mouse_wheel_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCharCallback(window, char_callback);

    glfwSetWindowSizeCallback(window, window_size_callback);
    glfwSetWindowPosCallback(window, window_pos_callback);

    //glfwSetWindowRefreshCallback(extras::ActiveWindow, window_refresh_callback);
    //glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    extras::WindowFocused = true;

    extras::DefaultCamera = new Camera2D();
    extras::activeCamera2D = extras::DefaultCamera;
    //SetUniforms2D();

    LoadDefaultShaders(); // calls -> BeginShaderMode(extras::Default2DShader);
    //BeginShaderMode(extras::Default2DShader);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Disable byte-alignment restriction

    BeginDepthMode(DepthModes::LESS); // default depth
    //
    //if (extras::CurrentActiveDepth == DepthModes::UNINITIALIZED) {
    //    if (extras::useInverseZBuffer == true) {
    //        BeginInverseZ();
    //    }
    //    else {
    //        EndInverseZ();
    //    }
    //}
    glDisable(GL_DEPTH_TEST); // Disable depth testing for Default 2D rendering

    glFrontFace(GL_CCW); // This is the default in OpenGL
    glCullFace(GL_BACK); // This is the default in OpenGL
    glDisable(GL_CULL_FACE); // Disable back-face culling

    //glEnable(GL_DEPTH_TEST);
    //glDisable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    extras::CurrentSrcAlpha = GL_SRC_ALPHA;
    extras::CurrentDstAlpha = GL_ONE_MINUS_SRC_ALPHA;
    BeginBlendingMode(extras::CurrentSrcAlpha, extras::CurrentDstAlpha); // default blending


    SetupGlobalGLElements();

    InitOpenGLResources();
    //initializeFontLibraryAndFace();

    extras::fontScale = 2.0;
    extras::defaultFont = new Font;
    extras::defaultFont->loadFromFile("resources/defaultFont.ttf");

    extras::defaultTextRenderer = new Text;

    extras::openAL = new OpenAL(); // calls -> SetupOpenAL();

    GetMouseDelta();

    extras::lastRawFrameTime = GetRawTime();
    extras::lastFrameTime = GetTime();

    return window;
}

void CloseWindow() {
    CleanupGlobalGLElements();
    glfwTerminate();
    if (extras::openAL != nullptr);
    delete extras::openAL;
    extras::ActiveWindow = nullptr;
    modalLoopThread.join();
    enet_deinitialize();
    //FreeImage_DeInitialise();
}

void EnableVsync() {
    glfwSwapInterval(1);
}

void DisableVsync() {
    glfwSwapInterval(0);
}

// Get true elapsed time in seconds since InitWindow()
double GetRawTime() {
    return glfwGetTime();// -extras::startTime;
}

// Get elapsed time in seconds since InitWindow()
double GetTime() {
    return glfwGetTime() - extras::totalTimeFrozen;// (extras::startTime + extras::totalTimeFrozen);
}

void _GetFrameTime() {
    if (extras::WindowFrozen == true && extras::pauseOnWindowDrag) {
        double frozeTime = GetRawTime() - extras::rawFreezeStartTime;
        extras::totalTimeFrozen += frozeTime;
    }
    extras::rawFrameDeltaTime = GetRawTime() - extras::lastRawFrameTime; // Calculate the time difference between now and the last recorded time
    extras::lastRawFrameTime = GetRawTime();

    extras::frameDeltaTime = GetTime() - extras::lastFrameTime; // Calculate the time difference between now and the last recorded time
    extras::lastFrameTime = GetTime();

    extras::perfLogger.Update(extras::frameDeltaTime);
}

// Get raw time in seconds for last frame drawn (delta time)
double GetRawFrameTime() {
    return extras::rawFrameDeltaTime; // Return the calculated frame time
}

// Get time in seconds for last frame drawn (delta time)
double GetFrameTime() {
    return extras::frameDeltaTime; // Return the calculated frame time
}

bool WindowSizeChanged() {
    return extras::WindowSizeChanged;
}

glm::vec2 GetCurrentWindowSize() {
    return glm::vec2(extras::CurrentWindowWidth, extras::CurrentWindowHeight);
}

// Set target FPS (maximum)
void SetTargetFPS(int fps) {
    if (fps <= 0) {
        DisableVsync();
        extras::targetFps = 0;
        extras::frameScheduleInitialized = false;
        return;
    }
    extras::targetFps = fps;
    extras::frameScheduleInitialized = false;
}

void SetPauseOnWindowDrag(bool pause) {
    extras::pauseOnWindowDrag = pause;
}

void LimitFPS() {
    if (extras::targetFps <= 0) {
        extras::frameScheduleInitialized = false;
        return;
    }

    const auto targetFrameDuration = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(1.0 / static_cast<double>(extras::targetFps)));
    const auto now = std::chrono::steady_clock::now();

    if (!extras::frameScheduleInitialized) {
        extras::nextFrameStartTime = now;
        extras::frameScheduleInitialized = true;
        return;
    }

    extras::nextFrameStartTime += targetFrameDuration;
    if (extras::nextFrameStartTime < now) {
        // A slow frame missed its slot. Restart the schedule instead of catching up.
        extras::nextFrameStartTime = now;
        return;
    }

    const auto sleepDeadline = extras::nextFrameStartTime - std::chrono::milliseconds(1);
    if (std::chrono::steady_clock::now() < sleepDeadline) {
        std::this_thread::sleep_until(sleepDeadline);
    }

    // Busy-wait only for the final millisecond to avoid scheduler wake-up jitter.
    while (std::chrono::steady_clock::now() < extras::nextFrameStartTime) {
    }
}


void SetCurrentActiveWindow(GLFWwindow* window) {
    extras::ActiveWindow = window;
}

void CheckActiveWindowSet() {
    if (extras::ActiveWindow == nullptr) {
        std::cerr << "no ActiveWindow set" << std::endl;
        exit(EXIT_FAILURE);
    }
}

void DumpCurrentGlDataToConsole() {
    std::cerr << "\n--- [DEBUG] OpenGL State Dump ---" << std::endl;

    // 1. Shader Program Check
    GLint currentProg;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);

    if (extras::ActiveShader != nullptr) {
        std::cerr << "  Active Shader Name: " << extras::ActiveShader->name << std::endl;
        std::cerr << "  Internal Program ID: " << extras::ActiveShader->shader << std::endl;
        std::cerr << "  Hardware Program ID: " << currentProg << std::endl;

        if ((GLuint)currentProg != extras::ActiveShader->shader) {
            std::cerr << "  !!! MISMATCH: ActiveShader pointer does not match GL_CURRENT_PROGRAM !!!" << std::endl;
        }
    }
    else {
        std::cerr << "  !!! NO ACTIVE SHADER POINTER IN EXTRAS !!! (GL Current: " << currentProg << ")" << std::endl;
    }

    // 2. VAO / VBO / EBO Check
    GLint vao, vbo, ebo;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &vbo);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &ebo);
    std::cerr << "  Bound VAO: " << vao << std::endl;
    std::cerr << "  Bound VBO (GL_ARRAY_BUFFER): " << vbo << std::endl;
    std::cerr << "  Bound EBO (GL_ELEMENT_ARRAY_BUFFER): " << ebo << std::endl;

    // 3. Texture Unit Audit
    std::cerr << "  --- Texture Units ---" << std::endl;
    for (int i = 0; i < 4; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        GLint bound2D;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound2D);

        if (bound2D > 0) {
            std::cerr << "    Unit " << i << ": [2D: " << bound2D << "]";
            if (extras::ActiveShader && extras::ActiveShader->boundTextureUnits.count(i)) {
                GLuint internalID = extras::ActiveShader->boundTextureUnits[i];
                if (internalID != (GLuint)bound2D) {
                    std::cerr << " <-- !!! STRUCT MISMATCH (Expected: " << internalID << ")";
                }
            }
            std::cerr << std::endl;
        }
    }

    // 4. Pipeline State & Inverse-Z Audit
    std::cerr << "  --- Pipeline & Depth State ---" << std::endl;
    bool isInvZ = extras::useInverseZBuffer;
    std::cerr << "  INVERSE_Z: " << (isInvZ ? "ENABLED" : "DISABLED") << std::endl;

    // Blending
    GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    std::cerr << "  GL_BLEND: " << (blendEnabled ? "ENABLED" : "DISABLED") << std::endl;

    // Depth Test & Mask
    GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean depthMaskEnabled;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskEnabled);
    std::cerr << "  GL_DEPTH_TEST: " << (depthEnabled ? "ENABLED" : "DISABLED") << std::endl;
    std::cerr << "  GL_DEPTH_MASK (Writing): " << (depthMaskEnabled ? "ENABLED" : "DISABLED") << std::endl;

    // Depth Function (Critical for Inverse-Z)
    GLint depthFunc;
    glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
    std::string funcName;
    bool funcCorrect = false;

    switch (depthFunc) {
    case GL_GREATER:
        funcName = "GL_GREATER";
        funcCorrect = isInvZ; // Correct only if Inverse-Z is on
        break;
    case GL_GEQUAL:
        funcName = "GL_GEQUAL";
        funcCorrect = isInvZ; // Correct only if Inverse-Z is on
        break;
    case GL_LESS:
        funcName = "GL_LESS (Default)";
        funcCorrect = !isInvZ; // Correct only if Inverse-Z is off
        break;
    case GL_LEQUAL:
        funcName = "GL_LEQUAL";
        funcCorrect = !isInvZ; // Correct only if Inverse-Z is off
        break;
    default:
        funcName = std::to_string(depthFunc);
        funcCorrect = false;
        break;
    }
    std::cerr << "  GL_DEPTH_FUNC: " << funcName << (funcCorrect ? " [CORRECT]" : " [!!! ERROR !!!]") << std::endl;

    // Depth Clear Value Audit
    GLfloat depthClear;
    glGetFloatv(GL_DEPTH_CLEAR_VALUE, &depthClear);
    bool clearCorrect = (isInvZ ? depthClear == 0.0f : depthClear == 1.0f);
    std::cerr << "  GL_DEPTH_CLEAR_VALUE: " << depthClear << (clearCorrect ? " [CORRECT]" : " [!!! ERROR !!!]") << std::endl;

    // Clip Control Audit
    GLint clipOrigin, clipDepth;
    glGetIntegerv(GL_CLIP_ORIGIN, &clipOrigin);
    glGetIntegerv(GL_CLIP_DEPTH_MODE, &clipDepth);
    bool clipCorrect = (isInvZ ? clipDepth == GL_ZERO_TO_ONE : clipDepth == GL_NEGATIVE_ONE_TO_ONE);
    std::string clipLabel = (clipDepth == GL_ZERO_TO_ONE) ? "0 to 1" : "-1 to 1";
    std::cerr << "  Clip Range: " << clipLabel << (clipCorrect ? " [CORRECT]" : " [!!! ERROR !!!]") << std::endl;

    // 5. Scissor/Viewport
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    std::cerr << "  Viewport: " << viewport[0] << ", " << viewport[1] << ", " << viewport[2] << "x" << viewport[3] << std::endl;

    std::cerr << "-------------------------\n" << std::endl;
    glActiveTexture(GL_TEXTURE0);
}

// check for gl errors
bool CheckGLErrors() {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        DumpCurrentGlDataToConsole();
        std::cerr << "OpenGL Error: " << getGLErrorString(error) << std::endl;
        //std::cerr << "OpenGL Error: " << gluErrorString(error) << std::endl;
        return true;
    }
    return false;
}

// Check collision between two circles
bool CheckCollisionCircles(glm::vec2 center1, float radius1, glm::vec2 center2, float radius2) {
    // Calculate the distance between the centers
    float distance = glm::distance(center1, center2);

    // Check if the distance is less than or equal to the sum of the radii
    return distance <= (radius1 + radius2);
}

// Function to check for intersection between two line segments
bool CheckLineIntersection(const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& q1, const glm::vec2& q2, glm::vec2& outIntersection, float& outT) {
    glm::vec2 r = p2 - p1;
    glm::vec2 s = q2 - q1;

    float rxs = r.x * s.y - r.y * s.x;
    float q_pxs = (q1 - p1).x * s.y - (q1 - p1).y * s.x;

    if (fabs(rxs) < 1e-6f)
        return false; // Parallel or collinear

    float t = q_pxs / rxs;
    float u = ((q1 - p1).x * r.y - (q1 - p1).y * r.x) / rxs;

    // Intersection occurs within both segments
    if (t >= 0.0f && t <= 1.0f &&
        u >= 0.0f && u <= 1.0f) {

        outIntersection = p1 + t * r;
        outT = t;
        return true;
    }

    return false;
}

// Function to check collision between a line segment and a rectangle
bool CheckCollisionLineRect(const glm::vec2& start, const glm::vec2& end, const Rect& rect, glm::vec2& intersectionPoint) {
    float closestT = FLT_MAX;
    bool hit = false;

    auto testEdge = [&](const glm::vec2& a, const glm::vec2& b) {
        glm::vec2 intersection;
        float t;

        if (CheckLineIntersection(start, end, a, b, intersection, t)) {
            if (t < closestT) {
                closestT = t;
                intersectionPoint = intersection;
                hit = true;
            }
        }
        };

    glm::vec2 bl = rect.position;
    glm::vec2 br = rect.position + glm::vec2(rect.size.x, 0);
    glm::vec2 tl = rect.position + glm::vec2(0, rect.size.y);
    glm::vec2 tr = rect.position + rect.size;

    testEdge(tl, tr);
    testEdge(tr, br);
    testEdge(br, bl);
    testEdge(bl, tl);

    // If the ray starts inside the rect
    if (!hit &&
        start.x >= rect.position.x &&
        start.x <= rect.position.x + rect.size.x &&
        start.y >= rect.position.y &&
        start.y <= rect.position.y + rect.size.y) {

        intersectionPoint = start;
        return true;
    }

    return hit;
}

// Function to check collision between a line segment and a circle
bool CheckCollisionLineCircle(const glm::vec2& start, const glm::vec2& end, const glm::vec2& circleCenter, float radius) {
    // Vector from start to end
    glm::vec2 d = end - start;

    // Vector from start to circle center
    glm::vec2 f = start - circleCenter;

    float a = glm::dot(d, d);
    float b = 2 * glm::dot(f, d);
    float c = glm::dot(f, f) - radius * radius;

    float discriminant = b * b - 4 * a * c;
    if (discriminant < 0) {
        // No intersection
        return false;
    }
    else {
        // Check the intersection points
        discriminant = std::sqrt(discriminant);

        float t1 = (-b - discriminant) / (2 * a);
        float t2 = (-b + discriminant) / (2 * a);

        if (t1 >= 0 && t1 <= 1) {
            // Intersection at t1
            return true;
        }

        if (t2 >= 0 && t2 <= 1) {
            // Intersection at t2
            return true;
        }

        // The line does intersect the circle, but the segment does not
        return false;
    }
}

// Function to check if a line segment intersects with a triangle and finds the intersection point
bool CheckCollisionLineTriangle(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& triA, const glm::vec3& triB, const glm::vec3& triC, glm::vec3& intersectionPoint) {
    // Calculate triangle normal
    glm::vec3 edge1 = triB - triA;
    glm::vec3 edge2 = triC - triA;
    glm::vec3 triNormal = glm::normalize(glm::cross(edge1, edge2));

    // Calculate the denominator for the intersection test
    glm::vec3 rayDirection = p1 - p0;
    float denom = glm::dot(triNormal, rayDirection);

    // If the denominator is close to 0, the line is parallel to the triangle plane
    if (glm::abs(denom) < 1e-6) return false;

    // Calculate the parameter t for the intersection point
    float t = glm::dot(triNormal, (triA - p0)) / denom;

    // If t is not within the segment range, there's no intersection
    if (t < 0.0f || t > 1.0f) return false;

    // Calculate the intersection point
    intersectionPoint = p0 + t * rayDirection;

    // Check if the intersection point is inside the triangle using the barycentric technique
    glm::vec3 edge0 = triB - triA;
    glm::vec3 vp0 = intersectionPoint - triA;
    float d00 = glm::dot(edge0, edge0);
    float d01 = glm::dot(edge0, edge2);
    float d11 = glm::dot(edge2, edge2);
    float d20 = glm::dot(vp0, edge0);
    float d21 = glm::dot(vp0, edge2);
    float denom2 = d00 * d11 - d01 * d01;

    float v = (d11 * d20 - d01 * d21) / denom2;
    float w = (d00 * d21 - d01 * d20) / denom2;
    float u = 1.0f - v - w;

    return (u >= 0) && (v >= 0) && (w >= 0); // Intersection is inside the triangle
}


// Helper: compute cross product axes for SAT
std::array<glm::vec3, 3> GetTriangleEdges(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
    return { b - a, c - b, a - c };
}

// Project triangle onto an axis
void ProjectTriangle(const glm::vec3& axis, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, float& min, float& max) {
    float p0 = glm::dot(axis, a);
    float p1 = glm::dot(axis, b);
    float p2 = glm::dot(axis, c);
    min = glm::min(p0, glm::min(p1, p2));
    max = glm::max(p0, glm::max(p1, p2));
}

// SAT test for triangle-triangle intersection
bool CheckTriangleTriangleCollision(const glm::vec3& A0, const glm::vec3& A1, const glm::vec3& A2, const glm::vec3& B0, const glm::vec3& B1, const glm::vec3& B2) {
    auto edgesA = GetTriangleEdges(A0, A1, A2);
    auto edgesB = GetTriangleEdges(B0, B1, B2);

    // Normals of each triangle
    glm::vec3 normalA = glm::normalize(glm::cross(edgesA[0], edgesA[1]));
    glm::vec3 normalB = glm::normalize(glm::cross(edgesB[0], edgesB[1]));

    std::array<glm::vec3, 13> axes = {
        normalA, normalB,
        glm::cross(edgesA[0], edgesB[0]),
        glm::cross(edgesA[0], edgesB[1]),
        glm::cross(edgesA[0], edgesB[2]),
        glm::cross(edgesA[1], edgesB[0]),
        glm::cross(edgesA[1], edgesB[1]),
        glm::cross(edgesA[1], edgesB[2]),
        glm::cross(edgesA[2], edgesB[0]),
        glm::cross(edgesA[2], edgesB[1]),
        glm::cross(edgesA[2], edgesB[2]),
        edgesA[0], edgesA[1], // optional for robustness
    };

    for (auto& axis : axes) {
        if (glm::length2(axis) < 1e-6f) continue; // skip near-zero axes

        axis = glm::normalize(axis);

        float minA, maxA, minB, maxB;
        ProjectTriangle(axis, A0, A1, A2, minA, maxA);
        ProjectTriangle(axis, B0, B1, B2, minB, maxB);

        // Check for separation
        if (maxA < minB || maxB < minA) return false; // found a separating axis
    }

    // No separating axis found => triangles intersect
    return true;
}

// Function to check collision between two rectangles
bool CheckCollisionRec(const Rect& rec1, const Rect& rec2, glm::vec2* dist) {
    // Calculate the actual boundaries of rec1
    float rec1Left = rec1.position.x;
    float rec1Right = rec1.position.x + rec1.size.x;
    float rec1Top = rec1.position.y;
    float rec1Bottom = rec1.position.y + rec1.size.y;

    // Calculate the actual boundaries of rec2
    float rec2Left = rec2.position.x;
    float rec2Right = rec2.position.x + rec2.size.x;
    float rec2Top = rec2.position.y;
    float rec2Bottom = rec2.position.y + rec2.size.y;

    // Calculate the centers of the rectangles
    glm::vec2 center1((rec1Left + rec1Right) / 2.0f, (rec1Top + rec1Bottom) / 2.0f);
    glm::vec2 center2((rec2Left + rec2Right) / 2.0f, (rec2Top + rec2Bottom) / 2.0f);

    // Calculate distance between the centers
    if (dist != nullptr) {
        *dist = center1 - center2;
    }

    // Check if there is overlap
    if (rec1Left >= rec2Right || rec1Right <= rec2Left ||
        rec1Bottom <= rec2Top || rec1Top >= rec2Bottom) {
        return false;
    }

    return true;
}

bool CheckPointInRectangle(glm::vec2 point, Rect rect) {
    return (point.x >= rect.position.x && point.x <= rect.position.x + rect.size.x &&
        point.y >= rect.position.y && point.y <= rect.position.y + rect.size.y);
}

bool CheckPointInCircle(glm::vec2 point, glm::vec2 origin, float radius) {
    float dx = point.x - origin.x;
    float dy = point.y - origin.y;
    return (dx * dx + dy * dy) <= (radius * radius);
}

// Check if a key has been pressed once
bool IsKeyPressed(int key) {
    CheckActiveWindowSet();
    bool isPressed = glfwGetKey(extras::ActiveWindow, key) == GLFW_PRESS;
    if (isPressed && extras::keyState[key] == false) {
        extras::keyState[key] = true;
        return true;
    }
    if (glfwGetKey(extras::ActiveWindow, key) == GLFW_RELEASE) {
        extras::keyState[key] = false;
    }
    return false;
}

// Check if a key has been pressed again (Only PLATFORM_DESKTOP)
bool IsKeyPressedRepeat(int key) {
    CheckActiveWindowSet();
    // GLFW handles key repeats automatically if enabled
    if (glfwGetKey(extras::ActiveWindow, key) == GLFW_PRESS) {
        return true; // This will fire every frame the key is down; adjust if needed for repeat behavior
    }
    return false;
}

// Check if a key is being pressed
bool IsKeyDown(int key) {
    CheckActiveWindowSet();
    return glfwGetKey(extras::ActiveWindow, key) == GLFW_PRESS;
}

// Check if a key has been released once
bool IsKeyReleased(int key) {
    CheckActiveWindowSet();
    bool isReleased = glfwGetKey(extras::ActiveWindow, key) == GLFW_RELEASE;
    if (isReleased && extras::keyState[key] == true) {
        extras::keyState[key] = false;
        return true;
    }
    if (glfwGetKey(extras::ActiveWindow, key) == GLFW_PRESS) {
        extras::keyState[key] = true;
    }
    return false;
}

// Check if a key is NOT being pressed
bool IsKeyUp(int key) {
    CheckActiveWindowSet();
    return glfwGetKey(extras::ActiveWindow, key) == GLFW_RELEASE;
}





// Check if a gamepad is available
bool IsGamepadAvailable(int gamepad) {
    return glfwJoystickPresent(gamepad) && glfwJoystickIsGamepad(gamepad);
}

// Get gamepad internal name id
const char* GetGamepadName(int gamepad) {
    return glfwGetGamepadName(gamepad);
}

// Check if a gamepad button has been pressed once
bool IsGamepadButtonPressed(int gamepad, int button) {
    GLFWgamepadstate state;
    if (glfwGetGamepadState(gamepad, &state)) {
        if (state.buttons[button] == GLFW_PRESS && extras::buttonState[gamepad][button] == false) {
            extras::buttonState[gamepad][button] = true;
            return true;
        }
        if (state.buttons[button] == GLFW_RELEASE) {
            extras::buttonState[gamepad][button] = false;
        }
    }
    return false;
}

// Check if a gamepad button is being pressed
bool IsGamepadButtonDown(int gamepad, int button) {
    GLFWgamepadstate state;
    return glfwGetGamepadState(gamepad, &state) && state.buttons[button] == GLFW_PRESS;
}

// Check if a gamepad button has been released once
bool IsGamepadButtonReleased(int gamepad, int button) {
    GLFWgamepadstate state;
    if (glfwGetGamepadState(gamepad, &state)) {
        if (state.buttons[button] == GLFW_RELEASE && extras::buttonState[gamepad][button] == true) {
            extras::buttonState[gamepad][button] = false;
            return true;
        }
        if (state.buttons[button] == GLFW_PRESS) {
            extras::buttonState[gamepad][button] = true;
        }
    }
    return false;
}

// Check if a gamepad button is not being pressed
bool IsGamepadButtonUp(int gamepad, int button) {
    GLFWgamepadstate state;
    return glfwGetGamepadState(gamepad, &state) && state.buttons[button] == GLFW_RELEASE;
}

// Get the last gamepad button pressed (not provided by GLFW directly)
int GetGamepadButtonPressed(void) {
    // This function would need custom handling, as GLFW does not support this directly.
    return -1; // Placeholder, no implementation.
}

// Get gamepad axis count for a gamepad
int GetGamepadAxisCount(int gamepad) {
    // GLFW does not provide direct access to axis count; hardcoding to standard count
    return GLFW_GAMEPAD_AXIS_LAST + 1;
}

// Get axis movement value for a gamepad axis with deadzone
float GetGamepadAxisMovement(int gamepad, int axis, float deadzone) {
    GLFWgamepadstate state;
    if (glfwGetGamepadState(gamepad, &state)) {
        float value = state.axes[axis];
        // Apply deadzone
        if (fabs(value) < deadzone) return 0.0f;

        // Re-map the remaining range to 0-1 smoothly
        if (value > 0.0f)
            value = (value - deadzone) / (1.0f - deadzone);
        else
            value = (value + deadzone) / (1.0f - deadzone);

        // Round to 4 decimal places (optional)
        return round(value * 10000.0f) / 10000.0f;
    }
    return 0.0f; // Return zero if no input is detected
}

// Set internal gamepad mappings (uses SDL_GameControllerDB format)
int SetGamepadMappings(const char* mappings) {
    return glfwUpdateGamepadMappings(mappings);
}





// Check if a mouse button is being pressed
bool IsMouseButtonDown(int button) {
    CheckActiveWindowSet();
    if (glfwGetMouseButton(extras::ActiveWindow, button) == GLFW_PRESS) {
        extras::mouseState[button] = true;
        return true;
    }
    else {
        extras::mouseState[button] = false;
        return false;
    }
}

// Check if a mouse button is NOT being pressed
bool IsMouseButtonUp(int button) {
    return !IsMouseButtonDown(button);
}

// Check if a mouse button has been pressed once
bool IsMouseButtonPressed(int button) {
    //CheckActiveWindowSet();
    if (IsMouseButtonDown(button) && extras::lastMouseState[button] == false) {
        return true;
    }
    return false;
}

// Check if a mouse button has been released once
bool IsMouseButtonReleased(int button) {
    //CheckActiveWindowSet();
    if (IsMouseButtonUp(button) && extras::lastMouseState[button] == true) {
        return true;
    }
    return false;
}

void UpdateMouseButtons() {
    extras::lastMouseState = extras::mouseState;
}

// Get mouse position XY in range 0.0-1.0 (can go above and below 0 and above 1 because of mouse out of window boundary
glm::vec2 GetMousePosition() {
    CheckActiveWindowSet();
    double x, y;
    glfwGetCursorPos(extras::ActiveWindow, &x, &y);

    x = x / double(extras::CurrentWindowWidth);
    y = y / double(extras::CurrentWindowHeight);


    if (!extras::drawYDown)
        y = -y + 1.0;
    return glm::vec2(x, y);
}

// Returns normalized window coordinates in NDC range [-1.0, 1.0]
glm::vec2 GetMousePositionNDC() {
    glm::vec2 pos = GetMousePosition();
    glm::vec2 ndc = (pos * 2.0f) - 1.0f;
    if (extras::drawYDown) ndc.y = -ndc.y;
    return ndc;
}

// Convert Mouse position XY range to 0.0-CurrentViewportSize
glm::vec2 GetMouseToScreen(glm::vec2 mousePos, Camera2D* camera) {
    if (camera == nullptr) {
        if (extras::activeCamera2D == nullptr || extras::ActiveWindow == nullptr) {
            std::cerr << "GetScreenToWorld2D() Resources for drawing are not available." << std::endl;
            exit(EXIT_FAILURE);
        }
        camera = extras::activeCamera2D;
    }

    return mousePos * camera->currentViewportSize;
    //glm::vec2(extras::CurrentWindowWidth, extras::CurrentWindowHeight);
}

// Get the ratio of viewport size to window size
glm::vec2 GetViewportScale(Camera2D* camera) {
    if (camera == nullptr) {
        if (extras::activeCamera2D == nullptr || extras::ActiveWindow == nullptr) {
            std::cerr << "GetScreenToWorld2D() Resources for drawing are not available." << std::endl;
            exit(EXIT_FAILURE);
        }
        camera = extras::activeCamera2D;
    }

    glm::vec2 viewportScale(
        camera->currentViewportSize.x / extras::CurrentWindowWidth,
        camera->currentViewportSize.y / extras::CurrentWindowHeight
    );

    return viewportScale;
}

// Get the world space position for a 2d camera screen space position
glm::vec2 GetScreenToWorld2D(glm::vec2 screenPos, Camera2D* camera) {
    if (camera == nullptr) {
        if (extras::activeCamera2D == nullptr || extras::ActiveWindow == nullptr) {
            std::cerr << "GetScreenToWorld2D() Resources for drawing are not available." << std::endl;
            exit(EXIT_FAILURE);
        }
        camera = extras::activeCamera2D;
    }

    glm::mat4 invView = camera->GetInverseViewMatrix();

    // Scale screen position to match the viewport size
    glm::vec2 viewportScale = GetViewportScale(camera);
    glm::vec2 scaledScreenPos = glm::vec2(
        screenPos.x * viewportScale.x,
        screenPos.y * viewportScale.y
    );

    // Mouse positions use a top-left origin. Convert to y-up only for the legacy projection.
    if (!extras::drawYDown)
        scaledScreenPos.y = camera->currentViewportSize.y - scaledScreenPos.y;

    // Transform scaled screen position to world space
    glm::vec4 screenPosHomogeneous(scaledScreenPos.x, scaledScreenPos.y, 0.0f, 1.0f);
    glm::vec4 worldPosHomogeneous = invView * screenPosHomogeneous;

    // Return world position
    return glm::vec2(worldPosHomogeneous.x, worldPosHomogeneous.y);
}

// Get the screen space position for a 2d world space position    
glm::vec2 GetWorldToScreen2D(glm::vec2 position, Camera2D* camera) {
    if (camera == nullptr) {
        if (extras::activeCamera2D == nullptr || extras::ActiveWindow == nullptr) {
            std::cerr << "GetScreenToWorld2D() Resources for drawing are not available." << std::endl;
            exit(EXIT_FAILURE);
        }
        camera = extras::activeCamera2D;
    }

    // Combine view and projection matrices
    glm::mat4 viewProj = camera->GetProjectionMatrix() * camera->GetViewMatrix();

    // Convert world position to homogeneous coordinates
    glm::vec4 world = glm::vec4(position, 0.0f, 1.0f);

    // Transform to clip space and normalize to screen space
    glm::vec4 clip = viewProj * world;
    clip /= clip.w; // Perspective divide (not strictly needed for 2D)

    // Scale to viewport dimensions
    glm::vec2 screenSpacePos = glm::vec2(
        ((clip.x + 1.0f) * 0.5f) * camera->currentViewportSize.x,
        ((1.0f - clip.y) * 0.5f) * camera->currentViewportSize.y
    );

    return screenSpacePos;
}

// Get the world space (X, Y) position on the Z = 0 plane from a 2D screen coordinate
glm::vec2 GetScreenToWorld3D(glm::vec2 screenPos, Camera3D* camera) {
    if (camera == nullptr) {
        if (extras::activeCamera3D == nullptr || extras::ActiveWindow == nullptr) {
            std::cerr << "GetScreenToWorld3D() Resources for drawing are not available." << std::endl;
            exit(EXIT_FAILURE);
        }
        camera = extras::activeCamera3D;
    }

    // 1. Convert pixel/screen coordinates to NDC [-1, 1] if not already converted
    glm::vec2 ndc = screenPos;
    if (screenPos.x > 1.0f || screenPos.x < -1.0f || screenPos.y > 1.0f || screenPos.y < -1.0f) {
        ndc.x = (2.0f * screenPos.x / static_cast<float>(extras::CurrentWindowWidth)) - 1.0f;
        ndc.y = 1.0f - (2.0f * screenPos.y / static_cast<float>(extras::CurrentWindowHeight));
    }

    glm::mat4 view = camera->GetViewMatrix();
    glm::mat4 proj = camera->GetProjectionMatrix();
    glm::mat4 viewProj = proj * view;

    // Guard against singular view-projection matrix
    if (std::abs(glm::determinant(viewProj)) < 1e-7f) {
        return glm::vec2(camera->position.x, camera->position.y);
    }

    glm::mat4 invViewProj = glm::inverse(viewProj);

    // 2. Sample NDC Near (-1.0) and a safe Mid depth (0.0) instead of Far (1.0)
    // Sampling at 0.0 stays safely away from the infinite/far clipping plane boundary
    glm::vec4 ndcNear = glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f);
    glm::vec4 ndcMid = glm::vec4(ndc.x, ndc.y, 0.0f, 1.0f);

    glm::vec4 worldNear = invViewProj * ndcNear;
    glm::vec4 worldMid = invViewProj * ndcMid;

    // Guard against perspective division by zero
    if (std::abs(worldNear.w) < 1e-6f || std::abs(worldMid.w) < 1e-6f) {
        return glm::vec2(camera->position.x, camera->position.y);
    }

    worldNear /= worldNear.w;
    worldMid /= worldMid.w;

    glm::vec3 rayStart = glm::vec3(worldNear);
    glm::vec3 rayDir = glm::normalize(glm::vec3(worldMid - worldNear));

    // Intersect ray with Z = 0 ground plane
    if (std::abs(rayDir.z) < 1e-6f) {
        return glm::vec2(rayStart.x, rayStart.y);
    }

    float t = -rayStart.z / rayDir.z;
    glm::vec3 worldIntersect = rayStart + rayDir * t;

    return glm::vec2(worldIntersect.x, worldIntersect.y);
}

// Get the screen space position for a 3d world space position
glm::vec2 GetWorldToScreen3D(glm::vec3 worldPos, Camera3D* camera) {
    if (camera == nullptr) {
        if (extras::activeCamera3D == nullptr || extras::ActiveWindow == nullptr) {
            std::cerr << "GetWorldToScreen3D() Resources for drawing are not available." << std::endl;
            exit(EXIT_FAILURE);
        }
        camera = extras::activeCamera3D;
    }

    // Combine view and projection matrices
    glm::mat4 viewProj = camera->GetProjectionMatrix() * camera->GetViewMatrix();

    // Transform world position to clip space
    glm::vec4 clip = viewProj * glm::vec4(worldPos, 1.0f);

    // Perspective divide to get Normalized Device Coordinates [-1, 1]
    if (clip.w != 0.0f) {
        clip /= clip.w;
    }

    // Return NDC screen coordinates (-1 to 1)
    return glm::vec2(clip.x, clip.y);
}

// Get mouse delta between frames in range 0.0-1.0
glm::vec2 GetMouseDelta() {
    return extras::CurrentMousePosition - extras::lastMousePosition;
}

// Get mouse wheel movement for X or Y, whichever is larger
float GetMouseWheelMove() {
    CheckActiveWindowSet();
    float move;
    if (glm::abs(extras::mouseWheelOffset.x) > glm::abs(extras::mouseWheelOffset.y)) {
        move = extras::mouseWheelOffset.x;
    }
    else {
        move = extras::mouseWheelOffset.y;
    }
    return move;
}

// Get mouse wheel movement for both X and Y
glm::vec2 GetMouseWheelMoveV() {
    CheckActiveWindowSet();
    glm::vec2 move = extras::mouseWheelOffset;
    return move;
}


//// GL and CL shared context setup (platform-specific, assuming Windows here for example)
//cl::Context createSharedContext(cl::Device& device) {
//    cl_context_properties props[] = {
//        CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
//        CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
//        CL_CONTEXT_PLATFORM, (cl_context_properties)(cl::Platform::getDefault())(),
//        0
//    };
//    return cl::Context(device, props);
//}

// Initialize OpenGL texture and share with OpenCL
//GLuint createSharedTexture(int width, int height) {
//    if (globalTextureID == 0) {
//        glGenTextures(1, &globalTextureID);
//    }
//    glBindTexture(GL_TEXTURE_2D, globalTextureID);
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//    glBindTexture(GL_TEXTURE_2D, 0);
//    return globalTextureID;
//}

cl::Kernel setupKernel(const cl::Context& context, const std::string& source, const std::string& sourceFunctionName) {
    cl::Program::Sources sources;
    sources.push_back({ source.c_str(), source.length() });

    cl::Program program(context, sources);
    if (program.build() != CL_SUCCESS) {
        std::cerr << "OpenCL Kernel build log: " << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(cl::Device::getDefault()) << std::endl;
        exit(EXIT_FAILURE);
    }

    return cl::Kernel(program, sourceFunctionName.c_str());
}


// Shader sources
const char* DefaultVertexShaderSource = R"glsl(
    #version 330 core

    layout (location = 0) in vec3 aPos;    // Position attribute
    layout (location = 1) in vec3 aNormal; // Normal attribute
    layout (location = 2) in vec2 aTexCoords; // Texture coordinate attribute
    layout (location = 3) in vec3 aColor; // Color attribute

    uniform mat4 model;       // Model transformation matrix
    uniform mat4 view;        // View transformation matrix
    uniform mat4 projection;  // Projection matrix

    out vec3 FragPos;         // Position to pass to fragment shader
    out vec3 Normal;          // Normal to pass to fragment shader
    out vec2 TexCoords;       // Pass texture coordinates to the fragment shader
    out vec3 Color;           // Pass color to fragment shader

    void main() {
        gl_Position = projection * view * model * vec4(aPos, 1.0);
        TexCoords = aTexCoords;
    
        // Transform normals to world space
        Normal = mat3(transpose(inverse(model))) * aNormal;
        // Transform vertex position to world space
        FragPos = vec3(model * vec4(aPos, 1.0));
        Color = aColor;
    }
)glsl";

const char* Default2DFragmentShaderSource = R"glsl(
    #version 330 core
    in vec2 TexCoords;  // Texture coordinates from the vertex shader

    uniform sampler2D textureSampler;  // Texture sampler for the texture unit
    uniform vec4 color;  // Uniform color if texture is not used
    uniform bool useTexture;  // Flag to indicate whether to use texture
    uniform bool mixColor;

    out vec4 FragColor;

    void main() {
        if (useTexture) {
            vec4 texColor = texture(textureSampler, TexCoords); // Sample the texture

            if (mixColor) {
                //FragColor.rgb = mix(color.rgb, texColor.rgb, 0.5);
                //FragColor.a = texColor.a;
                FragColor = color * texture(textureSampler, TexCoords);
            }
            else {
                FragColor = texColor;//color * texture(textureSampler, TexCoords);
            }
        } else {
            FragColor = color;
        }
    }
)glsl";

const char* Default3DFragmentShaderSource = R"glsl(
    #version 330 core

    in vec2 TexCoords;
    in vec3 Normal;
    in vec3 FragPos;

    uniform sampler2D textureSampler;
    uniform vec4 color;
    uniform bool useTexture;
    uniform vec3 viewPos;

    out vec4 FragColor;

    void main() {
        vec4 albedo = color;
        if (useTexture) {
            albedo *= texture(textureSampler, TexCoords);
        }

        float normalLength = length(Normal);
        vec3 normal = normalLength > 0.0001 ? Normal / normalLength : vec3(0.0, 1.0, 0.0);
        vec3 keyDir = normalize(vec3(-0.45, 0.82, 0.35));
        vec3 fillDir = normalize(vec3(0.42, 0.48, -0.77));
        float keyLight = max(dot(normal, keyDir), 0.0);
        float fillLight = max(dot(normal, fillDir), 0.0);

        // Keep shadow-facing surfaces readable while giving upward and sun-facing
        // surfaces enough contrast to reveal the shape of the geometry.
        float illumination = 0.32 + keyLight * 0.56 + fillLight * 0.14;

        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 halfDir = normalize(keyDir + viewDir);
        float specular = pow(max(dot(normal, halfDir), 0.0), 36.0) * 0.08;

        vec3 litColor = albedo.rgb * illumination + vec3(specular);
        FragColor = vec4(clamp(litColor, 0.0, 1.0), albedo.a);
    }
)glsl";


const char* DefaultSkyboxVertexShaderSource = R"glsl(
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 2) in vec2 aTexCoords;

out vec2 TexCoords;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    // Remove translation so skybox stays centered
    mat4 viewNoTranslation = mat4(mat3(view));

    TexCoords = aTexCoords;

    vec4 pos = projection * viewNoTranslation * vec4(aPos, 1.0);

    // Force depth to 0.0 (for Reverse-Z)
    gl_Position = vec4(pos.xy, 0.0, pos.w);
}
)glsl";

const char* DefaultSkyboxFragmentShaderSource = R"glsl(
    #version 330 core

    out vec4 FragColor;

    in vec2 TexCoords;

    uniform sampler2D skybox;

    void main()
    {
        FragColor = texture(skybox, TexCoords);
    }
)glsl";

const char* DefaultFontVertexShaderSource = R"glsl(
    #version 330 core

    layout (location = 0) in vec3 position;
    layout (location = 1) in vec2 texCoord;
    layout (location = 2) in float charProgress; // 0.0 at start of string, 1.0 at end

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    out vec2 TexCoords;
    out float vProgress;

    void main()
    {
        gl_Position = projection * view * model * vec4(position, 1.0);
        TexCoords = texCoord;
        vProgress = charProgress;
    }
)glsl";


// Hinted bitmap glyphs: the atlas alpha is the glyph's coverage.
const char* BitmapFontFragmentShaderSource = R"glsl(
#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D atlas;
uniform vec4 textColor;

void main() {
    float coverage = texture(atlas, TexCoords).a;
    if (coverage <= 0.0) discard;
    FragColor = vec4(textColor.rgb, textColor.a * coverage);
}
)glsl";


const char* DefaultFontFragmentShaderSource = R"glsl(
#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D MTSDF;
uniform float emRangeLow;
uniform float emRangeHigh;
uniform float pxRange; // Full signed-distance range in atlas pixels
uniform float strokeWidth = 0.0; // Extra glyph coverage in screen pixels
uniform float fontWeightOffset = 0.0; // Signed-distance field offset as a fraction of its encoded range
uniform vec4 textColor;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

float screenPxRange() {
    vec2 unitRange = vec2(pxRange) / vec2(textureSize(MTSDF, 0));
    vec2 dx = dFdx(TexCoords);
    vec2 dy = dFdy(TexCoords);
    vec2 texelFootprint = dx * dx + dy * dy;
    vec2 screenTexSize = inversesqrt(max(texelFootprint, vec2(1e-12)));
    return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

void main() {
    // Reconstruct the signed distance from the linear RGB MSDF channels.
    vec3 msd = texture(MTSDF, TexCoords).rgb;
    float sigDist = median(msd.r, msd.g, msd.b);

    // The zero-distance edge is not always at 0.5 for an asymmetric range.
    float edgeValue = -emRangeLow / max(emRangeHigh - emRangeLow, 0.000001);

    // Convert the encoded distance to screen pixels using the atlas range.
    // The one-pixel floor keeps the coverage ramp defined for very small text.
    float screenSpaceDist = (sigDist - edgeValue) * screenPxRange();
    float alpha = clamp(screenSpaceDist + 0.5 + strokeWidth + fontWeightOffset * screenPxRange(), 0.0, 1.0);

    // 6. Final Output
    vec4 finalColor = vec4(textColor.rgb, textColor.a * alpha);
    
    if (finalColor.a < 0.001) discard;

    FragColor = finalColor;
}
)glsl";

//const char* DefaultFontFragmentShaderSource = R"glsl(
//#version 330 core
//
//in vec2 TexCoords;
//in float vProgress;
//
//out vec4 FragColor;
//
//uniform sampler2D msdf;
//
//uniform float emRangeLow;
//uniform float emRangeHigh;
//uniform float weight;
//uniform float time; // optional animation
//
//float median(float r, float g, float b) {
//    return max(min(r, g), min(max(r, g), b));
//}
//
//// HSV -> RGB (clean rainbow)
//vec3 hsv2rgb(vec3 c) {
//    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
//    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
//    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
//}
//
//void main() {
//    float totalRange = emRangeHigh - emRangeLow;
//    float edgeThreshold = abs(emRangeLow) / totalRange;
//
//    vec2 dx = dFdx(TexCoords);
//    vec2 dy = dFdy(TexCoords);
//
//    vec2 offsets[4];
//    offsets[0] = vec2(-0.125, -0.375);
//    offsets[1] = vec2( 0.375, -0.125);
//    offsets[2] = vec2(-0.375,  0.125);
//    offsets[3] = vec2( 0.125,  0.375);
//
//    vec4 accumulated = vec4(0.0);
//
//    for (int i = 0; i < 4; i++) {
//        vec2 subUV = TexCoords + offsets[i].x * dx + offsets[i].y * dy;
//
//        vec3 msdfSample = texture(msdf, subUV).rgb;
//        float sd = median(msdfSample.r, msdfSample.g, msdfSample.b) * weight;
//
//        // Smooth edge
//        float alpha = smoothstep(edgeThreshold - 0.02, edgeThreshold + 0.02, sd);
//
//        // Rainbow using progress
//        float hue = fract(time * 0.2 - vProgress);
//        vec3 color = hsv2rgb(vec3(hue, 1.0, 1.0));
//
//        accumulated += vec4(color, alpha);
//    }
//
//    FragColor = accumulated * 0.25;
//
//    if (FragColor.a < 0.01)
//        discard;
//}
//)glsl";

//const char* DefaultFontFragmentShaderSource = R"glsl(
//#version 330 core
//
//in vec2 TexCoords;
//out vec4 FragColor;
//
//uniform sampler2D msdf;
//uniform sampler2D colorGradient; 
//
//uniform float emRangeLow;  // -0.4
//uniform float emRangeHigh; // 0.1
//uniform float weight;      // 1.0 is default, > 1 is bolder, < 1 is thinner
//
//float median(float r, float g, float b) {
//    return max(min(r, g), min(max(r, g), b));
//}
//
//// Helper to map SD to Gradient T
//float getT(float sd, float threshold) {
//    if (sd >= threshold) {
//        return mix(0.5, 0.0, clamp((sd - threshold) / (1.0 - threshold), 0.0, 1.0));
//    } else {
//        return mix(1.0, 0.5, clamp(sd / threshold, 0.0, 1.0));
//    }
//}
//
//void main() {
//    // 1. Calculate global constants once
//    float totalRange = emRangeHigh - emRangeLow;
//    float edgeThreshold = abs(emRangeLow) / totalRange;
//    float gradWidth = float(textureSize(colorGradient, 0).x);
//
//    // 2. Determine the pixel "footprint" in UV space
//    vec2 dx = dFdx(TexCoords);
//    vec2 dy = dFdy(TexCoords);
//
//    // 3. Define 4 sub-pixel offsets (Rotated Grid)
//    vec2 offsets[4];
//    offsets[0] = vec2(-0.125, -0.375);
//    offsets[1] = vec2( 0.375, -0.125);
//    offsets[2] = vec2(-0.375,  0.125);
//    offsets[3] = vec2( 0.125,  0.375);
//
//    vec4 accumulatedColor = vec4(0.0);
//
//    // 4. Sample the MSDF and Gradient 4 times
//    for (int i = 0; i < 4; i++) {
//        // Offset the UV for sub-pixel accuracy
//        vec2 subPixelUV = TexCoords + (offsets[i].x * dx) + (offsets[i].y * dy);
//        
//        // Sample MSDF at this specific sub-pixel position
//        vec3 msdfSample = texture(msdf, subPixelUV).rgb;
//        float sd = median(msdfSample.r, msdfSample.g, msdfSample.b) * weight;
//        
//        // Calculate gradient coordinate (t)
//        float t = getT(sd, edgeThreshold);
//        
//        // Fetch color from the gradient ramp
//        float texCoordX = (t * (gradWidth - 1.0) + 0.5) / gradWidth;
//        accumulatedColor += texture(colorGradient, vec2(texCoordX, 0.5));
//    }
//
//    // 5. Average the results
//    FragColor = accumulatedColor * 0.25;
//
//    if (FragColor.a < 0.001) discard;
//}
//)glsl";


const char* Default2DLineVertexShaderSource = R"glsl(
    #version 330 core

    layout(location = 1) in vec3 aPos;        // Line position
    layout(location = 2) in vec2 aOffsetDir; // Perpendicular offset direction
    layout(location = 3) in float aAbsoluteDistance; // Absolute distance along the polyline

    uniform mat4 view;
    uniform mat4 projection;
    uniform float thickness; // Line thickness

    out float absoluteDistance; // Absolute distance along the polyline

    void main() {
        // Expand line vertex based on thickness and offset
        vec3 expandedPos = aPos + vec3(aOffsetDir * thickness, 0.0);
        gl_Position = projection * view * vec4(expandedPos, 1.0);

        // Pass absolute distance to the fragment shader
        absoluteDistance = aAbsoluteDistance;
    }
)glsl";

const char* Default2DLineFragmentShaderSource = R"glsl(
    #version 330 core

    in float absoluteDistance; // Absolute distance along the polyline
    uniform vec4 lineColor;    // Line color
    uniform float dashLength;  // Length of dashes
    uniform float gapLength;   // Length of gaps
    out vec4 fragColor;

    void main() {
        // Total cycle length (dash + gap)
        float cycleLength = dashLength + gapLength;

        // Calculate position within the current dash-gap cycle
        float progress = mod(absoluteDistance, cycleLength);

        // Render the fragment if it falls within the dash length
        if (progress < dashLength) {
            fragColor = lineColor; // Render dash
        } else {
            discard; // Skip fragment for gap
        }
    }
)glsl";

// Blur Shader Source
const char* Default2DBlurFragmentShaderSource = R"glsl(
    #version 330 core
    in vec2 TexCoords;
    uniform sampler2D image;
    uniform bool horizontal;
    uniform float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    out vec4 FragColor;

    void main() {             
        vec2 tex_offset = 1.0 / textureSize(image, 0); 
        vec3 result = texture(image, TexCoords).rgb * weight[0];
        if(horizontal) {
            for(int i = 1; i < 5; ++i) {
                result += texture(image, TexCoords + vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
                result += texture(image, TexCoords - vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
            }
        } else {
            for(int i = 1; i < 5; ++i) {
                result += texture(image, TexCoords + vec2(0.0, tex_offset.y * i)).rgb * weight[i];
                result += texture(image, TexCoords - vec2(0.0, tex_offset.y * i)).rgb * weight[i];
            }
        }
        FragColor = vec4(result, 1.0);
    }
)glsl";

const char* Default2DGeometryGlassShaderSource = R"glsl(
    #version 330 core
    in vec2 TexCoords; 
    out vec4 FragColor;

    uniform sampler2D blurredTexture; 
    uniform vec2 iResolution;
    uniform vec2 shapeCenter;  
    uniform float shapeRadius; 
    uniform bool isRect;
    uniform vec2 rectSize;
    uniform float cornerRadius; 

    uniform vec4 color;          // The base color of the glass (rgb)
    uniform float tintStrength;  // Global density: 0.0 (clear) to 1.0 (opaque tint)

    float sdRoundedBox(vec2 p, vec2 b, float r) {
        vec2 q = abs(p) - b + r;
        return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
    }

    void main() {
        vec2 screenUV = gl_FragCoord.xy / iResolution;
        vec3 bg = texture(blurredTexture, screenUV).rgb;
    
        // 1. COORDINATE & DISTANCE LOGIC
        vec2 localDir = isRect ? (TexCoords - 0.5) * 2.0 : TexCoords;
        float currentDist = isRect ? sdRoundedBox(gl_FragCoord.xy - shapeCenter, rectSize * 0.5, cornerRadius) : length(gl_FragCoord.xy - shapeCenter);
        float currentMax = isRect ? 0.0 : shapeRadius;

        // 2. MASKING (Preserved 1px Glimmer)
        float aa = 1.5; 
        float maskMain = 1.0 - smoothstep(currentMax - aa, currentMax, currentDist);
        float maskInner = 1.0 - smoothstep(currentMax - 1.0 - aa, currentMax - 1.0, currentDist);
        float edgeGlimmer = clamp(maskMain - maskInner, 0.0, 1.0);

        // 3. UPDATED TINTING LOGIC
        // We use tintStrength to blend between the raw background and the colored background.
        // This creates that "thick glass" look consistently.
        vec3 tintedBg = mix(bg, bg * color.rgb, tintStrength);

        // 4. DIRECTIONAL GLIMMER
        vec2 dir = normalize(localDir);
        vec2 lightDir = normalize(vec2(-0.4, 0.7)); 
        float alignment = dot(dir, lightDir);
    
        // Glimmer highlight remains pure (calculated from raw bg) to feel like surface reflection
        vec3 colGlimmerBright = bg * 2.5 + vec3(0.5); 
        vec3 colGlimmerMid    = tintedBg; 
        vec3 colGlimmerDark   = tintedBg * 0.8;               

        vec3 finalGlimmerCol = (alignment > 0.0) 
            ? mix(colGlimmerMid, colGlimmerBright, pow(alignment, 2.0))
            : mix(colGlimmerMid, colGlimmerDark, pow(-alignment, 1.5));

        // 5. COMPOSITION
        vec3 colCore = tintedBg * 1.1;
        vec3 colDark = tintedBg * 0.6;

        vec3 col = colDark;
        col = mix(col, colCore, maskInner);
        col = mix(col, finalGlimmerCol, edgeGlimmer);
    
        float alpha = maskMain;
        if (alpha <= 0.001) discard;

        FragColor = vec4(col, alpha);
    }
)glsl";

const char* DefaultChartVertexShaderSource = R"glsl(
    #version 430 core

    layout (location = 0) in vec3 aPos;
    layout (location = 2) in vec2 aTexCoords;

    out vec2 fragUV;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main() {
        gl_Position = projection * view * model * vec4(aPos, 1.0);
        fragUV = aTexCoords;
    }
)glsl";

const char* DefaultLineChartFragmentShaderSource = R"glsl(
    #version 430 core
    out vec4 FragColor;

    in vec2 fragUV;

    uniform vec2 uResolution;
    uniform int uEndIndex;
    uniform int uWindowSize;
    uniform float uIndexOffset;
    uniform vec2 uMouseUV;
    uniform float uCurrentPrice;
    uniform bool uPriceUp;
    uniform int uBufferIndex;
    uniform vec2 uPriceRange;

    layout(std430, binding = 1) buffer priceBuffer {
        float prices[];
    };

    void main() {
        float chartWidthPx = uResolution.x;
        float pointPx = max(1.0, floor(chartWidthPx / float(uWindowSize)));
        float usedPx = pointPx * float(uWindowSize);
        float extraPx = chartWidthPx - usedPx;
        float spacingCorrection = (extraPx / float(uWindowSize)) / chartWidthPx;
        float step = pointPx / chartWidthPx + spacingCorrection;

        // Estimate which index this pixel might belong to
        float x = fragUV.x;
        float distanceFromRight = 1.0 - x;
        float relIndex = (distanceFromRight / step) - 0.5 + uIndexOffset;
        int pixelIndex = int(floor(float(uWindowSize - 1) - relIndex));

        // Cull pixels that are far outside the data range
        if (pixelIndex < -2 || pixelIndex > uWindowSize + 2) discard;

        for (int i = pixelIndex - 2; i <= pixelIndex + 2; ++i) {
            int idx0 = uEndIndex - uWindowSize + i;
            int idx1 = idx0 + 1;

            if (i < 0 || i >= uWindowSize - 1) continue;
            if (idx0 < 0 || idx1 >= prices.length()) continue;

            float price0 = prices[(uBufferIndex + idx0) % prices.length()];
            float price1 = prices[(uBufferIndex + idx1) % prices.length()];

            float y0 = (price0 - uPriceRange.x) / (uPriceRange.y - uPriceRange.x);
            float y1 = (price1 - uPriceRange.x) / (uPriceRange.y - uPriceRange.x);

            float shift0 = float(uWindowSize - 1 - i) - uIndexOffset;
            float shift1 = float(uWindowSize - 1 - (i + 1)) - uIndexOffset;

            float x0 = 1.0 - ((shift0 + 0.5) * step);
            float x1 = 1.0 - ((shift1 + 0.5) * step);

            vec2 p0 = vec2(x0, y0);
            vec2 p1 = vec2(x1, y1);

            vec2 p = fragUV;
            vec2 d = p1 - p0;
            float len2 = dot(d, d);
            float t = clamp(dot(p - p0, d) / len2, 0.0, 1.0);
            vec2 closest = p0 + t * d;
            float dist = distance(p, closest);

            float thickness = 1.5 / uResolution.y;
            float alpha = smoothstep(thickness, 0.0, dist);

            if (alpha > 0.0) {
                vec3 lineColor = vec3(0.2, 0.8, 1.0);
                FragColor = vec4(lineColor, alpha);
                return;
            }
        }

        discard;
    }
)glsl";

const char* DefaultBarChartFragmentShaderSource = R"glsl(
    #version 430 core
    out vec4 FragColor;

    in vec2 fragUV;

    uniform vec2 uResolution;
    uniform int uEndIndex;
    uniform int uWindowSize;
    uniform float uIndexOffset;
    uniform vec2 uMouseUV;
    uniform float uCurrentPrice;
    uniform bool uPriceUp;
    uniform int uBufferIndex;
    uniform float uMaxVolume;

    struct Candle {
        float open;
        float high;
        float low;
        float close;
        float volume;   // or fees or ID etc.
    };

    layout(std430, binding = 0) buffer CandleBuffer {
        Candle candles[];
    };

    uniform vec2 uPriceRange;

    void main() {
        float chartWidthPx = uResolution.x;
        float chartHeightPx = uResolution.y;

        float minBodyHeight = 1.0 / chartHeightPx;
        float wickWidth = 1.0 / chartWidthPx;

        // Skip border fragments
        float minCandleWidth = 1.0 / uResolution.x;
        float borderWidthX = minCandleWidth;
        float borderWidthY = minBodyHeight;

        if (fragUV.x < borderWidthX || fragUV.x > 1.0 - borderWidthX ||
            fragUV.y < borderWidthY || fragUV.y > 1.0 - borderWidthY) {
            FragColor = vec4(1.0, 0.0, 0.0, 1.0); // Red border
            return;
        }

        float rawCandlePx = chartWidthPx / float(uWindowSize);
        float candlePx = max(1.0, floor(rawCandlePx));
        float usedPx = candlePx * float(uWindowSize);
        float extraPx = chartWidthPx - usedPx;
        float spacingCorrection = (extraPx / float(uWindowSize)) / chartWidthPx;
        float candleStep = candlePx / chartWidthPx + spacingCorrection;

        float distanceFromRight = 1.0 - fragUV.x;
        float relIndex = (distanceFromRight / candleStep) - 0.5 + uIndexOffset;

        int i0 = int(floor(float(uWindowSize - 1) - relIndex));
        int i1 = i0 + 1;



        // --- Dashed crosshair for mouse ---
        float dashSize = 4.0;  // Adjust for dash frequency

        // Snap to nearest candle center
        rawCandlePx = uResolution.x / float(uWindowSize);
        candlePx = max(1.0, floor(rawCandlePx));
        usedPx = candlePx * float(uWindowSize);
        extraPx = uResolution.x - usedPx;
        spacingCorrection = (extraPx / float(uWindowSize)) / uResolution.x;
        candleStep = candlePx / uResolution.x + spacingCorrection;

        float distFromRight = uMouseUV.x;
        relIndex = (distFromRight / candleStep) - 0.5 - uIndexOffset;
        float snapIndex = float(uWindowSize - 1) - round(relIndex);
        float snappedCenter = 1.0 - ((snapIndex + 0.5 - uIndexOffset) * candleStep);

        // --- Horizontal dashed line at price level ---
        float pixelHeight = 1.0 / uResolution.y;
        // Snap the line to the center of the nearest pixel
        float yPixelCoord = floor(uMouseUV.y * uResolution.y) + 0.5;
        float snappedY = yPixelCoord / uResolution.y;
        if (abs(fragUV.y - snappedY) < 0.5 * pixelHeight) {
            if (mod(fragUV.x * uResolution.x, dashSize * 2.0) < dashSize) {
                FragColor = vec4(1.0, 1.0, 1.0, 1.0);
                return;
            }
        }

        // --- Vertical dashed line at snapped candle center ---
        float pixelWidth = 1.0 / uResolution.x;
        if (abs(fragUV.x - snappedCenter) < 0.5 * pixelWidth) {
            if (mod(fragUV.y * uResolution.y, dashSize * 2.0) < dashSize) {
                FragColor = vec4(1.0, 1.0, 1.0, 1.0);
                return;
            }
        }




        // --- Dashed crosshair for current price ---
        dashSize = 1.0;  // Adjust for dash frequency

        // --- Horizontal dashed line at price level ---
        yPixelCoord = floor(uCurrentPrice * uResolution.y) + 0.5;
        snappedY = yPixelCoord / uResolution.y;
        if (abs(fragUV.y - snappedY) < 0.5 * pixelHeight) {
            if (mod(fragUV.x * uResolution.x, dashSize * 3.0) < dashSize) {
                FragColor = uPriceUp ? vec4(0.1, 0.8, 0.3, 1.0)
                                     : vec4(0.9, 0.2, 0.2, 1.0);
                return;              
            }
        }



        for (int j = 0; j < 2; ++j) {
            int i = (j == 0) ? i0 : i1;
            int candleIndex = uEndIndex - uWindowSize + i;
            if (i < -1 || i > uWindowSize || candleIndex < 0 || candleIndex >= candles.length()) continue;

            int wrappedIndex = (uBufferIndex + candleIndex) % candles.length();
            if (wrappedIndex < 0 || wrappedIndex >= candles.length()) continue;
            Candle candle = candles[wrappedIndex];

            float open = candle.open;
            float high = candle.high;
            float low = candle.low;
            float close = candle.close;
            float volume = candle.volume;
            bool up = close > open;

            float indexShift = float(uWindowSize - 1 - i) - uIndexOffset;
            float candleCenter = 1.0 - ((indexShift + 0.5) * candleStep);
            float candleWidth = candlePx / chartWidthPx;

            float highY = (high - uPriceRange.x) / (uPriceRange.y - uPriceRange.x);
            float lowY = (low - uPriceRange.x) / (uPriceRange.y - uPriceRange.x);
            float openY = (open - uPriceRange.x) / (uPriceRange.y - uPriceRange.x);
            float closeY = (close - uPriceRange.x) / (uPriceRange.y - uPriceRange.x);

            if (abs(fragUV.x - candleCenter) < wickWidth * 0.5 &&
                fragUV.y >= lowY && fragUV.y <= highY) {
                FragColor = up ? vec4(0.1, 0.8, 0.3, 1.0)
                               : vec4(0.9, 0.1, 0.1, 1.0);
                return;
            }

            float bodyTop = max(openY, closeY);
            float bodyBot = min(openY, closeY);

            if (bodyTop - bodyBot < minBodyHeight) {
                float mid = 0.5 * (bodyTop + bodyBot);
                bodyTop = mid + 0.5 * minBodyHeight;
                bodyBot = mid - 0.5 * minBodyHeight;
            }

            if (abs(fragUV.x - candleCenter) < candleWidth * 0.45 &&
                fragUV.y >= bodyBot && fragUV.y <= bodyTop) {
                FragColor = up ? vec4(0.1, 0.8, 0.3, 1.0)
                               : vec4(0.9, 0.1, 0.1, 1.0);
                return;
            }

            float barRegion = 0.1;  // bottom 10% of screen reserved for volume
            float volumeNorm = volume / uMaxVolume;
            float barTop = barRegion * volumeNorm;

            if (fragUV.y <= barTop &&
                abs(fragUV.x - candleCenter) < candleWidth * 0.49) {
                FragColor = up ? vec4(0.1, 0.8, 0.3, 1.0)
                               : vec4(0.9, 0.1, 0.1, 1.0);
                return;
            }
        }

        discard;
    }
)glsl";

//const char* DefaultFontFragmentShaderSource = R"glsl(
//    #version 330 core
//    in vec2 TexCoords;
//
//    uniform sampler2D text;
//    uniform vec4 textColor;
//    uniform vec2 iResolution;  // Screen resolution
//
//    out vec4 color;
//
//    void main()
//    {    
//        vec2 texSize = textureSize(text, 0); // Get texture size
//        vec2 pixelCoords = (floor(TexCoords * iResolution)) / iResolution;
//        vec2 pix = pixelCoords * texSize;
//        vec2 pix2 = TexCoords * texSize;
//        vec2 nearestTexel1 = floor(pix) + 0.5;                                          // regular point sampling (emulated)   Minimal Preprocessing
//        vec2 nearestTexel2 = floor(pix2) + min(fract(pix2) / fwidth(pix2), 1.0) - 0.5;  // custom antialiased point sampling   Requires Premultiplied Alphas
//
//        vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, pix2 / texSize).r);
//        //vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, nearestTexel1 / texSize).r);
//        color = textColor * sampled;
//    }
//)glsl";

void printFormattedSource(const char* source) {
    std::istringstream iss(source);
    std::string line;
    int lineNumber = 1;
    int maxLines = 0;

    // First pass to count the total number of lines
    while (std::getline(iss, line)) {
        maxLines++;
    }

    // Calculate the number of digits in the largest line number
    int numDigits = static_cast<int>(std::log10(maxLines)) + 1;

    // Reset the stream to read the lines again
    iss.clear();
    iss.seekg(0, std::ios::beg);

    // Second pass to print each line with formatted numbers
    while (std::getline(iss, line)) {
        std::cout << std::setw(numDigits) << std::right << lineNumber << ")";
        std::cout << line; // Print the actual line content
        std::cout << '\n';
        lineNumber++;
    }
}

GLuint loadShaderFile(const char* source, int type) {
    if (source[0] == '\n') {
        ++source; // Skip the newline character
    }

    GLuint shader;
    if (type == Shader::Types::VERT) {
        shader = glCreateShader(GL_VERTEX_SHADER);
    }
    else if (type == Shader::Types::GEO) {
        shader = glCreateShader(GL_GEOMETRY_SHADER);
    }
    else if (type == Shader::Types::FRAG) {
        shader = glCreateShader(GL_FRAGMENT_SHADER);
    }
    else if (type == Shader::Types::COMPUTE) {
        shader = glCreateShader(GL_COMPUTE_SHADER);
    }
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    // Check vertex shader for compilation errors
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

        GLchar* infoLog = new GLchar[logLength];
        glGetShaderInfoLog(shader, logLength, NULL, infoLog);
        if (type == Shader::Types::VERT) {
            std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED : " << std::endl;
            printFormattedSource(source);
            std::cout << infoLog << std::endl;
        }
        else if (type == Shader::Types::GEO) {
            std::cout << "ERROR::SHADER::GEOMETRY::COMPILATION_FAILED : " << std::endl;
            printFormattedSource(source);
            std::cout << infoLog << std::endl;
        }
        else if (type == Shader::Types::FRAG) {
            std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED : " << std::endl;
            printFormattedSource(source);
            std::cout << infoLog << std::endl;
        }
        else if (type == Shader::Types::COMPUTE) {
            std::cout << "ERROR::SHADER::COMPUTE::COMPILATION_FAILED : " << std::endl;
            printFormattedSource(source);
            std::cout << infoLog << std::endl;
        }
        delete[] infoLog;
        return -1;
        //exit(EXIT_FAILURE);
    }
    return shader;
}

GLuint LinkShaders(std::vector<GLuint> shaders) {
    // Link shaders
    GLuint shaderProgram = glCreateProgram();
    for (GLuint shader : shaders) {
        glAttachShader(shaderProgram, shader);
    }
    glLinkProgram(shaderProgram);

    // Check for linking errors
    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &logLength);

        GLchar* infoLog = new GLchar[logLength];
        glGetProgramInfoLog(shaderProgram, logLength, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED : " << std::endl;
        std::cout << infoLog << std::endl;
        glGetError(); // suppress generated error
        return -1;
        //exit(EXIT_FAILURE);
    }

    for (GLuint shader : shaders) { // Shaders are no longer necessary once linked into a program
        glDeleteShader(shader);
    }

    return shaderProgram;
}

GLuint LinkComputeShader(GLuint computeShader) {
    // Link shaders
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, computeShader);
    glLinkProgram(shaderProgram);

    // Check for linking errors
    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &logLength);

        GLchar* infoLog = new GLchar[logLength];
        glGetProgramInfoLog(shaderProgram, logLength, NULL, infoLog);
        std::cout << "ERROR::COMPUTE_SHADER::PROGRAM::LINKING_FAILED : " << std::endl;
        std::cout << infoLog << std::endl;
        glGetError(); // suppress generated error
        return -1;
        //exit(EXIT_FAILURE);
    }

    glDeleteShader(computeShader); // Shaders are no longer necessary once linked into a program

    return shaderProgram;
}

// Function to compile shaders and create a shader program
void LoadDefaultShaders() {
    // Shaders
    //GLuint vertexShader = loadShaderFile(DefaultVertexShaderSource, 0);
    //GLuint fragmentShader2D = loadShaderFile(Default2DFragmentShaderSource, 1);
    //GLuint fragmentShader3D = loadShaderFile(Default3DFragmentShaderSource, 1);
    //
    //GLuint fontVertexShader = loadShaderFile(DefaultFontVertexShaderSource, 0);
    //GLuint fontFragmentShader = loadShaderFile(DefaultFontFragmentShaderSource, 1);
    //
    //extras::Default2DShader.shader = LinkShaders(vertexShader, fragmentShader2D);
    //extras::Default3DShader.shader = LinkShaders(vertexShader, fragmentShader3D);
    //extras::DefaultTextShader.shader = LinkShaders(fontVertexShader, fontFragmentShader);

    //extras::Default2DShader.name = "Default2DShader";
    //extras::Default3DShader.name = "Default3DShader";
    //extras::DefaultTextShader.name = "DefaultTextShader";

    extras::Default2DShader.loadFromFile(DefaultVertexShaderSource, Default2DFragmentShaderSource);
    extras::Default3DShader.loadFromFile(DefaultVertexShaderSource, Default3DFragmentShaderSource);
    extras::DefaultSkyboxShader.loadFromFile(DefaultSkyboxVertexShaderSource, DefaultSkyboxFragmentShaderSource);
    extras::DefaultTextShader.loadFromFile(DefaultFontVertexShaderSource, DefaultFontFragmentShaderSource);
    extras::BitmapTextShader.loadFromFile(DefaultFontVertexShaderSource, BitmapFontFragmentShaderSource);
    extras::DefaultLineShader.loadFromFile(Default2DLineVertexShaderSource, Default2DLineFragmentShaderSource);
    extras::Default2DBlurShader.loadFromFile(DefaultVertexShaderSource, Default2DBlurFragmentShaderSource);
    extras::Default2DGeometryGlassShader.loadFromFile(DefaultVertexShaderSource, Default2DGeometryGlassShaderSource);

    BeginShaderMode(extras::Default2DShader);
}


void externalDraw() {
    if (extras::renderFunction != nullptr) {
        extras::renderFunction();
    }
}

static void window_refresh_callback(GLFWwindow* window) {
    externalDraw();
}

static void window_pos_callback(GLFWwindow* window, int xpos, int ypos) {
    if (extras::WindowFrozen == false) {
        extras::WindowFrozen = true;
        extras::rawFreezeStartTime = GetRawTime();
    }
    externalDraw();
}

//static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
//    extras::WindowSizeChanged = true;
//
//    extras::CurrentWindowWidth = width;
//    extras::CurrentWindowHeight = height;
//    glViewport(0, 0, extras::CurrentWindowWidth, extras::CurrentWindowHeight);  // Ensure the viewport matches the new window size
//    externalDraw();
//}

static void window_size_callback(GLFWwindow* window, int width, int height) {
    if (extras::WindowFrozen == false) {
        extras::WindowFrozen = true;
        extras::rawFreezeStartTime = GetRawTime();
    }
    extras::WindowSizeChanged = true;

    extras::CurrentWindowWidth = width;
    extras::CurrentWindowHeight = height;
    //glViewport(0, 0, extras::CurrentWindowWidth, extras::CurrentWindowHeight);  // Ensure the viewport matches the new window size
    externalDraw();
}

// Callback function for window focus events
static void window_focus_callback(GLFWwindow* window, int focused) {
    extras::WindowFocused = focused;
}

static void mouse_wheel_callback(GLFWwindow* window, double xoffset, double yoffset) {
    extras::mouseWheelOffset += glm::vec2(static_cast<float>(xoffset), static_cast<float>(yoffset));
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    extras::keyCallbacksData.push_back({ key, scancode, action, mods });
}

static void char_callback(GLFWwindow* window, unsigned int codepoint) {
    extras::codepointsData.push_back(codepoint);
}



const char* getGLErrorString(GLenum error) {
    switch (error) {
    case GL_NO_ERROR:
        return "No error";
    case GL_INVALID_ENUM:
        return "Invalid enum";
    case GL_INVALID_VALUE:
        return "Invalid value";
    case GL_INVALID_OPERATION:
        return "Invalid operation : likely wrong type uniform passed OR buffer not bound";
    case GL_STACK_OVERFLOW:
        return "Stack overflow";
    case GL_STACK_UNDERFLOW:
        return "Stack underflow";
    case GL_OUT_OF_MEMORY:
        return "Out of memory";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        return "Invalid framebuffer operation";
    default:
        return "Unknown error";
    }
}

//const char* getGLFWErrorString(int error) // best to use "int code = glfwGetError(&description);" instead
//{
//    switch (error)
//    {
//    case GLFW_NO_ERROR:
//        return "No error";
//    case GLFW_NOT_INITIALIZED:
//        return "The GLFW library is not initialized";
//    case GLFW_NO_CURRENT_CONTEXT:
//        return "There is no current context";
//    case GLFW_INVALID_ENUM:
//        return "Invalid argument for enum parameter";
//    case GLFW_INVALID_VALUE:
//        return "Invalid value for parameter";
//    case GLFW_OUT_OF_MEMORY:
//        return "Out of memory";
//    case GLFW_API_UNAVAILABLE:
//        return "The requested client API is unavailable";
//    case GLFW_VERSION_UNAVAILABLE:
//        return "The requested client API version is unavailable";
//    case GLFW_PLATFORM_ERROR:
//        return "A platform-specific error occurred";
//    case GLFW_FORMAT_UNAVAILABLE:
//        return "The requested format is unavailable";
//    }
//
//    return "ERROR: UNKNOWN ERROR TOKEN PASSED TO glfwErrorString";
//}


bool WindowShouldClose() {
    // ensure the audio is still connected
    //bool haveDevicesChanged = devicesChanged();
    //if (haveDevicesChanged) {
    //    std::cout << "simulated openal restart!" << std::endl;
    //    //CloseOpenAL()
    //}
    return glfwWindowShouldClose(extras::ActiveWindow);
}




// Function to get the current active shader program
GLuint getCurrentActiveProgram() {
    GLint currentProgram;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    return static_cast<GLuint>(currentProgram);
}

// Function to get the current active shader programs uniforms
void printActiveUniforms(GLuint program) {
    GLint numUniforms = 0;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &numUniforms);

    char uniformName[256]; // Assuming uniform names less than 256 characters
    for (int i = 0; i < numUniforms; ++i) {
        GLint size;
        GLenum type;
        glGetActiveUniform(program, (GLuint)i, sizeof(uniformName), nullptr, &size, &type, uniformName);
        std::cout << "Uniform #" << i << " Type: " << type << " Name: " << uniformName << std::endl;
    }
}


// Set background color (framebuffer clear color)
void ClearBackground(glm::vec4 color) {
    if (extras::ActiveWindow == nullptr) {
        std::cerr << "ClearBackground() Resources for drawing are not available." << std::endl;
        exit(EXIT_FAILURE);
    }

    //glClearColor(color.r, color.g, color.b, color.a);  // Alpha is set to 1.0 for full opacity
    //glClear(GL_COLOR_BUFFER_BIT);  // Adjust based on your framebuffer attachments
    //glClearBufferfi(GL_DEPTH_STENCIL, 0, 0.0f, 1);

    glClearColor(color.r, color.g, color.b, color.a);  // Alpha is set to 1.0 for full opacity
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);  // Adjust based on your framebuffer attachments
    
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}


void StartScissor(Rect rect, Camera2D* cam)
{
    glEnable(GL_SCISSOR_TEST);

    // Get framebuffer size (NOT window size)
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(glfwGetCurrentContext(), &fbWidth, &fbHeight);

    glm::vec2 viewportScale = GetViewportScale(cam);

    // Convert to framebuffer pixels
    int x = (int)std::round(rect.position.x / viewportScale.x);
    int y = (int)std::round(rect.position.y / viewportScale.y);
    int w = (int)std::round(rect.size.x / viewportScale.x);
    int h = (int)std::round(rect.size.y / viewportScale.y);

    glScissor(x, y, w, h);
}

void StopScissor() {
    glDisable(GL_SCISSOR_TEST);
}


// Setup canvas (window framebuffer) to start drawing with default 2d shader
void BeginDrawing(void) {
    if (!extras::WindowFocused) {
        // Re-enable the cursor when the window loses focus
        glfwSetInputMode(extras::ActiveWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    else {
        if (extras::CaptureMouse) {
            // Capture the mouse and hide the cursor
            glfwSetInputMode(extras::ActiveWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else {
            // Re-enable the cursor when the window loses focus
            glfwSetInputMode(extras::ActiveWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }

    // Keep the 2D projection in logical window coordinates after resizing.
    extras::DefaultCamera->currentViewportSize = glm::vec2(
        extras::CurrentWindowWidth, extras::CurrentWindowHeight);
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(extras::ActiveWindow, &framebufferWidth, &framebufferHeight);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    BeginMode2D(*extras::DefaultCamera);
}

//@reentrancy       This function must not be called from a callback.
//@thread_safety    This function must only be called from the main thread.
void PollCustomEvents() {
    LimitFPS();

    extras::keyCallbacksData.clear();
    extras::codepointsData.clear();

    extras::mouseWheelOffset = glm::vec2(0, 0); // Reset every frame

    extras::WindowSizeChanged = false;

    UpdateMouseButtons();
    modalLoopRunning = true;
    glfwPollEvents();
    modalLoopRunning = false;
    //glfwWaitEventsTimeout(0.0);
    //glfwWaitEvents();

    extras::lastMousePosition = extras::CurrentMousePosition;
    extras::CurrentMousePosition = GetMousePosition();
    extras::CurrentMouseScreenPosition = GetMouseToScreen(extras::CurrentMousePosition);

    _GetFrameTime();
}

//@remark           The context of the specified window must be current on the calling thread.
//@thread_safety    This function may be called from any thread.
// End canvas drawing and swap buffers (double buffering)
void EndDrawing(void) {
    extras::WindowFrozen = false;

    // Swap buffers and poll IO events
    glfwSwapBuffers(extras::ActiveWindow);
}

void BeginMode2D(Camera2D& _camera) { //   !!! Currently must be called after BeginShaderMode if custom shaders are used !!!
    glDisable(GL_DEPTH_TEST); // Disable depth testing for 2D rendering
    glDisable(GL_CULL_FACE);
    extras::activeCamera2D = &_camera;
    SetUniforms2D();
}

void EndMode2D() {
    extras::activeCamera2D = nullptr;
    BeginMode2D(*extras::DefaultCamera);
}

void BeginMode3D(Camera3D& _camera) { //   !!! Currently must be called after BeginShaderMode if custom shaders are used !!!
    if (extras::ActiveShader == &extras::Default2DShader) {
        BeginShaderMode(extras::Default3DShader);
    }
    BeginInverseZ();
    glEnable(GL_CULL_FACE);
    if (extras::draw3DMirrored) {
        glFrontFace(GL_CW); // Change to clockwise if the default is counter-clockwise
    }
    else {
        glFrontFace(GL_CCW); // This is the default in OpenGL
    }
    extras::activeCamera3D = &_camera;
    SetUniforms3D();
}

void EndMode3D() {
    EndInverseZ();

    if (extras::ActiveShader == &extras::Default3DShader) {
        BeginShaderMode(extras::Default2DShader);
    }
    extras::activeCamera3D = nullptr;
    BeginMode2D(*extras::DefaultCamera);
}

// Begin custom shader drawing   !!! Currently must be called before any changes to the mode 2d or 3d !!!
void BeginShaderMode(Shader& shader) {
    CheckGLErrors();

    //if (extras::ActiveShader != &shader) {
    // Only push to stack if we actually have a current shader to save
    if (extras::ActiveShader != nullptr) {
        extras::LastActiveShaderStack.push_back(extras::ActiveShader);
    }

    extras::ActiveShader = &shader;
    glUseProgram(shader.shader);

    // Re-sync textures to ensure Slot 0/1/etc are correct for THIS shader
    for (auto const& [unit, textureID] : shader.boundTextureUnits) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, textureID);
    }
    //}

    SetUniforms2D();
}

// End custom shader drawing
void EndShaderMode(void) {
    // 1. Get the previous shader from the stack
    if (!extras::LastActiveShaderStack.empty()) {
        Shader* previous = extras::LastActiveShaderStack.back(); // Get the last element
        extras::LastActiveShaderStack.pop_back();              // Remove it from the vector

        // 2. Set ActiveShader to nullptr temporarily
        // This is important: it prevents BeginShaderMode from pushing the 
        // current (ending) shader BACK onto the stack.
        extras::ActiveShader = nullptr;

        BeginShaderMode(*previous);
    }
    else {
        // Default fallback
        extras::ActiveShader = nullptr;
        BeginShaderMode(extras::Default2DShader);
    }
}

void SwitchDepthMode(DepthModes depthMode) {
    switch (depthMode) {
    case NEVER:    glDepthFunc(GL_NEVER);    break;
    case LESS:     glDepthFunc(GL_LESS);     break;
    case EQUAL:    glDepthFunc(GL_EQUAL);    break;
    case LEQUAL:   glDepthFunc(GL_LEQUAL);   break;
    case GREATER:  glDepthFunc(GL_GREATER);  break;
    case NOTEQUAL: glDepthFunc(GL_NOTEQUAL); break;
    case GEQUAL:   glDepthFunc(GL_GEQUAL);   break;
    case ALWAYS:   glDepthFunc(GL_ALWAYS);   break;
    default: break;
    }
}

void BeginDepthMode(DepthModes depthMode, bool enableDepth) {
    // If we have a current mode, push it to the stack to save it
    if (extras::CurrentActiveDepth != DepthModes::UNINITIALIZED) {
        extras::LastActiveDepthStack.push_back(extras::CurrentActiveDepth);
    }

    extras::CurrentActiveDepth = depthMode;

    // Apply the actual OpenGL state
    SwitchDepthMode(extras::CurrentActiveDepth);

    // Ensure depth testing is enabled if we aren't explicitly disabling it
    if (enableDepth) {
        glEnable(GL_DEPTH_TEST);
    }
}

void EndDepthMode(bool disableDepth) {
    if (!extras::LastActiveDepthStack.empty()) {
        // 1. Get the previous depth mode from the stack
        DepthModes previous = extras::LastActiveDepthStack.back();
        extras::LastActiveDepthStack.pop_back();

        // 2. Temporarily set to UNINITIALIZED
        // This prevents BeginDepthMode from pushing the current mode back onto the stack
        extras::CurrentActiveDepth = DepthModes::UNINITIALIZED;

        BeginDepthMode(previous);
    }
    else {
        // Fallback to a safe default (usually LESS or GREATER depending on Inverse Z)
        extras::CurrentActiveDepth = DepthModes::UNINITIALIZED;
        if (extras::useInverseZBuffer) {
            BeginDepthMode(DepthModes::GREATER);
        }
        else {
            BeginDepthMode(DepthModes::LESS);
        }
    }

    // Ensure depth testing is disabled if we aren't explicitly enabling it
    if (disableDepth) {
        glDisable(GL_DEPTH_TEST);
    }
}

void BeginInverseZ() {
    extras::useInverseZBuffer = true;
    glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
    glClearDepth(0.0);
    
    // Clear the depth buffer to 0.0 right now so the new state takes effect!
    glClear(GL_DEPTH_BUFFER_BIT);
    BeginDepthMode(DepthModes::GREATER);
}

void EndInverseZ() {
    extras::useInverseZBuffer = false;
    glClipControl(GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE);
    glClearDepth(1.0);

    // Clear the depth buffer to 1.0 right now so the new state takes effect!
    glClear(GL_DEPTH_BUFFER_BIT);
    EndDepthMode();
}

void BeginBlendingMode(GLint srcAlpha, GLint dstAlpha) {
    extras::LastSrcAlpha = extras::CurrentSrcAlpha;
    extras::LastDstAlpha = extras::CurrentDstAlpha;

    extras::CurrentSrcAlpha = srcAlpha;
    extras::CurrentDstAlpha = dstAlpha;

    glBlendFunc(extras::CurrentSrcAlpha, extras::CurrentDstAlpha);
}
void EndBlendingMode() {
    extras::CurrentSrcAlpha = extras::LastSrcAlpha;
    extras::CurrentDstAlpha = extras::LastDstAlpha;

    glBlendFunc(extras::CurrentSrcAlpha, extras::CurrentDstAlpha);
    CheckGLErrors();
}



bool isUniformValueEqual(const UniformValue& uValue, const UniformValue& value) {
    return std::visit(UniformComparator{ uValue }, value);
}

// Function to bind VAO only if it's different from the currently bound VAO : (only if extras::optimizeGLCalls == true)
void bindVAO(GLuint vao) {
    if (!extras::optimizeGLCalls) {
        glBindVertexArray(vao);
        return;
    }

    GLint currentVAO;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &currentVAO);

    if (static_cast<GLuint>(currentVAO) != vao) {
        glBindVertexArray(vao);
    }
}


Shader::Shader() {

}

Shader::Shader(std::string _name) : name(_name) {

}

Shader::Shader(std::string _name, const char* fragSource) : name(_name) {
    loadFromFile(DefaultVertexShaderSource, fragSource);
}

Shader::Shader(std::string _name, const char* vertSource, const char* fragSource) : name(_name) {
    loadFromFile(vertSource, fragSource);
}

bool Shader::loadFromFile(const char* source, int type) {
    if (type == VERT) {
        vertexShader = loadShaderFile(source, VERT);
    }
    else if (type == GEO) {
        geometryShader = loadShaderFile(source, GEO);
    }
    else if (type == FRAG) {
        fragmentShader = loadShaderFile(source, FRAG);
    }
    else if (type == COMPUTE) {
        computeShader = loadShaderFile(source, COMPUTE);
    }

    if (vertexShader != -1 && fragmentShader != -1) {
        if (geometryShader != -1) {
            shader = LinkShaders({ vertexShader, geometryShader, fragmentShader });
        }
        else {
            shader = LinkShaders({ vertexShader, fragmentShader });
        }
    }
    else if (computeShader != -1) {
        shader = LinkComputeShader(computeShader);
    }
    return true;
}

bool Shader::loadFromFile(const char* vertSource, const char* fragSource) {
    vertexShader = loadShaderFile(vertSource, VERT);
    fragmentShader = loadShaderFile(fragSource, FRAG);

    if (vertexShader != -1 && fragmentShader != -1) {
        shader = LinkShaders({ vertexShader, fragmentShader });
    }
    return true;
}

bool Shader::loadFromFile(const char* vertSource, const char* geometrySource, const char* fragSource) {
    vertexShader = loadShaderFile(vertSource, VERT);
    geometryShader = loadShaderFile(geometrySource, GEO);
    fragmentShader = loadShaderFile(fragSource, FRAG);

    if (vertexShader != -1 && geometryShader != -1 && fragmentShader != -1) {
        shader = LinkShaders({ vertexShader, geometryShader, fragmentShader });
    }
    return true;
}

void Shader::end() {
    CheckGLErrors();

    for (auto unit : boundTextureUnits) {
        glActiveTexture(GL_TEXTURE0 + unit.second);
        glBindTexture(GL_TEXTURE_2D, 0);
        CheckGLErrors();
    }
    //boundTextureUnits.clear();
    for (auto unit : boundImageUnits) {
        glBindImageTexture(unit, 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
        CheckGLErrors();
    }
    //boundImageUnits.clear();

    //loadedUniformValues.clear();
    glActiveTexture(GL_TEXTURE0);
    CheckGLErrors();
}

GLint Shader::SafeGetUniformLocation(std::string uniform) {
    GLuint currentProgram = getCurrentActiveProgram();
    if (currentProgram != shader) {
        std::cout << "shader name : " + name + ", Shader Not Active" << std::endl;
        return -1;
    }

    if (loadedUniformLocations.find(uniform) != loadedUniformLocations.end()) {
        return loadedUniformLocations[uniform];
    }

    if (shader == -1) {
        std::cout << "Failed to find variable " + uniform + " in shader: " + name + " (Shader Not Initialized)" << std::endl;
        return -1;
    }
    GLint res = glGetUniformLocation(shader, uniform.c_str());
    loadedUniformLocations[uniform] = res;
    if (res == -1) {
        std::cout << "Failed to find variable " + uniform + " in shader: " + name + " (wrong shader OR wrong uniform name OR possibly uniform optimized away)" << std::endl;
        return -1;
    }
    return res;
}

GLint Shader::passedUniformChecks(std::string uniform, UniformValue value) {
    GLint index = SafeGetUniformLocation(uniform);
    if (index != -1) {
        if (extras::optimizeGLCalls) {
            // Check if the current value is different from the new value
            auto it = loadedUniformValues.find(uniform);
            if (it == loadedUniformValues.end() || !isUniformValueEqual(it->second, value)) {
                return index;
            }
        }
        else {
            return index;
        }
    }
    return -1;
}

void Shader::setUniform(std::string uniform, Texture& texture, int unit) {
    // If the instance pointer is null, exit safely before touching memory
    if (&texture == nullptr) {
        std::cerr << "[Engine Error] Attempted to bind a NULL texture instance to unit " << unit << "\n";
        return;
    }

    GLint index = SafeGetUniformLocation(uniform);
    if (index != -1) {
        auto it = boundTextureUnits.find(unit);
        if (it == boundTextureUnits.end() || it->second != texture.textureID) {
            glUniform1i(index, unit);
            texture.bind(unit);
            boundTextureUnits[unit] = texture.textureID;
            if (CheckGLErrors()) {
                std::cout << "shader name: " + name + ", uniform name: " << uniform << ", uniform type: Texture&" << std::endl;
                return;
            }
        }
    }
}

void Shader::setUniform(std::string uniform, uint value) {
    GLint index = passedUniformChecks(uniform, value);
    if (index != -1) {
        glUniform1ui(index, value);
        if (CheckGLErrors()) {
            std::cout << "shader name: " + name + ", uniform name: " << uniform << ", uniform type: uint" << std::endl;
            return;
        }
        loadedUniformValues[uniform] = value;
    }
}

void Shader::setUniform(std::string uniform, int value) {
    GLint index = passedUniformChecks(uniform, value);
    if (index != -1) {
        glUniform1i(index, value);
        if (CheckGLErrors()) {
            std::cout << "shader name: " + name + ", uniform name: " << uniform << ", uniform type: int" << std::endl;
            return;
        }
        loadedUniformValues[uniform] = value;
    }
}

void Shader::setUniform(std::string uniform, glm::ivec2 value) {
    GLint index = passedUniformChecks(uniform, value);
    if (index != -1) {
        glUniform2i(index, value.x, value.y);
        //glUniform2iv(SafeGetUniformLocation(uniform), 1, glm::value_ptr(value));
        if (CheckGLErrors()) {
            std::cout << "shader name: " + name + ", uniform name: " << uniform << ", uniform type: glm::ivec2" << std::endl;
            return;
        }
        loadedUniformValues[uniform] = value;
    }
}

void Shader::setUniform(std::string uniform, glm::ivec3 value) {
    GLint index = passedUniformChecks(uniform, value);
    if (index != -1) {
        glUniform3i(index, value.x, value.y, value.z);
        //glUniform3iv(SafeGetUniformLocation(uniform), 1, glm::value_ptr(value));
        if (CheckGLErrors()) {
            std::cout << "shader name: " + name + ", uniform name: " << uniform << ", uniform type: glm::ivec3" << std::endl;
            return;
        }
        loadedUniformValues[uniform] = value;
    }
}

void Shader::setUniform(std::string uniform, glm::ivec4 value) {
    GLint index = passedUniformChecks(uniform, value);
    if (index != -1) {
        glUniform4i(index, value.x, value.y, value.z, value.w);
        //glUniform4iv(SafeGetUniformLocation(uniform), 1, glm::value_ptr(value));
        if (CheckGLErrors()) {
            std::cout << "shader name: " + name + ", uniform name: " << uniform << ", uniform type: glm::ivec4" << std::endl;
            return;
        }
        loadedUniformValues[uniform] = value;
    }
}

void Shader::setUniform(std::string uniform, float value) {
    GLint index = passedUniformChecks(uniform, value);
    if (index != -1) {
        glUniform1f(index, value);
        if (CheckGLErrors()) {
            std::cout << "shader name: " + name + ", uniform name: " << uniform << ", uniform type: float" << std::endl;
            return;
        }
        loadedUniformValues[uniform] = value;
    }
}

void Shader::setUniform(std::string uniform, glm::vec2 value) {
    GLint index = passedUniformChecks(uniform, value);
    if (index != -1) {
        glUniform2f(index, value.x, value.y);
        //glUniform2fv(SafeGetUniformLocation(uniform), 1, glm::value_ptr(value));
        if (CheckGLErrors()) {
            std::cout << "shader name: " + name + ", uniform name: " << uniform << ", uniform type: glm::vec2" << std::endl;
            return;
        }
        loadedUniformValues[uniform] = value;
    }
}

void Shader::setUniform(std::string uniform, glm::vec3 value) {
    GLint index = passedUniformChecks(uniform, value);
    if (index != -1) {
        glUniform3f(index, value.x, value.y, value.z);
        //glUniform3fv(SafeGetUniformLocation(uniform), 1, glm::value_ptr(value));
        if (CheckGLErrors()) {
            std::cout << "shader name: " + name + ", uniform name: " << uniform << ", uniform type: glm::vec3" << std::endl;
            return;
        }
        loadedUniformValues[uniform] = value;
    }
}

void Shader::setUniform(std::string uniform, glm::vec4 value) {
    GLint index = passedUniformChecks(uniform, value);
    if (index != -1) {
        glUniform4f(index, value.x, value.y, value.z, value.w);
        //glUniform4fv(SafeGetUniformLocation(uniform), 1, glm::value_ptr(value));
        if (CheckGLErrors()) {
            std::cout << "shader name: " + name + ", uniform name: " << uniform << ", uniform type: glm::vec4" << std::endl;
            return;
        }
        loadedUniformValues[uniform] = value;
    }
}

void Shader::setUniform(std::string uniform, double value) {
    GLint index = passedUniformChecks(uniform, value);
    if (index != -1) {
        glUniform1d(index, value);
        if (CheckGLErrors()) {
            std::cout << "shader name: " + name + ", uniform name: " << uniform << ", uniform type: double" << std::endl;
            return;
        }
        loadedUniformValues[uniform] = value;
    }
}

void Shader::setUniform(std::string uniform, glm::dvec2 value) {
    GLint index = passedUniformChecks(uniform, value);
    if (index != -1) {
        glUniform2d(index, value.x, value.y);
        //glUniform2dv(SafeGetUniformLocation(uniform), 1, glm::value_ptr(value));
        if (CheckGLErrors()) {
            std::cout << "shader name: " + name + ", uniform name: " << uniform << ", uniform type: glm::dvec2" << std::endl;
            return;
        }
        loadedUniformValues[uniform] = value;
    }
}

void Shader::setUniform(std::string uniform, glm::dvec3 value) {
    GLint index = passedUniformChecks(uniform, value);
    if (index != -1) {
        glUniform3d(index, value.x, value.y, value.z);
        //glUniform3dv(SafeGetUniformLocation(uniform), 1, glm::value_ptr(value));
        if (CheckGLErrors()) {
            std::cout << "shader name: " + name + ", uniform name: " << uniform << ", uniform type: glm::dvec3" << std::endl;
            return;
        }
        loadedUniformValues[uniform] = value;
    }
}

void Shader::setUniform(std::string uniform, glm::dvec4 value) {
    GLint index = passedUniformChecks(uniform, value);
    if (index != -1) {
        glUniform4d(index, value.x, value.y, value.z, value.w);
        //glUniform4dv(SafeGetUniformLocation(uniform), 1, glm::value_ptr(value));
        if (CheckGLErrors()) {
            std::cout << "shader name: " + name + ", uniform name: " << uniform << ", uniform type: glm::dvec4" << std::endl;
            return;
        }
        loadedUniformValues[uniform] = value;
    }
}

void Shader::setUniform(std::string uniform, glm::mat4 value) {
    GLint index = passedUniformChecks(uniform, value);
    if (index != -1) {
        glUniformMatrix4fv(index, 1, GL_FALSE, glm::value_ptr(value));
        //glUniformMatrix4fv(globalDefaultViewUniformLocation, 1, GL_FALSE, glm::value_ptr(extras::activeCamera2D->GetViewMatrix()));
        if (CheckGLErrors()) {
            std::cout << "shader name: " + name + ", uniform name: " << uniform << ", uniform type: glm::mat4" << std::endl;
            return;
        }
        loadedUniformValues[uniform] = value;
    }
}



void Shader::setUniform(std::string uniform, float* values, size_t count) {
    GLint index = SafeGetUniformLocation(uniform);
    if (index != -1) {
        glUniform1fv(index, static_cast<GLsizei>(count), values);
        CheckGLErrors();
    }
}

void Shader::setUniform(std::string uniform, glm::vec2* values, size_t count) {
    GLint index = SafeGetUniformLocation(uniform);
    if (index != -1) {
        glUniform2fv(index, static_cast<GLsizei>(count), glm::value_ptr(values[0]));
        CheckGLErrors();
    }
}



void Shader::setUniform(std::string uniform, std::vector<float>& values) {
    GLint index = SafeGetUniformLocation(uniform);
    if (index != -1) {
        glUniform1fv(index, static_cast<GLsizei>(values.size()), values.data());
        if (CheckGLErrors()) {
            std::cout << "shader name: " + name + ", uniform name: " << uniform << ", uniform type: std::vector<float>" << std::endl;
        }
    }
}

void Shader::setUniform(std::string uniform, std::vector<glm::vec2>& values) {
    GLint index = SafeGetUniformLocation(uniform);
    if (index != -1) {
        glUniform2fv(index, values.size(), glm::value_ptr(values[0]));
        //glUniformMatrix4fv(globalDefaultViewUniformLocation, 1, GL_FALSE, glm::value_ptr(extras::activeCamera2D->GetViewMatrix()));
        if (CheckGLErrors()) {
            std::cout << "shader name: " + name + ", uniform name: " << uniform << ", uniform type: std::vector<glm::vec2>" << std::endl;
            return;
        }
    }
}

void Shader::setUniform(std::string uniform, std::vector<glm::vec3>& values) {
    GLint index = SafeGetUniformLocation(uniform);
    if (index != -1) {
        glUniform3fv(index, values.size(), glm::value_ptr(values[0]));
        //glUniformMatrix4fv(globalDefaultViewUniformLocation, 1, GL_FALSE, glm::value_ptr(extras::activeCamera2D->GetViewMatrix()));
        if (CheckGLErrors()) {
            std::cout << "shader name: " + name + ", uniform name: " << uniform << ", uniform type: std::vector<glm::vec3>" << std::endl;
            return;
        }
    }
}

void Shader::bindImage(Texture& texture, int unit, GL_AccessTypes access) {
    texture.bindForImageAccess(unit, access);
    boundImageUnits.insert(unit);
}





void premultiplyAlphas(unsigned char* data, int width, int height, int channels) {
    if (channels < 4) return;  // Ensure there is an alpha channel

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int index = (y * width + x) * channels;
            float alpha = data[index + 3] / 255.0f;
            data[index] = static_cast<unsigned char>(data[index] * alpha);    // R
            data[index + 1] = static_cast<unsigned char>(data[index + 1] * alpha); // G
            data[index + 2] = static_cast<unsigned char>(data[index + 2] * alpha); // B
            // Alpha channel remains unchanged
        }
    }
}


static std::vector<unsigned char> downsampleRgbaAlphaAware(
    const std::vector<unsigned char>& source, int sourceWidth, int sourceHeight,
    bool sourcePremultiplied) {
    const int destWidth = std::max(1, sourceWidth / 2);
    const int destHeight = std::max(1, sourceHeight / 2);
    std::vector<unsigned char> dest(static_cast<size_t>(destWidth) * destHeight * 4, 0);
    const double scaleX = static_cast<double>(sourceWidth) / destWidth;
    const double scaleY = static_cast<double>(sourceHeight) / destHeight;

    for (int y = 0; y < destHeight; ++y) {
        const double y0 = y * scaleY;
        const double y1 = (y + 1) * scaleY;
        const int firstSourceY = static_cast<int>(std::floor(y0));
        const int lastSourceY = std::min(sourceHeight - 1, static_cast<int>(std::ceil(y1)) - 1);
        for (int x = 0; x < destWidth; ++x) {
            const double x0 = x * scaleX;
            const double x1 = (x + 1) * scaleX;
            const int firstSourceX = static_cast<int>(std::floor(x0));
            const int lastSourceX = std::min(sourceWidth - 1, static_cast<int>(std::ceil(x1)) - 1);
            double totalWeight = 0.0;
            double alphaWeight = 0.0;
            double colorWeight[3] = { 0.0, 0.0, 0.0 };
            double plainColor[3] = { 0.0, 0.0, 0.0 };

            for (int sourceY = firstSourceY; sourceY <= lastSourceY; ++sourceY) {
                const double weightY = std::max(0.0,
                    std::min(y1, static_cast<double>(sourceY + 1)) - std::max(y0, static_cast<double>(sourceY)));
                for (int sourceX = firstSourceX; sourceX <= lastSourceX; ++sourceX) {
                    const double weightX = std::max(0.0,
                        std::min(x1, static_cast<double>(sourceX + 1)) - std::max(x0, static_cast<double>(sourceX)));
                    const double weight = weightX * weightY;
                    if (weight <= 0.0) continue;

                    const size_t sourceIndex = (static_cast<size_t>(sourceY) * sourceWidth + sourceX) * 4;
                    const double alpha = source[sourceIndex + 3] / 255.0;
                    totalWeight += weight;
                    alphaWeight += weight * alpha;
                    for (int channel = 0; channel < 3; ++channel) {
                        const double color = source[sourceIndex + channel] / 255.0;
                        colorWeight[channel] += weight * (sourcePremultiplied ? color : color * alpha);
                        plainColor[channel] += weight * color;
                    }
                }
            }

            const size_t destIndex = (static_cast<size_t>(y) * destWidth + x) * 4;
            const double alpha = totalWeight > 0.0 ? alphaWeight / totalWeight : 0.0;
            for (int channel = 0; channel < 3; ++channel) {
                // Fully transparent texels keep their sources' plain colour
                // (edge colours bled outwards), not black: straight-alpha
                // filtering against them would otherwise darken edges.
                const double color = sourcePremultiplied
                    ? (totalWeight > 0.0 ? colorWeight[channel] / totalWeight : 0.0)
                    : alphaWeight > 0.0 ? colorWeight[channel] / alphaWeight
                    : totalWeight > 0.0 ? plainColor[channel] / totalWeight : 0.0;
                dest[destIndex + channel] = static_cast<unsigned char>(std::clamp(
                    static_cast<int>(std::lround(color * 255.0)), 0, 255));
            }
            dest[destIndex + 3] = static_cast<unsigned char>(std::clamp(
                static_cast<int>(std::lround(alpha * 255.0)), 0, 255));
        }
    }

    return dest;
}

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define __STDC_LIB_EXT1__
#include "stb_image_write.h"


// Standalone Base64 encoder helper
std::string base64_encode(const std::string& in) {
    static const std::string table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(table[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(table[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

// Callback function required by stb_image_write to write into a std::string buffer
void stb_write_to_string(void* context, void* data, int size) {
    std::string* buffer = static_cast<std::string*>(context);
    buffer->append(static_cast<const char*>(data), size);
}

// Converts raw pixel bytes to a compressed base64 PNG Data URL
std::string convert_raw_pixels_to_base64_png(const std::vector<unsigned char>& pixels, int width, int height, int channels) {
    if (pixels.empty() || width <= 0 || height <= 0 || channels <= 0) {
        std::cerr << "[Engine Error] Invalid data passed to PNG compressor.\n";
        return "";
    }

    std::string compressed_png_binary;

    // Explicit row stride calculation: width * components per pixel
    int stride_in_bytes = width * channels;

    int success = stbi_write_png_to_func(
        stb_write_to_string,
        &compressed_png_binary,
        width, height, channels,
        pixels.data(),
        stride_in_bytes
    );

    if (!success) {
        std::cerr << "[Engine Error] Failed to compress texture data to PNG.\n";
        return "";
    }

    return "data:image/png;base64," + base64_encode(compressed_png_binary);
}




// Copy Assignment Operator
Image& Image::operator=(const Image& other) {
    if (this != &other) {  // Avoid self-assignment
        image.width = other.image.width;
        image.height = other.image.height;

        // Copy pixel data
        imageData = other.imageData;
        image.pixels = imageData.data(); // Update pointer to new data
    }
    return *this;
}

Image::Image() {

}

Image::Image(const std::string& path, bool flipped) {
    loadFromFile(path, flipped, false);
}

Image::Image(int width, int height) : imageData(width* height * 4) {
    image.width = width;
    image.height = height;
    image.pixels = imageData.data();
}

cv::Mat Image::ToCVImage() {
    // Create a cv::Mat object from the pixel data
    cv::Mat img(image.height, image.width, CV_8UC4, imageData.data());

    // Convert BGR to RGB (if needed)
    cv::cvtColor(img, img, cv::COLOR_BGRA2RGBA);

    return img;
}


void Image::loadFromFile(const std::string& path, bool flipped, bool _preMultiplyAlphas) {
    stbi_set_flip_vertically_on_load(flipped);
    int nChannels;
    unsigned char* imgData = stbi_load(path.c_str(), &image.width, &image.height, &nChannels, STBI_rgb_alpha);
    if (!imgData) {
        std::cerr << "Failed to load image: " << path.c_str() << ", Reason: " << stbi_failure_reason() << std::endl;
        return;
        //exit(EXIT_FAILURE);
    }

    if (_preMultiplyAlphas) {
        premultiplyAlphas(imgData, image.width, image.height, nChannels); // alpha premultiplication
    }

    size_t totalBytes = image.width * image.height * 4;
    imageData.assign(imgData, imgData + totalBytes);
    image.pixels = imageData.data(); // Update pointer to new data
    stbi_image_free(imgData);
}

void Image::saveToFile(const std::string& path) {
    cv::Mat img = ToCVImage();

    // Flip the image vertically
    cv::flip(img, img, 0);

    // Save the image using OpenCV
    if (cv::imwrite(path, img)) {
        std::cout << "Saved image to " << path << std::endl;
    }
    else {
        std::cerr << "Failed to save image to " << path << std::endl;
    }
}

void SaveScreenshot(int width, int height, const std::string& path) {
    // 1. Path Logic: Create directories if they don't exist
    try {
        std::filesystem::path p(path);
        if (p.has_parent_path()) {
            // create_directories creates the full chain (e.g., "captures/renders/today/")
            std::filesystem::create_directories(p.parent_path());
        }
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
        return;
    }

    // 2. Allocate and Read pixels
    std::vector<unsigned char> pixels(width * height * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    // 3. Create CV Matrix and Flip (Corrects OpenGL's bottom-left origin)
    cv::Mat img(height, width, CV_8UC4, pixels.data());
    cv::flip(img, img, 0);

    // 4. Convert RGBA to BGR (OpenCV's preferred save format)
    cv::Mat finalImg;
    cv::cvtColor(img, finalImg, cv::COLOR_RGBA2BGR);

    // 5. Save with confirmation
    if (cv::imwrite(path, finalImg)) {
        std::filesystem::path p(path);

        if (p.is_absolute()) {
            // It was a path like "F:/Screenshots/img.png"
            std::cout << "Success: Saved to Absolute Path -> " << p << std::endl;
        }
        else {
            // It was a path like "captures/img.png"
            // We show the relative name and the full path for convenience
            std::cout << "Success: Saved to Relative Path -> " << p << std::endl;
        }
    }
    else {
        std::cerr << "Error: OpenCV imwrite failed for " << path << std::endl;
    }
}




// Copy assignment operator
Texture& Texture::operator=(const Texture& other) {
    if (this != &other) { // Avoid self-assignment
        if (other.textureID == 0) {
            std::cerr << "Warning: Trying to copy an invalid texture!" << std::endl;
            return *this; // Don't copy if the other texture is invalid
        }

        textureData = other.textureData;
        setupTexture(other.width, other.height, other.filterMode, other.wrapMode, other.newInternalGLFormat, other.newExternalGLFormat, other.dataFormat, other.newNChannels);
        update(textureData.data());

        //std::vector<float> data = other.getTextureDataAsFloats();
        //updateTexture(data);
    }
    return *this;
}

//setupTexture(w, h, LINEAR, CLAMP_TO_EDGE,  GL_RGBA32F, GL_RGBA, 4);
//                   NEAREST, CLAMP_TO_EDGE, GL_RGBA8, GL_RGBA, 4);
//                                           GL_R32F, GL_RED, 1);

Texture::Texture() {

}

Texture::Texture(const std::string& path, bool flipped, bool _preMultiplyAlphas, bool useMipmaps, float svgRasterScale) {
    loadFromFile(path, flipped, _preMultiplyAlphas, useMipmaps, svgRasterScale);
}

Texture::Texture(int width, int height, GL_FilterModes filterMode, GL_WrapModes wrapMode, GL_InternalFormats newInternalGLFormat, GL_ExternalFormats newExternalGLFormat, GL_DataFormats dataFormat, int newNChannels) {
    setupTexture(width, height, filterMode, wrapMode, newInternalGLFormat, newExternalGLFormat, dataFormat, newNChannels);
}

Texture::Texture(int width, int height, glm::vec4 color) {
    setupTexture(width, height, GL_FilterModes::LINEAR, GL_WrapModes::CLAMP_TO_EDGE, GL_InternalFormats::RGBA8, GL_ExternalFormats::RGBA, GL_DataFormats::FLOAT, 4);

    std::vector<glm::vec4> data(width * height * 4);
    for (int i = 0; i < width * height; i++) {
        data[i] = color;
    }

    update(data);
}

Texture::Texture(int _width, int _height, std::vector<unsigned char> _textureData) {
    width = _width;
    height = _height;
    textureData = _textureData;

    setupTexture(width, height, GL_FilterModes::LINEAR, GL_WrapModes::REPEAT, GL_InternalFormats::RGBA8, GL_ExternalFormats::RGBA, GL_DataFormats::UNSIGNED_BYTE, 4);
    update(textureData.data());
}

Texture::~Texture() {
    if (width > 0)
        glDeleteTextures(1, &textureID);
}

void Texture::bind(int _unit) {
    // If the instance pointer is null, exit safely before touching memory
    if (this == nullptr) {
        std::cerr << "[Engine Error] Attempted to bind a NULL texture instance to unit " << _unit << "\n";
        return;
    }

    unit = _unit;
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, textureID);

    CheckGLErrors();
}

void Texture::unbind() {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, 0);

    glActiveTexture(GL_TEXTURE0);
    CheckGLErrors();
}

void Texture::bindForImageAccess(int imageUnit, GL_AccessTypes access) {
    GLenum GL_Access;
    if (access == GL_AccessTypes::READ_ONLY) {
        GL_Access = GL_READ_ONLY;
    }
    else if (access == GL_AccessTypes::WRITE_ONLY) {
        GL_Access = GL_WRITE_ONLY;
    }
    else if (access == GL_AccessTypes::READ_WRITE) {
        GL_Access = GL_READ_WRITE;
    }

    glBindImageTexture(imageUnit, textureID, 0, GL_FALSE, 0, GL_Access, internalGLFormat);

    CheckGLErrors();
}

void Texture::unbindFromImageAccess() {
    glBindImageTexture(unit, 0, 0, GL_FALSE, 0, GL_READ_ONLY, internalGLFormat);

    CheckGLErrors();
}

void Texture::clear() {
    glBindTexture(GL_TEXTURE_2D, textureID);
    std::vector<unsigned char> clearData(width * height * nChannels, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, internalGLFormat, width, height, 0, externalGLFormat, GL_UNSIGNED_BYTE, clearData.data());
}

void colorAdjacentPixels(unsigned char* data, int width, int height, int channels) {
    if (channels < 4) return;  // Ensure there is an alpha channel

    // Temporary buffer to store the modified image
    unsigned char* newData = new unsigned char[width * height * channels];
    memcpy(newData, data, width * height * channels);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int index = (y * width + x) * channels;
            if (data[index + 3] > 0) {  // Check for non-zero alpha
                // Check adjacent pixels
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                            int nindex = (ny * width + nx) * channels;
                            if (data[nindex + 3] == 0) {  // Target pixels with zero alpha
                                // Average color of the current non-zero alpha pixel and the adjacent non-zero alpha pixels
                                int count = 1;
                                int sumR = data[index];
                                int sumG = data[index + 1];
                                int sumB = data[index + 2];
                                // Collect neighboring non-zero alpha pixels
                                for (int a_dy = -1; a_dy <= 1; ++a_dy) {
                                    for (int a_dx = -1; a_dx <= 1; ++a_dx) {
                                        int a_nx = x + a_dx;
                                        int a_ny = y + a_dy;
                                        if (a_nx >= 0 && a_nx < width && a_ny >= 0 && a_ny < height && !(a_dx == 0 && a_dy == 0)) {
                                            int a_index = (a_ny * width + a_nx) * channels;
                                            if (data[a_index + 3] > 0) {
                                                sumR += data[a_index];
                                                sumG += data[a_index + 1];
                                                sumB += data[a_index + 2];
                                                count++;
                                            }
                                        }
                                    }
                                }
                                // Assign averaged color
                                newData[nindex] = sumR / count;  // R
                                newData[nindex + 1] = sumG / count; // G
                                newData[nindex + 2] = sumB / count; // B
                                newData[nindex + 3] = 0;  // A set to 0
                            }
                        }
                    }
                }
            }
        }
    }

    memcpy(data, newData, width * height * channels);
    delete[] newData;
}

void Texture::loadFromFile(const std::string& path, bool flipped, bool _preMultiplyAlphas, bool useMipmaps, float svgRasterScale) {
    const size_t extensionStart = path.find_last_of('.');
    bool isSvg = extensionStart != std::string::npos;
    if (isSvg) {
        const std::string extension = path.substr(extensionStart);
        isSvg = extension.size() == 4 &&
            std::tolower(static_cast<unsigned char>(extension[1])) == 's' &&
            std::tolower(static_cast<unsigned char>(extension[2])) == 'v' &&
            std::tolower(static_cast<unsigned char>(extension[3])) == 'g';
    }

    if (isSvg) {
        NSVGimage* svgImage = nsvgParseFromFile(path.c_str(), "px", 96.0f);
        if (!svgImage) {
            width = height = -1;
            std::cerr << "Failed to load SVG texture: " << path << std::endl;
            return;
        }

        constexpr float maxRasterDimension = 4096.0f;
        if (!std::isfinite(svgRasterScale) || svgRasterScale <= 0.0f) {
            nsvgDelete(svgImage);
            width = height = -1;
            std::cerr << "SVG raster scale must be positive and finite: " << path << std::endl;
            return;
        }
        const float largestSourceDimension = std::max(svgImage->width, svgImage->height);
        if (!std::isfinite(svgImage->width) || !std::isfinite(svgImage->height) ||
            svgImage->width <= 0.0f || svgImage->height <= 0.0f ||
            !std::isfinite(largestSourceDimension)) {
            nsvgDelete(svgImage);
            width = height = -1;
            std::cerr << "SVG has invalid dimensions: " << path << std::endl;
            return;
        }

        const float rasterScale = std::min(svgRasterScale, maxRasterDimension / largestSourceDimension);
        const int rasterWidth = std::max(1, static_cast<int>(std::ceil(svgImage->width * rasterScale)));
        const int rasterHeight = std::max(1, static_cast<int>(std::ceil(svgImage->height * rasterScale)));
        NSVGrasterizer* rasterizer = nsvgCreateRasterizer();
        if (!rasterizer) {
            nsvgDelete(svgImage);
            width = height = -1;
            std::cerr << "Failed to create SVG rasterizer for: " << path << std::endl;
            return;
        }

        std::vector<unsigned char> rgba(static_cast<size_t>(rasterWidth) * rasterHeight * 4, 0);
        nsvgRasterize(rasterizer, svgImage, 0.0f, 0.0f, rasterScale,
            rgba.data(), rasterWidth, rasterHeight, rasterWidth * 4);
        nsvgDeleteRasterizer(rasterizer);
        nsvgDelete(svgImage);

        if (flipped) {
            const size_t rowBytes = static_cast<size_t>(rasterWidth) * 4;
            for (int y = 0; y < rasterHeight / 2; ++y) {
                auto top = rgba.begin() + static_cast<size_t>(y) * rowBytes;
                auto bottom = rgba.begin() + static_cast<size_t>(rasterHeight - y - 1) * rowBytes;
                std::swap_ranges(top, top + rowBytes, bottom);
            }
        }

        if (_preMultiplyAlphas)
            premultiplyAlphas(rgba.data(), rasterWidth, rasterHeight, 4);

        width = rasterWidth;
        height = rasterHeight;
        nChannels = 4;
        textureData = std::move(rgba);
        setupTexture(width, height, GL_FilterModes::LINEAR, GL_WrapModes::REPEAT,
            GL_InternalFormats::RGBA8, GL_ExternalFormats::RGBA,
            GL_DataFormats::UNSIGNED_BYTE, 4);
        update(textureData.data());
        if (useMipmaps) {
            // Build mip levels with alpha-aware RGB filtering so transparent pixels
            // do not darken SVG outlines.
            wrapMode = GL_WrapModes::CLAMP_TO_EDGE;
            generateAlphaAwareMipmaps(_preMultiplyAlphas);
        }
        return;
    }

    stbi_set_flip_vertically_on_load(flipped);
    unsigned char* imgData = stbi_load(path.c_str(), &width, &height, &nChannels, STBI_rgb_alpha);
    setupTexture(width, height, GL_FilterModes::LINEAR, GL_WrapModes::REPEAT, GL_InternalFormats::RGBA8, GL_ExternalFormats::RGBA, GL_DataFormats::UNSIGNED_BYTE, 4);
    if (!imgData) {
        std::cerr << "Failed to load image: " << path.c_str() << ", Reason: " << stbi_failure_reason() << std::endl;
        return;
        //exit(EXIT_FAILURE);
    }
    // Extracted SWF bitmaps keep stray colors in fully transparent pixels;
    // bleed edge colors into them so filtering never samples that garbage.
    colorAdjacentPixels(imgData, width, height, 4);
    if (_preMultiplyAlphas) {
        premultiplyAlphas(imgData, width, height, nChannels); // alpha premultiplication
    }
    size_t totalBytes = width * height * 4;
    textureData.assign(imgData, imgData + totalBytes);
    update(textureData.data());
    stbi_image_free(imgData);
    if (useMipmaps)
        generateAlphaAwareMipmaps(_preMultiplyAlphas);
    //saveToFile("test.png");
}
//void Texture::saveToFile(const std::string& path) {
//    std::vector<unsigned char> imgData = getTextureDataAsUnsigned(true);
//    stbi_write_png(path.c_str(), width, height, nChannels, imgData.data(), width * nChannels);
//}

void Texture::saveToFile(const std::string& path) {
    //image.saveToFile(path);
}

void Texture::setupTexture(int _width, int _height, GL_FilterModes _filterMode, GL_WrapModes _wrapMode, GL_InternalFormats _newInternalGLFormat, GL_ExternalFormats _newExternalGLFormat, GL_DataFormats _dataFormat, int _newNChannels) {
    glGenTextures(1, &textureID);

    width = _width;
    height = _height;
    filterMode = _filterMode;
    wrapMode = _wrapMode;
    newInternalGLFormat = _newInternalGLFormat;
    newExternalGLFormat = _newExternalGLFormat;
    dataFormat = _dataFormat;
    newNChannels = _newNChannels;

    updateSamplerDetails(filterMode, wrapMode);
    updateFormat(newInternalGLFormat, newExternalGLFormat, dataFormat, newNChannels);
    // TODO: implement bindless textures
    //handle = glGetTextureHandleARB(textureID); 
}

void Texture::updateSamplerDetails(GL_FilterModes _filterMode, GL_WrapModes _wrapMode) {
    filterMode = _filterMode;
    wrapMode = _wrapMode;

    glBindTexture(GL_TEXTURE_2D, textureID);
    if (filterMode == GL_FilterModes::NEAREST) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    else if (filterMode == GL_FilterModes::LINEAR) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else if (filterMode == GL_FilterModes::MIPMAP) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        if (!alphaAwareMipmaps)
            glGenerateMipmap(GL_TEXTURE_2D);
    }
    else if (filterMode == GL_FilterModes::MIPMAP_NEAREST) {
        // Keep source-resolution pixels crisp while blending mip levels only
        // when a texture is minified.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        if (!alphaAwareMipmaps)
            glGenerateMipmap(GL_TEXTURE_2D);
    }
    else { // invalid filter mode
        std::cout << "Invalid filter mode, valid modes are NEAREST, LINEAR, MIPMAP, MIPMAP_NEAREST." << std::endl;
    }

    if (wrapMode == GL_WrapModes::CLAMP) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    }
    else if (wrapMode == GL_WrapModes::CLAMP_TO_EDGE) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    else if (wrapMode == GL_WrapModes::CLAMP_TO_BORDER) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    }
    else if (wrapMode == GL_WrapModes::REPEAT) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    else if (wrapMode == GL_WrapModes::MIRRORED_REPEAT) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    }
    else { // invalid wrap mode
        std::cout << "Invalid wrap mode, valid modes are CLAMP, CLAMP_TO_EDGE, CLAMP_TO_BORDER, REPEAT." << std::endl;
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    CheckGLErrors();
}

bool Texture::updateSafetyCheck(std::vector<float>& data) {
    if (width * height * nChannels == data.size()) {
        return true;
    }
    else {
        std::cout << "updateTexture(float) data size mismatch, dataSize: " << data.size() << ", expectedSize: " << width * height * nChannels << std::endl;
        return false;
    }
}

void Texture::updateFormat(GL_InternalFormats newInternalGLFormat, GL_ExternalFormats newExternalGLFormat, GL_DataFormats dataFormat, int newNChannels) {
    glBindTexture(GL_TEXTURE_2D, textureID);

    if (newInternalGLFormat == GL_InternalFormats::R8) {
        internalGLFormat = GL_R8;
    }
    else if (newInternalGLFormat == GL_InternalFormats::RG8) {
        internalGLFormat = GL_RG8;
    }
    else if (newInternalGLFormat == GL_InternalFormats::RGB8) {
        internalGLFormat = GL_RGB8;
    }
    else if (newInternalGLFormat == GL_InternalFormats::RGBA8) {
        internalGLFormat = GL_RGBA8;
    }

    else if (newInternalGLFormat == GL_InternalFormats::R16) {
        internalGLFormat = GL_R16;
    }
    else if (newInternalGLFormat == GL_InternalFormats::RG16) {
        internalGLFormat = GL_RG16;
    }
    else if (newInternalGLFormat == GL_InternalFormats::RGB16) {
        internalGLFormat = GL_RGB16;
    }
    else if (newInternalGLFormat == GL_InternalFormats::RGBA16) {
        internalGLFormat = GL_RGBA16;
    }

    else if (newInternalGLFormat == GL_InternalFormats::R16F) {
        internalGLFormat = GL_R16F;
    }
    else if (newInternalGLFormat == GL_InternalFormats::RG16F) {
        internalGLFormat = GL_RG16F;
    }
    else if (newInternalGLFormat == GL_InternalFormats::RGB16F) {
        internalGLFormat = GL_RGB16F;
    }
    else if (newInternalGLFormat == GL_InternalFormats::RGBA16F) {
        internalGLFormat = GL_RGBA16F;
    }

    else if (newInternalGLFormat == GL_InternalFormats::R32F) {
        internalGLFormat = GL_R32F;
    }
    else if (newInternalGLFormat == GL_InternalFormats::RG32F) {
        internalGLFormat = GL_RG32F;
    }
    else if (newInternalGLFormat == GL_InternalFormats::RGB32F) {
        internalGLFormat = GL_RGB32F;
    }
    else if (newInternalGLFormat == GL_InternalFormats::RGBA32F) {
        internalGLFormat = GL_RGBA32F;
    }

    else if (newInternalGLFormat == GL_InternalFormats::DEPTH_COMPONENT24) {
        internalGLFormat = GL_DEPTH_COMPONENT24;
    }
    else if (newInternalGLFormat == GL_InternalFormats::DEPTH_COMPONENT32) {
        internalGLFormat = GL_DEPTH_COMPONENT32;
    }
    else if (newInternalGLFormat == GL_InternalFormats::DEPTH_COMPONENT32F) {
        internalGLFormat = GL_DEPTH_COMPONENT32F;
    }
    else { // invalid internal GL Format
        std::cout << "Invalid Internal GL Format, valid modes are R[G][B][A](8/16), R[G][B][A](16/32)F, DEPTH_COMPONENT(24/32), DEPTH_COMPONENT32F." << std::endl;
    }

    if (newExternalGLFormat == GL_ExternalFormats::RED) {
        externalGLFormat = GL_RED;
    }
    else if (newExternalGLFormat == GL_ExternalFormats::RG) {
        externalGLFormat = GL_RG;
    }
    else if (newExternalGLFormat == GL_ExternalFormats::RGB) {
        externalGLFormat = GL_RGB;
    }
    else if (newExternalGLFormat == GL_ExternalFormats::RGBA) {
        externalGLFormat = GL_RGBA;
    }
    else if (newExternalGLFormat == GL_ExternalFormats::DEPTH_COMPONENT) {
        externalGLFormat = GL_DEPTH_COMPONENT;
    }
    else { // invalid external GL Format
        std::cout << "Invalid External GL Format, valid modes are RED, RGBA, DEPTH_COMPONENT." << std::endl;
    }

    GLint format = GL_FLOAT; // default to float

    if (dataFormat == GL_DataFormats::BYTE) {
        format = GL_BYTE;
    }
    else if (dataFormat == GL_DataFormats::UNSIGNED_BYTE) {
        format = GL_UNSIGNED_BYTE;
    }
    else if (dataFormat == GL_DataFormats::SHORT) {
        format = GL_SHORT;
    }
    else if (dataFormat == GL_DataFormats::UNSIGNED_SHORT) {
        format = GL_UNSIGNED_SHORT;
    }
    else if (dataFormat == GL_DataFormats::INT) {
        format = GL_INT;
    }
    else if (dataFormat == GL_DataFormats::UNSIGNED_INT) {
        format = GL_UNSIGNED_INT;
    }
    else if (dataFormat == GL_DataFormats::HALF_FLOAT) {
        format = GL_HALF_FLOAT;
    }
    else if (dataFormat == GL_DataFormats::FLOAT) {
        format = GL_FLOAT;
    }
    else { // invalid data Format
        std::cout << "Invalid Data Format, valid modes are (UNSIGNED_)BYTE, (UNSIGNED_)SHORT, (UNSIGNED_)INT, (HALF_)FLOAT." << std::endl;
    }

    nChannels = newNChannels;

    glTexImage2D(GL_TEXTURE_2D, 0, internalGLFormat, width, height, 0, externalGLFormat, format, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    CheckGLErrors();
}

void Texture::update(unsigned char* data) {
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, internalGLFormat, width, height, 0, externalGLFormat, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);

    CheckGLErrors();
}

void Texture::update(std::vector<float>& data) {
    if (updateSafetyCheck(data)) {
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalGLFormat, width, height, 0, externalGLFormat, GL_FLOAT, data.data());
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    CheckGLErrors();
}

void Texture::update(std::vector<glm::vec4>& data) {
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, internalGLFormat, width, height, 0, externalGLFormat, GL_FLOAT, data.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    CheckGLErrors();
}

void Texture::update(glm::vec4 data) {
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, internalGLFormat, width, height, 0, externalGLFormat, GL_FLOAT, &data);
    glBindTexture(GL_TEXTURE_2D, 0);

    CheckGLErrors();
}

std::vector<float> Texture::getTextureDataAsFloats() const {
    // Ensure the texture is bound
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Create a vector to hold the texture data
    std::vector<float> pixels(width * height * nChannels);

    // Get the texture data
    glGetTexImage(GL_TEXTURE_2D, 0, externalGLFormat, GL_FLOAT, pixels.data());

    // Unbind the texture
    glBindTexture(GL_TEXTURE_2D, 0);

    CheckGLErrors();

    return pixels;
}

std::vector<unsigned char> Texture::getTextureDataAsUnsigned(bool flipped) {
    // Ensure the texture is bound
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Create a vector to hold the texture data
    std::vector<unsigned char> pixels(width * height * nChannels);

    // Get the texture data
    glGetTexImage(GL_TEXTURE_2D, 0, externalGLFormat, GL_UNSIGNED_BYTE, pixels.data());

    // Unbind the texture
    glBindTexture(GL_TEXTURE_2D, 0);

    if (flipped) {
        pixels = FlipData(pixels);
    }

    CheckGLErrors();

    return pixels;
}

std::vector<unsigned char> Texture::FlipData(std::vector<unsigned char> pixels) {
    // Ensure the vector is the correct size
    if (pixels.size() != width * height * nChannels) {
        throw std::runtime_error("Pixel data size does not match texture dimensions.");
    }

    std::vector<unsigned char> flippedPixels(pixels.size());

    // Iterate through each row and copy it to the corresponding row in the flipped image
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < nChannels; ++c) {
                flippedPixels[(height - 1 - y) * width * nChannels + x * nChannels + c] =
                    pixels[y * width * nChannels + x * nChannels + c];
            }
        }
    }

    return flippedPixels;
}

void Texture::generateMipmap() {
    glBindTexture(GL_TEXTURE_2D, textureID);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    CheckGLErrors();
}

void Texture::generateAlphaAwareMipmaps(bool premultiplied) {
    if (width <= 0 || height <= 0 ||
        textureData.size() < static_cast<size_t>(width) * height * 4)
        return;
    alphaAwareMipmaps = true;
    glBindTexture(GL_TEXTURE_2D, textureID);
    int mipWidth = width;
    int mipHeight = height;
    int mipLevel = 1;
    std::vector<unsigned char> mipPixels = textureData;
    while (mipWidth > 1 || mipHeight > 1) {
        std::vector<unsigned char> nextMip = downsampleRgbaAlphaAware(
            mipPixels, mipWidth, mipHeight, premultiplied);
        mipWidth = std::max(1, mipWidth / 2);
        mipHeight = std::max(1, mipHeight / 2);
        glTexImage2D(GL_TEXTURE_2D, mipLevel++, internalGLFormat,
            mipWidth, mipHeight, 0, externalGLFormat, GL_UNSIGNED_BYTE, nextMip.data());
        mipPixels = std::move(nextMip);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    updateSamplerDetails(GL_FilterModes::MIPMAP, wrapMode);
    CheckGLErrors();
}

// TODO: implement bindless textures
//void Texture::makeTextureHandleResident() {
//    glMakeTextureHandleResidentARB(handle);
//}
//
//void Texture::makeTextureHandleNonResident() {
//    glMakeTextureHandleNonResidentARB(handle);
//}




FrameBuffer::FrameBuffer(int width, int height, GL_FilterModes filterMode, GL_WrapModes wrapMode, GL_InternalFormats newInternalGLFormat, GL_ExternalFormats newExternalGLFormat, GL_DataFormats dataFormat, int newNChannels) : width(width), height(height) {
    texture = new Texture(width, height, filterMode, wrapMode, newInternalGLFormat, newExternalGLFormat, dataFormat, newNChannels);
    customRect = glm::vec4(0, 0, width, height);
    if (texture->internalGLFormat == GL_DEPTH_COMPONENT24 || texture->internalGLFormat == GL_DEPTH_COMPONENT32F) {
        setupDepthFramebuffer();
    }
    else {
        setupFramebuffer();
    }
}

FrameBuffer::FrameBuffer(const std::string& filepath) {
    texture = new Texture(filepath);
    width = texture->width;
    height = texture->height;
    customRect = glm::vec4(0, 0, width, height);
    setupFramebuffer();
}

//FrameBuffer::FrameBuffer(Texture& _texture) : texture = _texture {
//    customRect = glm::vec4(0, 0, width, height);
//    setupFramebuffer();
//}

FrameBuffer::~FrameBuffer() {
    cleanup();
}

// Custom Move Constructor
FrameBuffer::FrameBuffer(FrameBuffer&& other) noexcept
    : fbo(0), texture(nullptr), rbo(0), width(0), height(0) {
    *this = std::move(other); // Reuse your existing Move Assignment Operator
}

// Custom Move Assignment Operator
FrameBuffer& FrameBuffer::operator=(FrameBuffer&& other) noexcept {
    if (this != &other) {
        // Free existing resources
        cleanup();

        // Transfer ownership of resources from `other`
        fbo = other.fbo;
        texture = other.texture;
        rbo = other.rbo;
        width = other.width;
        height = other.height;
        color = other.color;
        prevViewportWidth = other.prevViewportWidth;
        prevViewportHeight = other.prevViewportHeight;
        customRect = other.customRect;

        // Reset `other` object to prevent it from deleting resources
        other.fbo = 0;
        other.texture = nullptr;
        other.rbo = 0;
        other.width = 0;
        other.height = 0;
    }
    return *this;
}

void FrameBuffer::startDrawing() {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    //extras::CurrentViewportWidth = width;
    //extras::CurrentViewportHeight = height;
    //prevViewportWidth = extras::activeCamera2D->currentViewportSize.x;
    //prevViewportHeight = extras::activeCamera2D->currentViewportSize.y;
    //extras::activeCamera2D->currentViewportSize = glm::vec2(width, height);
    glViewport(0, 0, width, height); // draw to entire framebuffer (must match framebuffer size)
    SetUniforms2D();
}

void FrameBuffer::stopDrawing() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    //extras::CurrentViewportWidth = extras::TargetViewportWidth;
    //extras::CurrentViewportHeight = extras::TargetViewportHeight;
    //extras::activeCamera2D->currentViewportSize.x = prevViewportWidth;
    //extras::activeCamera2D->currentViewportSize.y = prevViewportHeight;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(extras::ActiveWindow, &framebufferWidth, &framebufferHeight);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    SetUniforms2D();
}

void FrameBuffer::drawFramebufferToScreen() {
    glm::vec2 size = NDCtoScreen(glm::vec2(1.0));
    DrawRectangle(Rect(0,0, size.x, size.y), color, *texture, &customRect);
}

void FrameBuffer::drawFramebufferToScreen(Rect rect) {
    DrawRectangle(rect, color, *texture, &customRect);
}

std::vector<float> FrameBuffer::getTextureDataAsFloatsDirect() {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    std::vector<float> floatPixels(width * height * 1);  // Assuming RGBA format
    glReadPixels(0, 0, width, height, GL_RED, GL_FLOAT, floatPixels.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    CheckGLErrors();

    return floatPixels;
}

void FrameBuffer::setupFramebuffer() {
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Attach the texture to the framebuffer
    glBindTexture(GL_TEXTURE_2D, texture->textureID);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->textureID, 0);

    // Create and setup renderbuffer for depth and stencil
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void FrameBuffer::setupDepthFramebuffer() {
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Attach depth texture only
    glBindTexture(GL_TEXTURE_2D, texture->textureID);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture->textureID, 0);

    // Tell OpenGL there are no color buffers
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void FrameBuffer::cleanup() {
    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &rbo);
    if (texture != nullptr && texture->width > 0)
        delete texture;
}


AnimatedTexture::AnimatedTexture(glm::vec2 frameSize, std::string texturePath) : frameWidth(frameSize.x), frameHeight(frameSize.y), texture(texturePath) {
    nFramesX = floor(float(texture.width) / float(frameWidth));
    nFramesY = floor(float(texture.height) / float(frameHeight));
    nTotalFrames = nFramesX * nFramesY;
    //frame = nFramesX * (nFramesY - 1);
    frame = 0;
}

AnimatedTexture::AnimatedTexture(int nFramesX, int nFramesY, std::string texturePath) : nFramesX(nFramesX), nFramesY(nFramesY), texture(texturePath) {
    frameWidth = floor(float(texture.width) / float(nFramesX));
    frameHeight = floor(float(texture.height) / float(nFramesY));
    nTotalFrames = nFramesX * nFramesY;
    //frame = nFramesX * (nFramesY - 1);
    frame = 0;
}

void AnimatedTexture::Update(double dt) {
    timePassed += dt;
    if (timePassed >= timePerFrame && timePerFrame != -1.0f) {
        frame += step;
        timePassed = 0;
    }

    if (step > 0) {
        if (frame > endFrame || frame > nTotalFrames) {
            frame = loopbackFrame;
        }
    }
    else {
        if (frame < endFrame || frame < 0) {
            frame = loopbackFrame;
        }
    }
}

void AnimatedTexture::UpdateAnimationFrames(float _timePerFrame, int _startFrame, int _loopbackFrame, int _endFrame, int _step) {
    timePerFrame = _timePerFrame;
    frame = _startFrame;
    loopbackFrame = _loopbackFrame;
    endFrame = _endFrame;
    step = _step;
}

void AnimatedTexture::Draw(Rect rect, int frame) {
    int posX = frame % nFramesX;
    int posY = (nFramesY - 1) - (frame / nFramesX);

    //DrawRectangle(position.x, position.y, size.x, size.y, *fbo.texture);

    glm::vec4 customRect = { posX * frameWidth, posY * frameHeight, frameWidth, frameHeight };

    DrawRectangle(rect, texture, &customRect);
}



std::pair<std::string, std::string> splitPath(const std::string& filepath) {
    size_t lastSlash = filepath.find_last_of("/\\");  // Handle both Unix and Windows paths
    size_t lastDot = filepath.rfind('.');

    std::string directory = filepath.substr(0, lastSlash + 1);  // Include the slash in the directory
    std::string filename = filepath.substr(lastSlash + 1, lastDot - lastSlash - 1);  // Exclude the extension

    return { directory, filename };
}

Model::Model() : position(0.0f), scale(1.0f) {

}

Model::Model(std::string objFile) : position(0.0f), scale(1.0f) {
    std::pair<std::string, std::string> res = splitPath(objFile);
    std::string basePath = res.first;
    std::string fileName = res.second;

    LoadModelDataFromOBJ(objFile);
    loadVAOVBO();
    LoadModelTexturesFromMTL(basePath, fileName + ".mtl");
}

Model::Model(const std::vector<Triangle>& tris, Texture& texture) {
    LoadFromTriangles(tris, texture);
}

// Destructor to clean up resources
Model::~Model() {
    CleanUp();
}

// Custom Copy Constructor
Model::Model(const Model& other) : position(other.position), scale(other.scale), orientation(other.orientation),
color(other.color), allMateralsData(other.allMateralsData),  // Copy vector data
storedPositionPairanymore(other.storedPositionPairanymore),  // Copy map data
inflation(other.inflation) {
    loadVAOVBO();
}

// Custom Move Constructor
Model::Model(Model&& other) noexcept
    : position(other.position), scale(other.scale), orientation(other.orientation),
    color(other.color), allMateralsData(std::move(other.allMateralsData)),
    storedPositionPairanymore(std::move(other.storedPositionPairanymore)),
    triangles(std::move(other.triangles)), inflation(other.inflation) {}

// Custom Move Assignment Operator
Model& Model::operator=(Model&& other) noexcept {
    // 1. Self-assignment check
    if (this != &other) {
        // 2. Clean up OUR existing GPU resources before taking new ones
        CleanUp();

        // 3. Transfer simple value types
        this->position = other.position;
        this->scale = other.scale;
        this->orientation = other.orientation;
        this->color = other.color;
        this->inflation = other.inflation;

        // 4. "Steal" the containers (Move semantics)
        // std::move on a vector transfers the internal pointer. 
        // The 'other' vector becomes empty, so its destructor won't loop over materials.
        this->allMateralsData = std::move(other.allMateralsData);
        this->triangles = std::move(other.triangles);
        this->storedPositionPairanymore = std::move(other.storedPositionPairanymore);
    }
    return *this;
}

void Model::LoadModelDataFromOBJ(const std::string& filename, bool useModelNormals, float thresholdAngle) {
    struct RawFace {
        int pos[3] = { -1, -1, -1 };
        int tex[3] = { -1, -1, -1 };
        int norm[3] = { -1, -1, -1 };
    };

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;
    std::unordered_map<std::string, std::vector<RawFace>> facesByMaterial;
    std::string currentMaterial;

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open OBJ: " << filename << std::endl;
        exit(EXIT_FAILURE);
    }

    // Reserve from a rough estimate of the file's record counts to avoid
    // repeated growth while loading large exported meshes.
    file.seekg(0, std::ios::end);
    const std::streamoff fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    const size_t fileBytes = fileSize > 0 ? static_cast<size_t>(fileSize) : 0;
    positions.reserve(fileBytes / 128);
    normals.reserve(fileBytes / 128);
    texCoords.reserve(fileBytes / 160);
    facesByMaterial.reserve(8);

    const auto skipWhitespace = [](std::string_view& text) {
        while (!text.empty() &&
            (text.front() == ' ' || text.front() == '\t' || text.front() == '\r' ||
                text.front() == '\n' || text.front() == '\f' || text.front() == '\v')) {
            text.remove_prefix(1);
        }
    };
    const auto nextToken = [&skipWhitespace](std::string_view& text) {
        skipWhitespace(text);
        const size_t tokenEnd = text.find_first_of(" \t\r\n\f\v");
        const std::string_view token = text.substr(0, tokenEnd);
        text.remove_prefix(tokenEnd == std::string_view::npos ? text.size() : tokenEnd);
        return token;
    };
    const auto parseFloat = [](std::string_view token, float& value) {
        if (!token.empty() && token.front() == '+') {
            token.remove_prefix(1);
        }
        if (token.empty()) return false;
        const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
        return result.ec == std::errc{} && result.ptr == token.data() + token.size();
    };
    const auto parseInt = [](std::string_view token, int& value) {
        if (!token.empty() && token.front() == '+') {
            token.remove_prefix(1);
        }
        if (token.empty()) return false;
        const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
        return result.ec == std::errc{} && result.ptr == token.data() + token.size();
    };

    std::string line;
    while (std::getline(file, line)) {
        std::string_view fields(line);
        const std::string_view type = nextToken(fields);

        if (type == "v") {
            glm::vec3 vertex(0.0f);
            if (parseFloat(nextToken(fields), vertex.x) &&
                parseFloat(nextToken(fields), vertex.y) &&
                parseFloat(nextToken(fields), vertex.z)) {
                positions.push_back(vertex);
            }
        }
        else if (type == "vn") {
            glm::vec3 normal(0.0f);
            if (parseFloat(nextToken(fields), normal.x) &&
                parseFloat(nextToken(fields), normal.y) &&
                parseFloat(nextToken(fields), normal.z)) {
                normals.push_back(glm::normalize(normal));
            }
        }
        else if (type == "vt") {
            glm::vec2 uv(0.0f);
            if (parseFloat(nextToken(fields), uv.x) && parseFloat(nextToken(fields), uv.y)) {
                texCoords.push_back(uv);
            }
        }
        else if (type == "usemtl") {
            const std::string_view material = nextToken(fields);
            if (!material.empty()) {
                currentMaterial.assign(material.data(), material.size());
                if (facesByMaterial.try_emplace(currentMaterial).second) {
                    std::cout << currentMaterial << "\n";
                }
            }
        }
        else if (type == "f") {
            RawFace face;
            bool validFace = true;

            for (int vertexIndex = 0; vertexIndex < 3; ++vertexIndex) {
                const std::string_view definition = nextToken(fields);
                if (definition.empty()) {
                    validFace = false;
                    break;
                }

                std::string_view parts[3];
                size_t partStart = 0;
                for (int partIndex = 0; partIndex < 3; ++partIndex) {
                    const size_t slash = definition.find('/', partStart);
                    parts[partIndex] = definition.substr(partStart,
                        slash == std::string_view::npos ? definition.size() - partStart : slash - partStart);
                    if (slash == std::string_view::npos) {
                        break;
                    }
                    partStart = slash + 1;
                }

                int rawIndex = 0;
                if (!parseInt(parts[0], rawIndex) || rawIndex <= 0) {
                    validFace = false;
                    break;
                }
                face.pos[vertexIndex] = rawIndex - 1;

                if (!parts[1].empty()) {
                    if (!parseInt(parts[1], rawIndex) || rawIndex <= 0) {
                        validFace = false;
                        break;
                    }
                    face.tex[vertexIndex] = rawIndex - 1;
                }

                if (!parts[2].empty()) {
                    if (!parseInt(parts[2], rawIndex) || rawIndex <= 0) {
                        validFace = false;
                        break;
                    }
                    face.norm[vertexIndex] = rawIndex - 1;
                }
            }

            // The model loader currently treats every polygon as its first
            // triangle. Keep that behavior while dropping malformed records.
            if (validFace) {
                facesByMaterial[currentMaterial].push_back(face);
            }
        }
    }

    size_t totalFaceCount = 0;
    for (const auto& materialFaces : facesByMaterial) {
        totalFaceCount += materialFaces.second.size();
    }
    triangles.reserve(triangles.size() + totalFaceCount);

    for (auto& pair : facesByMaterial) {
        const std::string& matName = pair.first;
        std::vector<RawFace>& faces = pair.second;

        // Ignore invalid references instead of indexing outside the OBJ arrays.
        faces.erase(std::remove_if(faces.begin(), faces.end(), [&](const RawFace& face) {
            for (int corner = 0; corner < 3; ++corner) {
                if (face.pos[corner] < 0 || static_cast<size_t>(face.pos[corner]) >= positions.size()) {
                    return true;
                }
            }
            return false;
        }), faces.end());

        MaterialData mat;
        mat.materialName = matName;
        mat.vertices.reserve(faces.size() * 3);

        bool needsGeneratedNormals = !useModelNormals;
        if (!needsGeneratedNormals) {
            for (const RawFace& face : faces) {
                for (int corner = 0; corner < 3; ++corner) {
                    if (face.norm[corner] < 0 || static_cast<size_t>(face.norm[corner]) >= normals.size()) {
                        needsGeneratedNormals = true;
                        break;
                    }
                }
                if (needsGeneratedNormals) {
                    break;
                }
            }
        }

        std::vector<glm::vec3> faceNormals;
        std::unordered_map<int, std::vector<size_t>> vertexToFaceMap;
        if (needsGeneratedNormals) {
            faceNormals.resize(faces.size());
            vertexToFaceMap.reserve(faces.size() * 2);

            for (size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
                const RawFace& face = faces[faceIndex];
                const glm::vec3 p0 = positions[face.pos[0]];
                const glm::vec3 p1 = positions[face.pos[1]];
                const glm::vec3 p2 = positions[face.pos[2]];
                faceNormals[faceIndex] = glm::normalize(glm::cross(p1 - p0, p2 - p0));

                for (int corner = 0; corner < 3; ++corner) {
                    vertexToFaceMap[face.pos[corner]].push_back(faceIndex);
                }
            }
        }

        for (size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
            RawFace& face = faces[faceIndex];
            const glm::vec3 faceNormal = needsGeneratedNormals ? faceNormals[faceIndex] : glm::vec3(0.0f);
            Triangle triangle;

            for (int corner = 0; corner < 3; ++corner) {
                Vertex vertex;
                vertex.position = positions[face.pos[corner]];

                const int normalIndex = face.norm[corner];
                if (useModelNormals && normalIndex >= 0 &&
                    static_cast<size_t>(normalIndex) < normals.size()) {
                    vertex.normal = normals[normalIndex];
                }
                else {
                    glm::vec3 smoothNormal(0.0f);
                    for (size_t adjacentFace : vertexToFaceMap[face.pos[corner]]) {
                        const glm::vec3 adjacentNormal = faceNormals[adjacentFace];
                        const float angle = glm::degrees(glm::acos(glm::clamp(
                            glm::dot(adjacentNormal, faceNormal), -1.0f, 1.0f)));
                        if (angle <= thresholdAngle) {
                            smoothNormal += adjacentNormal;
                        }
                    }
                    vertex.normal = glm::normalize(smoothNormal);
                }

                const int texCoordIndex = face.tex[corner];
                if (texCoordIndex >= 0 && static_cast<size_t>(texCoordIndex) < texCoords.size()) {
                    vertex.texCoord = texCoords[texCoordIndex];
                }
                else {
                    vertex.texCoord = glm::vec2(0.0f);
                }

                mat.vertices.push_back(vertex);
                if (corner == 0) triangle.vertexA = vertex;
                else if (corner == 1) triangle.vertexB = vertex;
                else triangle.vertexC = vertex;
            }

            triangles.push_back(triangle);
        }

        storedPositionPairanymore[matName] = static_cast<int>(allMateralsData.size());
        allMateralsData.push_back(std::move(mat));
    }
}

void Model::LoadModelTexturesFromMTL(const std::string& basePath, const std::string& filename) {
    //std::vector<glm::vec3> originalVertices;
    //std::vector<glm::vec3> originalNormals;
    //std::vector<glm::vec2> originalTexCoords; // Additional vector to store texture coordinates
    std::string currentMaterial;

    std::ifstream file(basePath + filename);
    std::string line;

    if (!file.is_open()) {
        std::cerr << "Failed to open the file: " << filename << std::endl;
        exit(EXIT_FAILURE);
    }

    while (getline(file, line)) {
        std::istringstream ss(line);
        std::string key;
        ss >> key;

        if (key == "newmtl") {
            std::string currentMaterialName;
            ss >> currentMaterialName;
            currentMaterial = currentMaterialName;

        }
        else if (key == "Kd") {
            glm::vec3 diffuseColor;
            ss >> diffuseColor.r >> diffuseColor.g >> diffuseColor.b;
            allMateralsData[storedPositionPairanymore[currentMaterial]].diffuseColor = diffuseColor;
        }
        else if (key == "map_Kd") {
            std::string texturePath;
            ss >> texturePath;
            allMateralsData[storedPositionPairanymore[currentMaterial]].colorTexture = new Texture(basePath + texturePath);
        }
    }


    file.close();
}

void Model::LoadFromTriangles(const std::vector<Triangle>& tris, Texture& texture) {
    ClearGeometry();

    MaterialData mat;
    mat.materialName = "Procedural";

    for (const Triangle& tri : tris)
    {
        triangles.push_back(tri);
        mat.vertices.push_back(tri.vertexA);
        mat.vertices.push_back(tri.vertexB);
        mat.vertices.push_back(tri.vertexC);
    }

    // optional default color
    mat.diffuseColor = glm::vec3(1.0f);
    mat.colorTexture = &texture;

    allMateralsData.push_back(mat);

    loadVAOVBO();
}

void Model::UpdateFromTriangles(const std::vector<Triangle>& tris) {
    if (allMateralsData.empty())
        return;

    MaterialData& mat = allMateralsData[0];

    mat.vertices.clear();

    for (const Triangle& tri : tris)
    {
        mat.vertices.push_back(tri.vertexA);
        mat.vertices.push_back(tri.vertexB);
        mat.vertices.push_back(tri.vertexC);
    }

    glBindBuffer(GL_ARRAY_BUFFER, mat.VBO);
    glBufferData(GL_ARRAY_BUFFER,
        mat.vertices.size() * sizeof(Vertex),
        mat.vertices.data(),
        GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Model::ClearGeometry() {
    triangles.clear();
    for (auto& mat : allMateralsData)
    {
        if (mat.VAO) glDeleteVertexArrays(1, &mat.VAO);
        if (mat.VBO) glDeleteBuffers(1, &mat.VBO);
    }

    allMateralsData.clear();
    storedPositionPairanymore.clear();
}

struct Vec3Hash {
    size_t operator()(const glm::vec3& v) const {
        return std::hash<float>()(v.x) ^ std::hash<float>()(v.y) << 1 ^ std::hash<float>()(v.z) << 2;
    }
};

struct Vec3Equal {
    bool operator()(const glm::vec3& a, const glm::vec3& b) const {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
};

void Model::loadVAOVBO() {
    for (int i = 0; i < allMateralsData.size(); i++) {
        MaterialData& mat = allMateralsData[i];

        // Generate VAO and VBO
        glGenVertexArrays(1, &mat.VAO);
        glGenBuffers(1, &mat.VBO);

        glBindVertexArray(mat.VAO);
        glBindBuffer(GL_ARRAY_BUFFER, mat.VBO);

        // Upload all Vertex data at once
        glBufferData(GL_ARRAY_BUFFER, mat.vertices.size() * sizeof(Vertex), mat.vertices.data(), GL_STATIC_DRAW);

        // Vertex positions (location 0)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
        glEnableVertexAttribArray(0);

        // Normals (location 1)
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(1);

        // Texture coordinates (location 2)
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
        glEnableVertexAttribArray(2);

        // Vertex Color (location 3)
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
        glEnableVertexAttribArray(3);

        // Unbind VBO/VAO
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
}

void Model::updateSamplerDetails(GL_FilterModes filterMode, GL_WrapModes wrapMode) {
    for (auto& mat : allMateralsData) {
        mat.colorTexture->updateSamplerDetails(filterMode, wrapMode);
    }
}

void Model::Draw(bool noColor) {
    SetUniforms3D();

    glm::mat4 model = glm::mat4(1.0f); // Start with the identity matrix
    model = glm::translate(model, position);  // Apply translation
    model = glm::scale(model, scale);  // Apply scaling

    model = model * glm::mat4_cast(orientation);

    extras::ActiveShader->setUniform("model", model);

    for (int i = 0; i < allMateralsData.size(); i++) {
        if (noColor) {
            extras::ActiveShader->setUniform("color", BLACK);
            extras::ActiveShader->setUniform("useTexture", 0); // Indicate NOT to use texture
        }
        else {
            if (allMateralsData[i].colorTexture == nullptr) {
                glm::vec4 diffuseColor = glm::vec4(allMateralsData[i].diffuseColor, 1.0);
                extras::ActiveShader->setUniform("color", diffuseColor);
                extras::ActiveShader->setUniform("useTexture", 0); // Indicate NOT to use texture
            }
            else {
                // set the texture sampler to use texture unit 0
                extras::ActiveShader->setUniform("textureSampler", *allMateralsData[i].colorTexture); // bind texture
                extras::ActiveShader->setUniform("color", color);
                extras::ActiveShader->setUniform("useTexture", 1); // Indicate to use texture
            }
        }

        //glBindVertexArray(allMateralsData[i].VAO);
        bindVAO(allMateralsData[i].VAO);
        glDrawArrays(GL_TRIANGLES, 0, allMateralsData[i].vertices.size());
        if (!extras::optimizeGLCalls) {
            glBindVertexArray(0); // Unbind the VAO
        }
    }

    CheckGLErrors();
}

void Model::CleanUp() {
    for (int i = 0; i < allMateralsData.size(); i++) {
        glDeleteVertexArrays(1, &allMateralsData[i].VAO);
        glDeleteBuffers(1, &allMateralsData[i].VBO);
        //delete allMateralsData[i].colorTexture;
    }
}




double sin01(double x) {
    return 0.5 * (sin(x - PI / 2) + 1);
}

double Clamp(double value, double min, double max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

double Round(double value, int decimals) {
    value = value * pow(10, decimals);
    value = round(value);
    value = value / pow(10, decimals);
    return value;
}

bool IsValidInteger(const std::string& str) {
    bool has_digits = false;
    for (size_t i = 0; i < str.size(); ++i) {
        if (i == 0 && (str[i] == '+' || str[i] == '-')) {
            if (str.size() == 1) return false;  // "+" or "-" alone is not valid
            continue;
        }
        if (!std::isdigit(str[i])) return false;  // Check if every character is a digit
        has_digits = true;
    }
    return has_digits;  // Return true if there's at least one digit
}

bool IsValidFloat(const std::string& str) {
    bool has_digits = false, has_decimal = false, has_exponent = false;
    size_t start = 0;

    if (str.empty()) return false;

    // Handling the sign at the beginning
    if (str[start] == '+' || str[start] == '-') {
        ++start;
        if (start >= str.size()) return false;  // String was only "+" or "-"
    }

    for (size_t i = start; i < str.size(); ++i) {
        if (std::isdigit(str[i])) {
            has_digits = true;
        }
        else if (str[i] == '.' && !has_decimal && !has_exponent) {
            has_decimal = true;  // First decimal point is okay
        }
        else if ((str[i] == 'e' || str[i] == 'E') && has_digits && !has_exponent) {
            has_exponent = true;
            has_digits = false;  // Reset digit flag for digits after exponent
            if (i + 1 < str.size() && (str[i + 1] == '+' || str[i + 1] == '-')) i++;  // Skip the sign immediately after exponent
        }
        else {
            return false;  // Invalid character
        }
    }
    return has_digits;  // Valid float must have at least one digit
}

int safe_stoi(const std::string& str, int default_value) {
    if (IsValidInteger(str)) {
        return std::stoi(str);
    }
    return default_value;
}

float safe_stof(const std::string& str, float default_value) {
    if (IsValidFloat(str)) {
        return std::stof(str);
    }
    return default_value;
}

glm::vec2 safeNormalize(glm::vec2 v) {
    float len = glm::length(v);
    glm::vec2 normalized = glm::normalize(v);

    // Check if any component of the normalized vector is NaN or infinite
    if (std::isfinite(normalized.x) && std::isfinite(normalized.y)) {
        return normalized;
    }

    // Return zero vector if the length is too small or normalization results in NaN or infinity
    return glm::vec2(0.0f);
}

glm::vec3 safeNormalize(glm::vec3 v) {
    float len = glm::length(v);
    glm::vec3 normalized = glm::normalize(v);

    // Check if any component of the normalized vector is NaN or infinite
    if (std::isfinite(normalized.x) && std::isfinite(normalized.y) && std::isfinite(normalized.z)) {
        return normalized;
    }

    // Return zero vector if the length is too small or normalization results in NaN or infinity
    return glm::vec3(0.0f);
}

glm::vec4 getRainbow(float t)
{
    const float r = sin(t);
    const float g = sin(t + 0.33f * TWO_PI);
    const float b = sin(t + 0.66f * TWO_PI);
    return { r * r, g * g, b * b, 1.0f };
}

// Get current screen width
int GetScreenWidth(void) {
    return extras::CurrentWindowWidth;
}

// Get current screen height
int GetScreenHeight(void) {
    return extras::CurrentWindowHeight;
}

glm::vec2 WASDtoVec2() {
    glm::vec2 res = glm::vec2(0);
    if (IsKeyDown(GLFW_KEY_W)) res.y += 1;
    if (IsKeyDown(GLFW_KEY_S)) res.y -= 1;
    if (IsKeyDown(GLFW_KEY_A)) res.x -= 1;
    if (IsKeyDown(GLFW_KEY_D)) res.x += 1;
    res = safeNormalize(res);
    return res;
}



void BaseController::update(float deltaTime, Camera3D& camera) {
    if (IsKeyPressed(GLFW_KEY_ESCAPE)) {
        extras::CaptureMouse = !extras::CaptureMouse;
    }

    // ===============================
    // DERIVE CAMERA AXES FROM QUAT
    // ===============================
    glm::vec3 forward = orientation * glm::vec3(0, 0, -1);
    glm::vec3 right = orientation * glm::vec3(1, 0, 0);
    glm::vec3 up = orientation * glm::vec3(0, 1, 0);
    glm::vec3 worldUp = camera.worldUp;

    camera.forward = safeNormalize(forward);
    camera.right = safeNormalize(right);
    camera.up = safeNormalize(up);

    // ===============================
    // FLATTEN FORWARD/RIGHT FOR HORIZONTAL MOVEMENT
    // ===============================
    glm::vec3 flatForward = forward - glm::dot(forward, worldUp) * worldUp;
    glm::vec3 flatRight = right - glm::dot(right, worldUp) * worldUp;

    flatForward = safeNormalize(flatForward);
    flatRight = safeNormalize(flatRight);

    if (extras::draw3DMirrored) {
        flatRight *= -1.0f;
    }

    // ===============================
    // MOVEMENT
    // ===============================
    glm::vec3 keyboardVelocity(0.0f);
    glm::vec3 gamepadVelocity(0.0f);

    if (IsKeyDown(GLFW_KEY_W)) keyboardVelocity += flatForward;
    if (IsKeyDown(GLFW_KEY_S)) keyboardVelocity -= flatForward;
    if (IsKeyDown(GLFW_KEY_A)) keyboardVelocity -= flatRight;
    if (IsKeyDown(GLFW_KEY_D)) keyboardVelocity += flatRight;

    if (glm::length(keyboardVelocity) > 0.0f)
        keyboardVelocity = safeNormalize(keyboardVelocity) * moveSpeed;

    // Vertical movement (jump/fly)
    if (IsKeyDown(GLFW_KEY_SPACE))      keyboardVelocity += worldUp * verticalSpeed;
    if (IsKeyDown(GLFW_KEY_LEFT_SHIFT)) keyboardVelocity -= worldUp * verticalSpeed;

    float lX = GetGamepadAxisMovement(0, GLFW_GAMEPAD_AXIS_LEFT_X);
    float lY = -GetGamepadAxisMovement(0, GLFW_GAMEPAD_AXIS_LEFT_Y);

    gamepadVelocity += flatRight * lX * moveSpeed;
    gamepadVelocity += flatForward * lY * moveSpeed;

    glm::vec3 totalInputVelocity = keyboardVelocity + gamepadVelocity;

    positionBeforeAnyMovement = position;
    position += (velocity * deltaTime);
    positionBeforeInputMovement = position;
    position += totalInputVelocity * deltaTime;

    // ===============================
    // ROTATION INPUT (YAW + PITCH ONLY)
    // ===============================
    float yawInputSign = extras::draw3DMirrored ? -1.0f : 1.0f;
    const float rotationSpeed = 2.2f;

    float yawDelta = 0.0f;
    float pitchDelta = 0.0f;

    if (IsKeyDown(GLFW_KEY_LEFT))  yawDelta += rotationSpeed * deltaTime * yawInputSign;
    if (IsKeyDown(GLFW_KEY_RIGHT)) yawDelta -= rotationSpeed * deltaTime * yawInputSign;
    if (IsKeyDown(GLFW_KEY_UP))    pitchDelta += rotationSpeed * deltaTime;
    if (IsKeyDown(GLFW_KEY_DOWN))  pitchDelta -= rotationSpeed * deltaTime;

    glm::vec2 mouseDelta = GetMouseDelta();
    if (extras::CaptureMouse && extras::ActiveWindow) {
        yawDelta -= sensitivity.x * glm::radians(mouseDelta.x) * float(extras::CurrentWindowWidth) * yawInputSign;
        pitchDelta += sensitivity.y * glm::radians(mouseDelta.y) * float(extras::CurrentWindowHeight);
    }

    float rX = GetGamepadAxisMovement(0, GLFW_GAMEPAD_AXIS_RIGHT_X);
    float rY = GetGamepadAxisMovement(0, GLFW_GAMEPAD_AXIS_RIGHT_Y);

    yawDelta -= rX * rotationSpeed * deltaTime * yawInputSign;
    pitchDelta -= rY * rotationSpeed * deltaTime;

    // ===============================
    // APPLY ROTATION VIA QUATS WITH PITCH CLAMP
    // ===============================

    // Yaw around world up
    if (yawDelta != 0.0f) {
        glm::quat yawQuat = glm::angleAxis(yawDelta, worldUp);
        orientation = glm::normalize(yawQuat * orientation);
    }

    // Recompute right after yaw
    glm::vec3 rightAfterYaw = orientation * glm::vec3(1, 0, 0);

    // Compute current pitch relative to world up
    glm::vec3 forwardAfterYaw = orientation * glm::vec3(0, 0, -1);
    float currentPitch = glm::asin(glm::clamp(glm::dot(forwardAfterYaw, worldUp), -1.0f, 1.0f));

    // Apply delta and clamp
    currentPitch += pitchDelta;
    const float pitchLimit = glm::radians(89.9f);
    currentPitch = glm::clamp(currentPitch, -pitchLimit, pitchLimit);

    // Rebuild pitch quaternion around right axis
    glm::quat pitchQuat = glm::angleAxis(currentPitch - glm::asin(glm::clamp(glm::dot(forwardAfterYaw, worldUp), -1.0f, 1.0f)), rightAfterYaw);
    orientation = glm::normalize(pitchQuat * orientation);

    camera.update(orientation, position);
}



ThirdPersonController::ThirdPersonController() {
    extras::CaptureMouse = true;
}

void ThirdPersonController::update(float deltaTime) {
    baseController.update(deltaTime, camera);
}





FirstPersonController::FirstPersonController() {
    extras::CaptureMouse = true;
}


void FirstPersonController::update(float deltaTime) {
    baseController.update(deltaTime, camera);
}

glm::vec3 FirstPersonController::getViewPos() {
    return baseController.position + camera.offset;
}


// Setup VAO, VBO and EBO
GLuint global_rectangle_vao, global_rectangle_vbo, global_rectangle_ebo;
GLuint global_circle_vao, global_circle_vbo;
GLuint global_line_vao, global_line_vbo;

GLuint global_triangle_vao, global_triangle_vbo;
std::vector<Vertex> global_triangle_vertices(3); // pre-allocate 3 vertices

GLuint global_cube_vao, global_cube_vbo;
std::vector<Vertex> global_cube_vertices;

// Global handles for your sphere VAO/VBO/EBO
GLuint global_sphere_vao, global_sphere_vbo, global_sphere_ebo, global_sphere_instance_vbo;
size_t global_sphere_index_count;

GLuint global_wireframe_cube_vao, global_wireframe_cube_vbo, global_wireframe_cube_ebo;
std::vector<glm::vec3> global_wireframe_cube_vertices;
std::vector<GLuint> global_wireframe_cube_indices;

GLuint global_skybox_cube_vao, global_skybox_cube_vbo, global_skybox_cube_ebo;
std::vector<Vertex> global_skybox_cube_vertices;
std::vector<GLuint> global_skybox_cube_indices;

void InitRectangleMesh() {
    glGenVertexArrays(1, &global_rectangle_vao);
    glBindVertexArray(global_rectangle_vao);

    glGenBuffers(1, &global_rectangle_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, global_rectangle_ebo);

    unsigned int indices[] = {
        0, 1, 2, // first triangle
        2, 3, 0  // second triangle
    };
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glGenBuffers(1, &global_rectangle_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, global_rectangle_vbo);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glDisableVertexAttribArray(3);

    glBindVertexArray(0);
    CheckGLErrors();
}

void InitCircleMesh() {
    glGenVertexArrays(1, &global_circle_vao);
    glBindVertexArray(global_circle_vao);

    glGenBuffers(1, &global_circle_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, global_circle_vbo);

    GLsizei stride = sizeof(Vertex);

    // Location 0: position (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, position));

    // Location 1: normal (vec3)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, normal));

    // Location 2: texCoord (vec2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, texCoord));

    // Location 3: color (vec3)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, color));

    glBindVertexArray(0);
    CheckGLErrors();
}

void InitLineMesh() {
    glGenVertexArrays(1, &global_line_vao);
    glBindVertexArray(global_line_vao);

    glGenBuffers(1, &global_line_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, global_line_vbo);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    CheckGLErrors();
}

void InitTriangleMesh() {
    glGenVertexArrays(1, &global_triangle_vao);
    glGenBuffers(1, &global_triangle_vbo);

    glBindVertexArray(global_triangle_vao);
    glBindBuffer(GL_ARRAY_BUFFER, global_triangle_vbo);
    glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);

    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);

    // Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);

    // TexCoords
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    CheckGLErrors();
}

void InitSphereMesh(unsigned int rings = 16, unsigned int sectors = 32) {
    std::vector<SphereVertex> vertices;
    std::vector<unsigned int> indices;

    float const R = 1.0f / (float)(rings - 1);
    float const S = 1.0f / (float)(sectors - 1);

    for (unsigned int r = 0; r < rings; ++r) {
        // Polar angle phi: sweeps from 0 (top pole) to PI (bottom pole)
        float const phi = PI * r * R;
        float const sinPhi = sin(phi);
        float const cosPhi = cos(phi);

        for (unsigned int s = 0; s < sectors; ++s) {
            // Azimuthal angle theta: sweeps around the equator 0 to 2*PI
            float const theta = 2.0f * PI * s * S;

            float const x = sinPhi * cos(theta);
            float const y = cosPhi;
            float const z = sinPhi * sin(theta);

            SphereVertex v;
            // Unit sphere coordinates (Normal vector)
            v.normal = glm::vec3(x, y, z);

            // Position scaled to unit diameter (Radius = 0.5)
            v.position = v.normal * 0.5f;
            v.texCoords = glm::vec2(s * S, r * R);
            vertices.push_back(v);

            if (r < rings - 1 && s < sectors - 1) {
                unsigned int cur = r * sectors + s;
                unsigned int next = (r + 1) * sectors + s;

                indices.push_back(cur);
                indices.push_back(cur + 1);
                indices.push_back(next);

                indices.push_back(cur + 1);
                indices.push_back(next + 1);
                indices.push_back(next);
            }
        }
    }

    global_sphere_index_count = indices.size();

    glGenVertexArrays(1, &global_sphere_vao);
    glGenBuffers(1, &global_sphere_vbo);
    glGenBuffers(1, &global_sphere_ebo);

    glBindVertexArray(global_sphere_vao);

    glBindBuffer(GL_ARRAY_BUFFER, global_sphere_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(SphereVertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, global_sphere_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Location 0: Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SphereVertex), (void*)offsetof(SphereVertex, position));

    // Location 1: Normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SphereVertex), (void*)offsetof(SphereVertex, normal));

    // Location 2: TexCoords
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SphereVertex), (void*)offsetof(SphereVertex, texCoords));

    glGenBuffers(1, &global_sphere_instance_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, global_sphere_instance_vbo);

    // Allocate GPU space for up to 1,000 mat4 models (re-allocates automatically if exceeded)
    glBufferData(GL_ARRAY_BUFFER, 1000 * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);

    std::size_t vec4Size = sizeof(glm::vec4);
    for (int i = 0; i < 4; i++) {
        GLuint location = 3 + i;
        glEnableVertexAttribArray(location);
        glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * vec4Size));

        // This line tells OpenGL to advance this attribute once per INSTANCE, not per vertex!
        glVertexAttribDivisor(location, 1);
    }
    // =========================================================================

    glBindVertexArray(0);
}

void InitCubeMesh() {
    glGenVertexArrays(1, &global_cube_vao);
    glGenBuffers(1, &global_cube_vbo);

    global_cube_vertices = {
        // Front face
        {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},

        // Back face
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},

        // Top face
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f,  0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f,  0.0f}, {0.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f,  0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f,  0.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f,  0.0f}, {0.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f,  0.0f}, {0.0f, 0.0f}},

        // Bottom face
        {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},

        // Right face
        {{ 0.5f, -0.5f, -0.5f}, {1.0f,  0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {1.0f,  0.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f,  0.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f,  0.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {1.0f,  0.0f, 0.0f}, {0.0f, 1.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {1.0f,  0.0f, 0.0f}, {0.0f, 0.0f}},

        // Left face
        {{-0.5f,  0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}}
    };

    glBindVertexArray(global_cube_vao);
    glBindBuffer(GL_ARRAY_BUFFER, global_cube_vbo);
    glBufferData(GL_ARRAY_BUFFER, global_cube_vertices.size() * sizeof(Vertex), global_cube_vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    CheckGLErrors();
}

void InitWireframeCubeMesh() {
    glGenVertexArrays(1, &global_wireframe_cube_vao);
    glGenBuffers(1, &global_wireframe_cube_vbo);
    glGenBuffers(1, &global_wireframe_cube_ebo);

    global_wireframe_cube_vertices = {
        // Front face
        { -0.5f, -0.5f,  0.5f },
        {  0.5f, -0.5f,  0.5f },
        {  0.5f,  0.5f,  0.5f },
        { -0.5f,  0.5f,  0.5f },
        // Back face
        { -0.5f, -0.5f, -0.5f },
        {  0.5f, -0.5f, -0.5f },
        {  0.5f,  0.5f, -0.5f },
        { -0.5f,  0.5f, -0.5f }
    };

    global_wireframe_cube_indices = {
        // Front face
        0, 1, 1, 2, 2, 3, 3, 0,
        // Back face
        4, 5, 5, 6, 6, 7, 7, 4,
        // Connecting edges
        0, 4, 1, 5, 2, 6, 3, 7
    };

    glBindVertexArray(global_wireframe_cube_vao);

    glBindBuffer(GL_ARRAY_BUFFER, global_wireframe_cube_vbo);
    glBufferData(GL_ARRAY_BUFFER, global_wireframe_cube_vertices.size() * sizeof(glm::vec3), global_wireframe_cube_vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, global_wireframe_cube_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, global_wireframe_cube_indices.size() * sizeof(GLuint), global_wireframe_cube_indices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    CheckGLErrors();
}

void InitSkyboxCubeMesh() {
    glGenVertexArrays(1, &global_skybox_cube_vao);
    glGenBuffers(1, &global_skybox_cube_vbo);
    glGenBuffers(1, &global_skybox_cube_ebo);

    float oneThird = (1.0f / 3.0f);
    float twoThird = (2.0f / 3.0f);
    float texelOffsetX = 0.5f / 1024.0f;
    float texelOffsetY = 0.5f / 768.0f;

    global_skybox_cube_vertices = {
        // Front face
        {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, -1.0f}, {0.25f + texelOffsetX, oneThird + texelOffsetY}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, -1.0f}, {0.25f + texelOffsetX, twoThird - texelOffsetY}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, -1.0f}, {0.50f - texelOffsetX, twoThird - texelOffsetY}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, -1.0f}, {0.50f - texelOffsetX, oneThird + texelOffsetY}},

        // Back face
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f - texelOffsetX, oneThird + texelOffsetY}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.75f + texelOffsetX, oneThird + texelOffsetY}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.75f + texelOffsetX, twoThird - texelOffsetY}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f - texelOffsetX, twoThird - texelOffsetY}},

        // Top face
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {0.50f - texelOffsetX, twoThird - texelOffsetY}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {0.25f + texelOffsetX, twoThird - texelOffsetY}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.25f + texelOffsetX, 1.0f - texelOffsetY}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.50f - texelOffsetX, 1.0f - texelOffsetY}},

        // Bottom face
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.25f + texelOffsetX, 0.0f + texelOffsetY}},
        {{-0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.25f + texelOffsetX, oneThird + texelOffsetY}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.50f - texelOffsetX, oneThird + texelOffsetY}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.50f - texelOffsetX, 0.0f + texelOffsetY}},

        // Right face
        {{ 0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.75f - texelOffsetX, oneThird + texelOffsetY}},
        {{ 0.5f, -0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {0.50f + texelOffsetX, oneThird + texelOffsetY}},
        {{ 0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {0.50f + texelOffsetX, twoThird - texelOffsetY}},
        {{ 0.5f,  0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.75f - texelOffsetX, twoThird - texelOffsetY}},

        // Left face
        {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f + texelOffsetX, oneThird + texelOffsetY}},
        {{-0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f + texelOffsetX, twoThird - texelOffsetY}},
        {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.25f - texelOffsetX, twoThird - texelOffsetY}},
        {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.25f - texelOffsetX, oneThird + texelOffsetY}}
    };

    global_skybox_cube_indices = {
        // Front face
        0, 1, 2,  2, 3, 0,
        // Back face
        4, 5, 6,  6, 7, 4,
        // Top face
        8, 9, 10,  10, 11, 8,
        // Bottom face
        12, 13, 14,  14, 15, 12,
        // Right face
        16, 17, 18,  18, 19, 16,
        // Left face
        20, 21, 22,  22, 23, 20
    };

    glBindVertexArray(global_skybox_cube_vao);

    glBindBuffer(GL_ARRAY_BUFFER, global_skybox_cube_vbo);
    glBufferData(GL_ARRAY_BUFFER, global_skybox_cube_vertices.size() * sizeof(Vertex), global_skybox_cube_vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, global_skybox_cube_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, global_skybox_cube_indices.size() * sizeof(GLuint), global_skybox_cube_indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    CheckGLErrors();
}

void SetupGlobalGLElements() {
    // 2D Primitives
    InitRectangleMesh();
    InitCircleMesh();
    InitLineMesh();

    // 3D Primitives
    InitTriangleMesh();
    InitSphereMesh();
    InitCubeMesh();
    InitWireframeCubeMesh();
    InitSkyboxCubeMesh();
}

void CleanupGlobalGLElements() {
    // 2D elements
    glDeleteVertexArrays(1, &global_rectangle_vao);
    glDeleteBuffers(1, &global_rectangle_vbo);
    glDeleteBuffers(1, &global_rectangle_ebo);

    glDeleteVertexArrays(1, &global_circle_vao);
    glDeleteBuffers(1, &global_circle_vbo);

    glDeleteVertexArrays(1, &global_line_vao);
    glDeleteBuffers(1, &global_line_vbo);

    // 3D elements
    glDeleteVertexArrays(1, &global_cube_vao);
    glDeleteBuffers(1, &global_cube_vbo);

    glDeleteVertexArrays(1, &global_wireframe_cube_vao);
    glDeleteBuffers(1, &global_wireframe_cube_vbo);
    glDeleteBuffers(1, &global_wireframe_cube_ebo);

    glDeleteVertexArrays(1, &global_skybox_cube_vao);
    glDeleteBuffers(1, &global_skybox_cube_vbo);
}

// run every time drawing is started or the active camera is changed
void SetUniforms2D() {
    if (extras::activeCamera2D == nullptr || extras::ActiveWindow == nullptr) {
        std::cerr << "SetUniforms2D() Resources for drawing are not available." << std::endl;
        exit(EXIT_FAILURE);
    }

    CheckGLErrors();

    // Send the uniform data to the active shader
    //glUseProgram(extras::ActiveShader->shader);
    extras::ActiveShader->setUniform("view", extras::activeCamera2D->GetViewMatrix());
    extras::ActiveShader->setUniform("projection", extras::activeCamera2D->GetProjectionMatrix());
    extras::ActiveShader->setUniform("useTexture", 0);
    extras::ActiveShader->setUniform("mixColor", 0);
    //extras::ActiveShader->setUniform("iResolution", glm::vec2(extras::CurrentWindowWidth * extras::activeCamera2D->zoom.x, extras::CurrentWindowHeight * extras::activeCamera2D->zoom.y));
}

// run every time drawing is started or the active camera is changed
void SetUniforms3D() {
    if (extras::activeCamera3D == nullptr || extras::ActiveWindow == nullptr) {
        std::cerr << "SetUniforms2D() Resources for drawing are not available." << std::endl;
        exit(EXIT_FAILURE);
    }

    // Send the uniform data to the shader
    //glUseProgram(extras::ActiveShader->shader);
    extras::ActiveShader->setUniform("model", glm::mat4(1.0f));
    extras::ActiveShader->setUniform("view", extras::activeCamera3D->GetViewMatrix());
    extras::ActiveShader->setUniform("projection", extras::activeCamera3D->GetProjectionMatrix());
    extras::ActiveShader->setUniform("viewPos", extras::activeCamera3D->position);
    extras::ActiveShader->setUniform("useTexture", 0);
}

int calculateCircleSegments(float radius) {
    const float k = 50.0f;  // Coefficient that determines granularity
    const int minSegments = 3;  // Minimum number of segments for small circles
    const int maxSegments = 360;  // Maximum number of segments for very large circles

    // Estimate the number of segments needed for a smooth appearance
    int num_segments = static_cast<int>(k * sqrt(radius));  // Example formula

    // Clamp the number of segments to the minimum and maximum bounds
    return Clamp(num_segments, minSegments, maxSegments);
}


void DrawCircleEx(float centerX, float centerY, float radiusX, float radiusY, glm::vec4 color, int num_segments, bool wires = false) {
    if (num_segments == 0) {
        num_segments = calculateCircleSegments(std::max(radiusX, radiusY));
    }

    std::vector<Vertex> vertices(num_segments);
    float theta = TWO_PI / float(num_segments);
    float cos_theta = cosf(theta);
    float sin_theta = sinf(theta);

    // Starting point on the unit circle (-1 to 1 range)
    float ux = 1.0f;
    float uy = 0.0f;

    for (int i = 0; i < num_segments; ++i) {
        // Position: Actual screen/world coordinates
        vertices[i].position = glm::vec3(ux * radiusX + centerX, uy * radiusY + centerY, 0.0f);

        // Texture Coordinates: Local unit-circle coords (-1 to 1)
        // This is what the fragment shader uses for the glimmer and edge effects
        vertices[i].texCoord = glm::vec2(ux, uy);

        // Zero out unused fields
        vertices[i].normal = glm::vec3(0.0f);
        vertices[i].color = glm::vec3(color.r, color.g, color.b);

        // Rotate for next segment
        float tx = cos_theta * ux - sin_theta * uy;
        float ty = sin_theta * ux + cos_theta * uy;
        ux = tx;
        uy = ty;
    }

    glBindVertexArray(global_circle_vao);
    glBindBuffer(GL_ARRAY_BUFFER, global_circle_vbo);

    // Upload the interleaved data
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    extras::ActiveShader->setUniform("model", glm::mat4(1.0f));
    // Texture drawing leaves these uniforms enabled on the shared 2D shader.
    // Circles must use their solid color so badges cannot inherit terrain texture.
    extras::ActiveShader->setUniform("useTexture", false);
    extras::ActiveShader->setUniform("mixColor", false);
    // Pass the alpha through if your shader uses the 'color' uniform
    extras::ActiveShader->setUniform("color", color);

    if (!wires) {
        glDrawArrays(GL_TRIANGLE_FAN, 0, (GLsizei)vertices.size());
    }
    else {
        glDrawArrays(GL_LINE_LOOP, 0, (GLsizei)num_segments);
    }

    // Cleanup
    glBindBuffer(GL_ARRAY_BUFFER, 0); // This unbinds the VBO but the VAO still remembers it
    glBindVertexArray(0);
}

// Draw a color-filled circle
void DrawCircle(float centerX, float centerY, float radius, glm::vec4 color, int num_segments) {
    DrawCircleEx(centerX, centerY, radius, radius, color, num_segments);
}

// Draw a wireframe circle
void DrawCircleWires(float centerX, float centerY, float radius, glm::vec4 color, int num_segments) {
    DrawCircleEx(centerX, centerY, radius, radius, color, num_segments, true);
}

void DrawRectangleEx(Rect rect, float angle, glm::vec4* color = nullptr, Texture* texture = nullptr, bool wires = false, bool centerOrigin = false, bool mixColor = false, glm::vec4* customRect = nullptr) {
    float posX = rect.position.x;
    float posY = rect.position.y;

    float width = rect.size.x;
    float height = rect.size.y;

    float left = 0.0;
    float right = 1.0;
    float bottom = 0.0;
    float top = 1.0;

    if (customRect != nullptr) {
        if (texture != nullptr) {
            left = customRect->x / float(texture->width);
            right = (customRect->x + customRect->z) / float(texture->width);
            bottom = customRect->y / float(texture->height);
            top = (customRect->y + customRect->w) / float(texture->height);
        }
    }

    float vertices[24];
    if (extras::drawYDown) {
        const float yDownVertices[24] = {
            // Positions           // Texture Coords
            0.0f, 0.0f, 0.0f,      left, top,    // Top-left corner
            0.0f, 1.0f, 0.0f,      left, bottom, // Bottom-left corner
            1.0f, 1.0f, 0.0f,      right, bottom, // Bottom-right corner
            1.0f, 0.0f, 0.0f,      right, top    // Top-right corner
        };
        std::copy_n(yDownVertices, 24, vertices);
    } else {
        const float yUpVertices[24] = {
            // Positions           // Texture Coords
            0.0f, 1.0f, 0.0f,      left, top,    // Top-left corner
            0.0f, 0.0f, 0.0f,      left, bottom, // Bottom-left corner
            1.0f, 0.0f, 0.0f,      right, bottom, // Bottom-right corner
            1.0f, 1.0f, 0.0f,      right, top    // Top-right corner
        };
        std::copy_n(yUpVertices, 24, vertices);
    }

    glBindVertexArray(global_rectangle_vao);
    glBindBuffer(GL_ARRAY_BUFFER, global_rectangle_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    if (extras::optimizeGLCalls == false) {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); // Unlike VBOs, the EBO does not need to be unbound after setting it up because its binding is part of the VAO state.
    }

    if (width < 0.0) {
        posX -= width;
    }
    if (height < 0.0) {
        posY -= height;
    }

    // Set up transformation matrix
    glm::mat4 model = glm::mat4(1.0f);
    if (centerOrigin) {
        glm::vec2 origin = glm::vec2(width / 2.0, height / 2.0);
        model = glm::translate(model, glm::vec3(posX, posY, 0.0f));
        model = glm::translate(model, glm::vec3(origin.x, origin.y, 0.0f));
        model = glm::rotate(model, angle, glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::translate(model, glm::vec3(-origin.x, -origin.y, 0.0f));
        model = glm::scale(model, glm::vec3(width, height, 1.0f));
    }
    else {
        model = glm::translate(model, glm::vec3(posX, posY, 0.0f));
        model = glm::rotate(model, angle, glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, glm::vec3(width, height, 1.0f));
    }

    extras::ActiveShader->setUniform("model", model);
    extras::ActiveShader->setUniform("mixColor", mixColor);

    if (color != nullptr) {
        extras::ActiveShader->setUniform("color", *color);
    }
    else {
        extras::ActiveShader->setUniform("color", WHITE);
    }

    if (texture != nullptr) {
        // Set uniforms
        extras::ActiveShader->setUniform("textureSampler", 0);
        extras::ActiveShader->setUniform("useTexture", 1);

        // Bind the texture
        texture->bind(0);

        // Draw the rectangle
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Cleanup
        texture->unbind();
    }
    else {
        extras::ActiveShader->setUniform("useTexture", 0);

        if (!wires) {
            // Draw the rectangle
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
        else {
            // Draw the rectangle
            glDisable(GL_CULL_FACE);               // Ensure lines render from front & back angles
            glEnable(GL_POLYGON_OFFSET_LINE);     // Prevent Z-fighting depth buffer occlusion
            glPolygonOffset(-1.0f, -1.0f);        // Shift depth slightly toward camera
            glLineWidth(2.0f);                    // Ensure line rasterization width

            glDrawArrays(GL_LINE_LOOP, 0, 4);     // Render Wireframe

            // Restore GL state
            glDisable(GL_POLYGON_OFFSET_LINE);
            glLineWidth(1.0f);
        }
    }

    if (extras::optimizeGLCalls == false) {
        glBindVertexArray(0);
    }

    CheckGLErrors();
}

// Draw a color-filled rectangle
void DrawRectangle(Rect rect, glm::vec4 color) {
    DrawRectangleEx(rect, 0, &color, nullptr, false, false, false, nullptr);
}

// Draw a color-filled rotated rectangle
void DrawRectangle(Rect rect, float angle, glm::vec4 color) {
    DrawRectangleEx(rect, angle, &color, nullptr, false, true, false, nullptr);
}

// Draw a texture-filled rectangle
void DrawRectangle(Rect rect, Texture& texture, glm::vec4* customRect) {
    DrawRectangleEx(rect, 0, nullptr, &texture, false, false, false, customRect);
}

// Draw a texture-filled rorated rectangle
void DrawTexturedQuad(const glm::mat4& model, Texture& texture, glm::vec4 color) {
    // Same unit quad as DrawRectangleEx: (0,0) is the texture's top-left corner.
    float vertices[20];
    if (extras::drawYDown) {
        const float yDownVertices[20] = {
            0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
            0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
            1.0f, 1.0f, 0.0f, 1.0f, 0.0f,
            1.0f, 0.0f, 0.0f, 1.0f, 1.0f
        };
        std::copy_n(yDownVertices, 20, vertices);
    } else {
        const float yUpVertices[20] = {
            0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
            1.0f, 1.0f, 0.0f, 1.0f, 1.0f
        };
        std::copy_n(yUpVertices, 20, vertices);
    }

    glBindVertexArray(global_rectangle_vao);
    glBindBuffer(GL_ARRAY_BUFFER, global_rectangle_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    if (extras::optimizeGLCalls == false) {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    extras::ActiveShader->setUniform("model", model);
    extras::ActiveShader->setUniform("mixColor", true); // color multiplies the texture
    extras::ActiveShader->setUniform("color", color);
    extras::ActiveShader->setUniform("textureSampler", 0);
    extras::ActiveShader->setUniform("useTexture", 1);
    texture.bind(0);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    texture.unbind();

    if (extras::optimizeGLCalls == false) {
        glBindVertexArray(0);
    }
    CheckGLErrors();
}

void DrawRectangle(Rect rect, float angle, Texture& texture, glm::vec4* customRect) {
    DrawRectangleEx(rect, angle, nullptr, &texture, false, true, false, customRect);
}

// Draw a tinted texture-filled rectangle
void DrawRectangle(Rect rect, glm::vec4 color, Texture& texture, glm::vec4* customRect) {
    DrawRectangleEx(rect, 0, &color, &texture, false, false, true, customRect);
}

// Draw a tinted texture-filled rotated rectangle
void DrawRectangle(Rect rect, float angle, glm::vec4 color, Texture& texture, glm::vec4* customRect) {
    DrawRectangleEx(rect, angle, &color, &texture, false, true, false, customRect);
}

// Draw a tinted texture-filled rotated rectangle
void DrawRectangle(Rect rect, float angle, glm::vec4 color, Texture& texture, bool mix, glm::vec4* customRect) {
    DrawRectangleEx(rect, angle, &color, &texture, false, true, mix, customRect);
}

// Draw a wireframe rectangle
void DrawRectangleWires(Rect rect, glm::vec4 color) {
    DrawRectangleEx(rect, 0, &color, nullptr, true, false, false, nullptr);
}


// Draw the triangle
void DrawTriangleEx(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
    glm::vec4* color = nullptr, bool wires = false) {
    // update vertex positions
    global_triangle_vertices[0].position = a;
    global_triangle_vertices[1].position = b;
    global_triangle_vertices[2].position = c;

    // normals/texcoords can be zero
    for (int i = 0; i < 3; i++) {
        global_triangle_vertices[i].normal = glm::vec3(0.0f);
        global_triangle_vertices[i].texCoord = glm::vec2(0.0f);
    }

    // update VBO
    glBindBuffer(GL_ARRAY_BUFFER, global_triangle_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, 3 * sizeof(Vertex), global_triangle_vertices.data());

    glBindVertexArray(global_triangle_vao);

    glm::mat4 model = glm::mat4(1.0f);
    extras::ActiveShader->setUniform("model", model);
    extras::ActiveShader->setUniform("useTexture", 0);
    extras::ActiveShader->setUniform("mixColor", false);
    extras::ActiveShader->setUniform("color", color ? *color : WHITE);

    glDrawArrays(wires ? GL_LINE_LOOP : GL_TRIANGLES, 0, 3);

    if (!extras::optimizeGLCalls)
        glBindVertexArray(0);

    CheckGLErrors();
}

// Convenience function
void DrawTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, glm::vec4 color) {
    DrawTriangleEx(a, b, c, &color, false);
}




// Draw a color-filled line
void DrawLine(float startX, float startY, float endX, float endY, glm::vec4 color) {
    //glDisable(GL_DEPTH_TEST); // Disable depth testing for 2D rendering

    // Line vertices
    std::vector<glm::vec3> vertices = {
        {startX, startY, 0.0f}, // Start point
        {endX, endY, 0.0f}      // End point
    };

    // Bind global VAO and VBO
    glBindVertexArray(global_line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, global_line_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);

    // Send the color uniform data to the shader
    extras::ActiveShader->setUniform("model", glm::mat4(1.0f));
    extras::ActiveShader->setUniform("useTexture", 0); // Disable texture usage in shader
    extras::ActiveShader->setUniform("color", color);

    // Draw the line
    glBindVertexArray(global_line_vao);
    glDrawArrays(GL_LINES, 0, 2);  // Draws 1 line

    // Cleanup
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    //glEnable(GL_DEPTH_TEST); // Re-enable depth testing if needed elsewhere
}

void DrawLine(glm::vec2 start, glm::vec2 end, glm::vec4 color) {
    DrawLine(start.x, start.y, end.x, end.y, color);
}

// Draw a color-filled line with thickness
void DrawLine(float startX, float startY, float endX, float endY, glm::vec4 color, float thickness, float dashLength, float gapLength) {
    struct LineVertex {
        glm::vec3 position;      // Vertex position
        glm::vec2 offsetDir;     // Perpendicular offset direction
        float absoluteDistance;  // Absolute distance along the line
    };

    std::vector<LineVertex> vertices;

    // Calculate direction and perpendicular offset
    glm::vec2 startPoint = { startX, startY };
    glm::vec2 endPoint = { endX, endY };
    glm::vec2 direction = glm::normalize(endPoint - startPoint);
    glm::vec2 perpendicular = glm::vec2(-direction.y, direction.x) * (thickness / 2.0f);
    float segmentLength = glm::length(endPoint - startPoint);

    // Create vertices for the line
    vertices.push_back({ glm::vec3(startPoint, 0.0f), perpendicular, 0.0f });
    vertices.push_back({ glm::vec3(startPoint, 0.0f), -perpendicular, 0.0f });
    vertices.push_back({ glm::vec3(endPoint, 0.0f), perpendicular, segmentLength });
    vertices.push_back({ glm::vec3(endPoint, 0.0f), -perpendicular, segmentLength });

    // Activate the shader
    BeginShaderMode(extras::DefaultLineShader);
    SetUniforms2D();

    // Set shader uniforms for thickness, color, and dash/gap
    extras::ActiveShader->setUniform("thickness", thickness / 2.0f);
    extras::ActiveShader->setUniform("lineColor", color);
    extras::ActiveShader->setUniform("dashLength", dashLength);
    extras::ActiveShader->setUniform("gapLength", gapLength);

    // Upload data to GPU
    glBindVertexArray(global_line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, global_line_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(LineVertex), vertices.data(), GL_STATIC_DRAW);

    // Configure vertex attributes
    glEnableVertexAttribArray(1); // Position
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)0);

    glEnableVertexAttribArray(2); // Offset direction
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)offsetof(LineVertex, offsetDir));

    glEnableVertexAttribArray(3); // Absolute distance
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)offsetof(LineVertex, absoluteDistance));

    // Draw the line as GL_TRIANGLE_STRIP
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Cleanup
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    EndShaderMode();
}

// Batch draw many color-filled lines
void BatchDrawLines(std::vector<glm::vec2> points, glm::vec4 color, float thickness, float dashLength, float gapLength) {
    if (points.size() < 2 || thickness <= 0.0f) return;

    struct LineVertex {
        glm::vec3 position;
        glm::vec2 offsetDir;
        float absoluteDistance;
    };

    const size_t segmentCount = points.size() - 1;
    std::vector<glm::vec2> normals(segmentCount);
    std::vector<float> distances(points.size(), 0.0f);
    for (size_t i = 0; i < segmentCount; ++i) {
        const glm::vec2 delta = points[i + 1] - points[i];
        const float length = glm::length(delta);
        if (length <= 0.0001f) return;
        const glm::vec2 direction = delta / length;
        normals[i] = glm::vec2(-direction.y, direction.x);
        distances[i + 1] = distances[i] + length;
    }

    const bool closed = glm::length(points.front() - points.back()) <= 0.0001f;
    std::vector<LineVertex> vertices;
    vertices.reserve(points.size() * 2);
    for (size_t i = 0; i < points.size(); ++i) {
        glm::vec2 offset = normals[i == 0 ? 0 : i - 1];
        if (i > 0 && i < segmentCount) {
            const glm::vec2 nextNormal = normals[i];
            const glm::vec2 normalSum = offset + nextNormal;
            if (glm::length(normalSum) > 0.0001f) {
                const glm::vec2 miter = glm::normalize(normalSum);
                const float denominator = glm::dot(miter, nextNormal);
                const float miterScale = std::abs(denominator) > 0.0001f
                    ? std::clamp(1.0f / denominator, -4.0f, 4.0f)
                    : 1.0f;
                offset = miter * miterScale;
            }
        } else if (closed) {
            const glm::vec2 previousNormal = normals[segmentCount - 1];
            const glm::vec2 nextNormal = normals[0];
            const glm::vec2 normalSum = previousNormal + nextNormal;
            if (glm::length(normalSum) > 0.0001f) {
                const glm::vec2 miter = glm::normalize(normalSum);
                const float denominator = glm::dot(miter, nextNormal);
                const float miterScale = std::abs(denominator) > 0.0001f
                    ? std::clamp(1.0f / denominator, -4.0f, 4.0f)
                    : 1.0f;
                offset = miter * miterScale;
            }
        }

        vertices.push_back({ glm::vec3(points[i], 0.0f), offset, distances[i] });
        vertices.push_back({ glm::vec3(points[i], 0.0f), -offset, distances[i] });
    }

    BeginShaderMode(extras::DefaultLineShader);
    SetUniforms2D();
    glBindVertexArray(global_line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, global_line_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(LineVertex), vertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)0);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)offsetof(LineVertex, offsetDir));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)offsetof(LineVertex, absoluteDistance));

    extras::ActiveShader->setUniform("thickness", thickness / 2.0f);
    extras::ActiveShader->setUniform("lineColor", color);
    extras::ActiveShader->setUniform("dashLength", dashLength);
    extras::ActiveShader->setUniform("gapLength", gapLength);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, static_cast<GLsizei>(vertices.size()));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    EndShaderMode();
}
void DrawCubeEx(glm::vec3 position, glm::vec3 size, glm::vec3 rotation, glm::vec4* color = nullptr, Texture* texture = nullptr, glm::vec4* customRect = nullptr, bool wires = false) {
    // Combine translation, rotation, and scaling into a single model matrix
    glm::mat4 model = glm::mat4(1.0f); // Start with the identity matrix
    model = glm::translate(model, position);  // Apply translation
    model = glm::scale(model, size);  // Apply scale
    //model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f)); // Rotate around x-axis
    //model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f)); // Rotate around y-axis
    //model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f)); // Rotate around z-axis

    // Set uniform values
    extras::ActiveShader->setUniform("model", model);

    if (color != nullptr) {
        extras::ActiveShader->setUniform("color", *color);
    }
    else {
        extras::ActiveShader->setUniform("color", WHITE);
    }

    if (texture != nullptr) {
        // Set uniforms
        extras::ActiveShader->setUniform("textureSampler", *texture); // Bind the texture
        extras::ActiveShader->setUniform("useTexture", 1);

        // Bind vertex array and Draw the cube
        bindVAO(global_cube_vao);
        //glBindVertexArray(global_cube_vao);
        //glBindBuffer(GL_ARRAY_BUFFER, global_cube_vbo);
        glDrawArrays(GL_TRIANGLES, 0, global_cube_vertices.size());
    }
    else {
        extras::ActiveShader->setUniform("useTexture", 0); // Disable texture usage in shader

        if (wires) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // Set to draw wireframe

            bindVAO(global_wireframe_cube_vao);
            //glBindVertexArray(global_wireframe_cube_vao);
            glDrawElements(GL_LINES, global_wireframe_cube_indices.size(), GL_UNSIGNED_INT, 0);

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // Reset to fill mode
        }
        else {
            // Bind vertex array and Draw the cube
            bindVAO(global_cube_vao);
            //glBindVertexArray(global_cube_vao);
            //glBindBuffer(GL_ARRAY_BUFFER, global_cube_vbo);
            glDrawArrays(GL_TRIANGLES, 0, global_cube_vertices.size());
        }
    }

    if (extras::optimizeGLCalls == false) {
        //glBindBuffer(GL_ARRAY_BUFFER, 0); // Unbind the VBO
        glBindVertexArray(0); // Unbind the VAO

        //glActiveTexture(GL_TEXTURE0);
        //glBindTexture(GL_TEXTURE_2D, 0);
    }

    CheckGLErrors();
}

// Draw a plane XZ
void DrawPlane(glm::vec3 centerPos, glm::vec2 size, glm::vec4 color) {
    DrawCubeEx(centerPos, glm::vec3(size.x, 0.01, size.y), { 0, 0, 0 }, &color);
}

// Draw a Textured plane XZ
void DrawPlane(glm::vec3 centerPos, glm::vec2 size, Texture& texture, glm::vec4* customRect) {
    DrawCubeEx(centerPos, glm::vec3(size.x, 0.01, size.y), { 0, 0, 0 }, nullptr, &texture, customRect);
}

// Draw a plane XZ wires
void DrawPlaneWires(glm::vec3 centerPos, glm::vec2 size, glm::vec4 color) {
    DrawCubeEx(centerPos, glm::vec3(size.x, 0.01, size.y), { 0, 0, 0 }, &color, nullptr, nullptr, true);
}

// Draw cube
void DrawCube(glm::vec3 position, glm::vec3 size, glm::vec4 color) {
    DrawCubeEx(position, size, { 0, 0, 0 }, &color);
}

// Draw Textured Cube
void DrawCube(glm::vec3 position, glm::vec3 size, Texture& texture, glm::vec4* customRect) {
    DrawCubeEx(position, size, { 0, 0, 0 }, nullptr, &texture, customRect);
}

// Draw cube wires
void DrawCubeWires(glm::vec3 position, glm::vec3 size, glm::vec4 color) {
    DrawCubeEx(position, size, { 0, 0, 0 }, &color, nullptr, nullptr, true);
}

void DrawSphereEx(glm::vec3 position, glm::vec3 radius, glm::vec3 rotation, float seed = 0.0f, glm::vec4* color = nullptr, Texture* texture = nullptr) {
    // 1. Build Model Matrix (Translation * Rotation * Scale)
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, radius);

    // 2. Pass Model Matrix and Seed to the Shader
    extras::ActiveShader->setUniform("model", model);

    // 3. Set Color Uniform
    if (color != nullptr) {
        extras::ActiveShader->setUniform("color", *color);
    }
    else {
        extras::ActiveShader->setUniform("color", glm::vec4(0.55f, 0.50f, 0.48f, 1.0f));
    }

    // 4. Set Texture Uniforms
    if (texture != nullptr) {
        extras::ActiveShader->setUniform("textureSampler", *texture);
        extras::ActiveShader->setUniform("useTexture", 1);
    }
    else {
        extras::ActiveShader->setUniform("useTexture", 0);
    }

    // 5. Draw the Sphere Indexed Mesh
    bindVAO(global_sphere_vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(global_sphere_index_count), GL_UNSIGNED_INT, 0);

    if (!extras::optimizeGLCalls) {
        glBindVertexArray(0);
    }

    CheckGLErrors();
}

void DrawSphereOutline(glm::vec3 position, glm::vec3 radius, glm::vec3 rotation, float outlineThickness) {
    // Enable front-face culling so we only render the expanded back faces
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    // Build Model Matrix scaled up slightly
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, radius + glm::vec3(outlineThickness)); // Scale outwards

    extras::ActiveShader->setUniform("model", model);
    extras::ActiveShader->setUniform("color", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)); // Pure Black Ink
    extras::ActiveShader->setUniform("useTexture", 0);

    bindVAO(global_sphere_vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(global_sphere_index_count), GL_UNSIGNED_INT, 0);

    // Reset Culling State to normal back-face culling
    glCullFace(GL_BACK);
}

// Quick helper overload
void DrawSphere(glm::vec3 position, float radius, glm::vec3 rotation, glm::vec4 color, float seed) {
    DrawSphereEx(position, glm::vec3(radius), rotation, seed, &color);
}

void DrawSkybox(FirstPersonController& fpc, Texture& cubemapTexture) {
    BeginShaderMode(extras::DefaultSkyboxShader);

    BeginDepthMode(DepthModes::GEQUAL); // Set depth function for skybox rendering

    glDepthMask(GL_FALSE);    // don't write depth

    // Remove translation from the view matrix
    glm::mat4 view = glm::mat4(glm::mat3(fpc.camera.GetViewMatrix()));
    glm::mat4 projection = fpc.camera.GetProjectionMatrix();

    // Set uniforms
    extras::ActiveShader->setUniform("view", view);
    extras::ActiveShader->setUniform("projection", projection);

    // Bind VAO and cubemap texture
    glBindVertexArray(global_skybox_cube_vao);
    cubemapTexture.bind(0);
    //glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture.textureID);

    // Draw the skyboxgl
    glDrawElements(GL_TRIANGLES, global_skybox_cube_indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    EndDepthMode(); // Reset depth function to default

    CheckGLErrors(); // Check for any OpenGL errors

    EndShaderMode();
}

void DrawTriangle() {

}



namespace {
// FreeType library shared by every font's bitmap face (msdfgen keeps its own).
FT_Library BitmapFreetype() {
    static FT_Library library = nullptr;
    static bool initialised = false;
    if (!initialised) {
        initialised = true;
        if (FT_Init_FreeType(&library) != 0) {
            std::cerr << "ERROR::FREETYPE: Failed to initialize bitmap text\n";
            library = nullptr;
        }
    }
    return library;
}

constexpr int BitmapAtlasSize = 1024;
}

Font::Font(std::string path)
    : glyphTexture(1024, 1024, GL_FilterModes::LINEAR, GL_WrapModes::CLAMP_TO_EDGE, GL_InternalFormats::RGBA8, GL_ExternalFormats::RGBA, GL_DataFormats::UNSIGNED_BYTE, 4),
      bitmapAtlas(BitmapAtlasSize, BitmapAtlasSize, GL_FilterModes::LINEAR, GL_WrapModes::CLAMP_TO_EDGE, GL_InternalFormats::RGBA8, GL_ExternalFormats::RGBA, GL_DataFormats::UNSIGNED_BYTE, 4)
{
    bitmapAtlas.clear();
    loadFromFile(path);
}

Font::Font(GL_FilterModes mode)
    : glyphTexture(1024, 1024, mode, GL_WrapModes::CLAMP_TO_EDGE, GL_InternalFormats::RGBA8, GL_ExternalFormats::RGBA, GL_DataFormats::UNSIGNED_BYTE, 4),
      bitmapAtlas(BitmapAtlasSize, BitmapAtlasSize, GL_FilterModes::LINEAR, GL_WrapModes::CLAMP_TO_EDGE, GL_InternalFormats::RGBA8, GL_ExternalFormats::RGBA, GL_DataFormats::UNSIGNED_BYTE, 4)
{
    bitmapAtlas.clear();
}

Font::~Font()
{
    if (bitmapFace) { FT_Done_Face(bitmapFace); bitmapFace = nullptr; }
    if (font) { msdfgen::destroyFont(font); font = nullptr; }
    if (ft) {  msdfgen::deinitializeFreetype(ft); ft = nullptr; }
}

bool Font::loadFromFile(const std::filesystem::path& filename, int _baseGlyphSize)
{
    baseGlyphSize = _baseGlyphSize;

    // Reloading replaces both glyph caches.
    if (bitmapFace) { FT_Done_Face(bitmapFace); bitmapFace = nullptr; }
    if (font) { msdfgen::destroyFont(font); font = nullptr; }
    if (ft) { msdfgen::deinitializeFreetype(ft); ft = nullptr; }
    characters.clear();
    clearBitmaps();
    path = filename;

    ft = msdfgen::initializeFreetype();
    if (!ft) {
        std::cerr << "ERROR::MSDFGEN: Failed to initialize Freetype\n";
        return false;
    }

    font = msdfgen::loadFont(ft, filename.string().c_str());
    if (!font) {
        std::cerr << "ERROR::MSDFGEN: Failed to load font\n";
        return false;
    }

    if (!msdfgen::getFontMetrics(metrics, font)) {
        std::cerr << "ERROR::MSDFGEN: Failed to get font metrics\n";
        return false;
    }

    double invEm = 1.0 / metrics.emSize;

    metrics.ascenderY *= invEm;
    metrics.descenderY *= invEm;
    metrics.lineHeight *= invEm;

    // Now EM = 1.0
    metrics.emSize = 1.0;

    return true;
}

bool Font::loadGlyph(char32_t codepoint) {
    if (!font) return false;

    msdfgen::Shape shape;
    double advance;

    // Load glyph in EM space
    if (!msdfgen::loadGlyph(shape, font, codepoint, msdfgen::FONT_SCALING_EM_NORMALIZED, &advance))
        return false;

    msdfgen::resolveShapeGeometry(shape);

    // Handle empty glyphs (like space)
    if (shape.contours.empty()) {
        Glyph glyph = {};
        glyph.Advance = (float)advance; // EM SPACE
        characters[codepoint] = glyph;
        return true;
    }

    msdfgen::edgeColoringInkTrap(shape, 3.0, 0);

    msdfgen::Range range(emRangeLow, emRangeHigh);

    // --- Atlas resolution still uses baseGlyphSize ---
    double scale = (double)baseGlyphSize;

    msdfgen::Shape::Bounds bounds = shape.getBounds();

    double extraPadding = std::max(std::abs(emRangeLow), std::abs(emRangeHigh));
    double l = bounds.l - extraPadding;
    double b = bounds.b - extraPadding;
    double r = bounds.r + extraPadding;
    double t = bounds.t + extraPadding;

    int width = (int)std::ceil((r - l) * scale);
    int height = (int)std::ceil((t - b) * scale);

    msdfgen::Projection projection(
        msdfgen::Vector2(scale, scale),
        msdfgen::Vector2(-l, -b)
    );

    msdfgen::Bitmap<float, 4> msdf(width, height);
    msdfgen::MSDFGeneratorConfig config;
    msdfgen::generateMTSDF(msdf, shape, projection, range, config);

    // Convert to bytes
    std::vector<unsigned char> pixels(width * height * 4);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            auto p = msdf(x, y);
            int idx = 4 * (y * width + x);

            pixels[idx + 0] = msdfgen::pixelFloatToByte(p[0]);
            pixels[idx + 1] = msdfgen::pixelFloatToByte(p[1]);
            pixels[idx + 2] = msdfgen::pixelFloatToByte(p[2]);
            pixels[idx + 3] = msdfgen::pixelFloatToByte(p[3]);
        }
    }

    // Atlas packing
    if (offsetX + width >= glyphTexture.width) {
        offsetX = 0;
        offsetY += rowHeight + padding;
        rowHeight = 0;
    }

    if (offsetY + height >= glyphTexture.height)
        return false;

    glBindTexture(GL_TEXTURE_2D, glyphTexture.textureID);
    glTexSubImage2D(GL_TEXTURE_2D, 0, offsetX, offsetY, width, height,
        GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    // --- STORE METRICS IN EM SPACE ---
    Glyph glyph;
    glyph.Size = { (float)width, (float)height }; // pixels
    glyph.Advance = (float)advance;               // EM

    glyph.Bearing.x = (float)l;                  // EM
    glyph.Bearing.y = (float)t;                  // EM

    glyph.TexCoordStart = {
        (float)offsetX / glyphTexture.width,
        (float)offsetY / glyphTexture.height
    };
    glyph.TexCoordEnd = {
        (float)(offsetX + width) / glyphTexture.width,
        (float)(offsetY + height) / glyphTexture.height
    };

    characters[codepoint] = glyph;

    offsetX += width + padding;
    rowHeight = std::max(rowHeight, height);

    return true;
}

// Preload all glyphs for a given size
bool Font::loadGlyphs(const std::string& charset) {
    clear(); // Reset atlas and offsets

    for (unsigned char c : charset) {
        if (!loadGlyph(static_cast<char32_t>(c))) {
            return false; // Stop if texture runs out of space
        }
    }

    return true;
}

void Font::clear() {
    characters.clear();
    glyphTexture.clear();
}

bool Font::openBitmapFace() {
    if (bitmapFace) return true;
    FT_Library library = BitmapFreetype();
    if (!library || path.empty()) return false;
    FT_Face face = nullptr;
    if (FT_New_Face(library, path.string().c_str(), 0, &face) != 0) {
        std::cerr << "ERROR::FREETYPE: Failed to load " << path.string() << '\n';
        return false;
    }
    FT_Select_Charmap(face, FT_ENCODING_UNICODE);
    bitmapFace = face;
    return true;
}

BitmapStrike* Font::getStrike(int pixelSize, int weight) {
    if (!openBitmapFace()) return nullptr;
    pixelSize = std::clamp(pixelSize, 1, 512);
    weight = std::clamp(weight, 100, 900);
    const int key = pixelSize * 1000 + weight;
    auto found = strikes.find(key);
    if (found != strikes.end()) return &found->second;

    if (FT_Set_Pixel_Sizes(bitmapFace, 0, static_cast<FT_UInt>(pixelSize)) != 0) return nullptr;
    // Size metrics are 26.6 fixed point, already rounded to whole pixels when hinting.
    const FT_Size_Metrics& sizeMetrics = bitmapFace->size->metrics;
    BitmapStrike strike;
    strike.ascender = std::ceil(sizeMetrics.ascender / 64.0f);
    strike.descender = std::floor(sizeMetrics.descender / 64.0f);
    strike.lineHeight = std::round(sizeMetrics.height / 64.0f);
    return &strikes.emplace(key, std::move(strike)).first->second;
}

const BitmapGlyph* Font::getBitmapGlyph(char32_t codepoint, int pixelSize, int weight) {
    BitmapStrike* strike = getStrike(pixelSize, weight);
    if (!strike) return nullptr;
    auto found = strike->glyphs.find(codepoint);
    if (found != strike->glyphs.end()) return &found->second;

    pixelSize = std::clamp(pixelSize, 1, 512);
    weight = std::clamp(weight, 100, 900);
    FT_Set_Pixel_Sizes(bitmapFace, 0, static_cast<FT_UInt>(pixelSize));
    FT_UInt index = FT_Get_Char_Index(bitmapFace, codepoint);
    if (index == 0 && codepoint != U' ')
        index = FT_Get_Char_Index(bitmapFace, U'?');

    // FULL uses the classic (v35) TrueType interpreter, which grid-fits stems in
    // both directions like Windows GDI; the others use the modern v40 one.
    FT_UInt interpreter = hinting == FontHinting::FULL ? TT_INTERPRETER_VERSION_35 : TT_INTERPRETER_VERSION_40;
    FT_Property_Set(BitmapFreetype(), "truetype", "interpreter-version", &interpreter);

    FT_Int32 flags = FT_LOAD_DEFAULT;
    if (hinting == FontHinting::NONE) flags |= FT_LOAD_NO_HINTING;
    else if (hinting == FontHinting::LIGHT) flags |= FT_LOAD_TARGET_LIGHT;
    else flags |= FT_LOAD_TARGET_NORMAL;
    if (FT_Load_Glyph(bitmapFace, index, flags) != 0) return nullptr;

    FT_GlyphSlot slot = bitmapFace->glyph;
    FT_Pos embolden = 0;
    if (weight != 400 && slot->format == FT_GLYPH_FORMAT_OUTLINE) {
        // CSS-style weights: 700 strokes about 1/24 em more than 400.
        embolden = static_cast<FT_Pos>((weight - 400) / 300.0f * pixelSize * 64.0f / 24.0f);
        FT_Outline_Embolden(&slot->outline, embolden);
    }
    if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL) != 0) return nullptr;
    const FT_Bitmap& bitmap = slot->bitmap;

    BitmapGlyph glyph;
    glyph.Advance = (slot->advance.x + embolden) / 64.0f;
    glyph.Size = { static_cast<float>(bitmap.width), static_cast<float>(bitmap.rows) };
    glyph.Bearing = { static_cast<float>(slot->bitmap_left), static_cast<float>(slot->bitmap_top) };

    const int width = static_cast<int>(bitmap.width);
    const int height = static_cast<int>(bitmap.rows);
    if (width > 0 && height > 0) {
        const int gap = 1; // Keeps linear filtering from bleeding between glyphs
        if (width + gap > bitmapAtlas.width || height + gap > bitmapAtlas.height) return nullptr;
        if (bitmapOffsetX + width + gap > bitmapAtlas.width) {
            bitmapOffsetX = 0;
            bitmapOffsetY += bitmapRowHeight + gap;
            bitmapRowHeight = 0;
        }
        if (bitmapOffsetY + height + gap > bitmapAtlas.height) {
            // Full: start a fresh atlas. Glyphs are re-rasterised on demand.
            clearBitmaps();
            return getBitmapGlyph(codepoint, pixelSize, weight);
        }

        std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4, 255);
        for (int y = 0; y < height; ++y) {
            const unsigned char* row = bitmap.buffer + static_cast<ptrdiff_t>(y) * bitmap.pitch;
            for (int x = 0; x < width; ++x) {
                float coverage = row[x] / 255.0f;
                if (coverageGamma != 1.0f)
                    coverage = std::pow(coverage, 1.0f / coverageGamma);
                pixels[(static_cast<size_t>(y) * width + x) * 4 + 3] =
                    static_cast<unsigned char>(std::lround(coverage * 255.0f));
            }
        }
        glBindTexture(GL_TEXTURE_2D, bitmapAtlas.textureID);
        glTexSubImage2D(GL_TEXTURE_2D, 0, bitmapOffsetX, bitmapOffsetY, width, height,
            GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

        // Bitmap rows run top-down, so TexCoordStart.y is the glyph's top edge.
        glyph.TexCoordStart = {
            static_cast<float>(bitmapOffsetX) / bitmapAtlas.width,
            static_cast<float>(bitmapOffsetY) / bitmapAtlas.height
        };
        glyph.TexCoordEnd = {
            static_cast<float>(bitmapOffsetX + width) / bitmapAtlas.width,
            static_cast<float>(bitmapOffsetY + height) / bitmapAtlas.height
        };
        bitmapOffsetX += width + gap;
        bitmapRowHeight = std::max(bitmapRowHeight, height);
    }
    return &strike->glyphs.emplace(codepoint, glyph).first->second;
}

float Font::getKerning(char32_t left, char32_t right) {
    if (!openBitmapFace() || !FT_HAS_KERNING(bitmapFace)) return 0.0f;
    FT_Vector delta{};
    if (FT_Get_Kerning(bitmapFace, FT_Get_Char_Index(bitmapFace, left),
            FT_Get_Char_Index(bitmapFace, right), FT_KERNING_UNSCALED, &delta) != 0)
        return 0.0f;
    return static_cast<float>(delta.x) / bitmapFace->units_per_EM;
}

float Font::getBitmapKerning(char32_t left, char32_t right, int pixelSize) {
    if (!openBitmapFace() || !FT_HAS_KERNING(bitmapFace)) return 0.0f;
    FT_Set_Pixel_Sizes(bitmapFace, 0, static_cast<FT_UInt>(std::clamp(pixelSize, 1, 512)));
    FT_Vector delta{};
    if (FT_Get_Kerning(bitmapFace, FT_Get_Char_Index(bitmapFace, left),
            FT_Get_Char_Index(bitmapFace, right), FT_KERNING_DEFAULT, &delta) != 0)
        return 0.0f;
    return delta.x / 64.0f;
}

void Font::setHinting(FontHinting mode) {
    if (hinting == mode) return;
    hinting = mode;
    clearBitmaps();
}

void Font::setCoverageGamma(float gamma) {
    gamma = std::clamp(gamma, 0.25f, 4.0f);
    if (coverageGamma == gamma) return;
    coverageGamma = gamma;
    clearBitmaps();
}

void Font::clearBitmaps() {
    strikes.clear();
    bitmapOffsetX = bitmapOffsetY = bitmapRowHeight = 0;
    if (bitmapAtlas.textureID) bitmapAtlas.clear();
}



bool NextUtf8Codepoint(const std::string& text, size_t& byteIndex, char32_t& codepoint) {
    if (byteIndex >= text.size()) return false;

    const auto byteAt = [&](size_t index) {
        return static_cast<unsigned char>(text[index]);
    };
    const unsigned char lead = byteAt(byteIndex);
    if (lead < 0x80) {
        codepoint = lead;
        ++byteIndex;
        return true;
    }

    int continuationCount = 0;
    char32_t decoded = 0;
    char32_t minimumValue = 0;
    if ((lead & 0xE0) == 0xC0) {
        continuationCount = 1;
        decoded = lead & 0x1F;
        minimumValue = 0x80;
    }
    else if ((lead & 0xF0) == 0xE0) {
        continuationCount = 2;
        decoded = lead & 0x0F;
        minimumValue = 0x800;
    }
    else if ((lead & 0xF8) == 0xF0) {
        continuationCount = 3;
        decoded = lead & 0x07;
        minimumValue = 0x10000;
    }
    else {
        codepoint = 0xFFFD;
        ++byteIndex;
        return true;
    }

    if (byteIndex + static_cast<size_t>(continuationCount) >= text.size()) {
        codepoint = 0xFFFD;
        ++byteIndex;
        return true;
    }
    for (int i = 1; i <= continuationCount; ++i) {
        const unsigned char continuation = byteAt(byteIndex + static_cast<size_t>(i));
        if ((continuation & 0xC0) != 0x80) {
            codepoint = 0xFFFD;
            ++byteIndex;
            return true;
        }
        decoded = (decoded << 6) | (continuation & 0x3F);
    }

    if (decoded < minimumValue || decoded > 0x10FFFF ||
        (decoded >= 0xD800 && decoded <= 0xDFFF)) {
        codepoint = 0xFFFD;
        ++byteIndex;
        return true;
    }

    byteIndex += static_cast<size_t>(continuationCount + 1);
    codepoint = decoded;
    return true;
}

const Glyph* GetGlyph(Font* font, char32_t codepoint) {
    if (!font) return nullptr;

    auto it = font->characters.find(codepoint);
    if (it != font->characters.end())
        return &it->second;

    if (font->loadGlyph(codepoint)) {
        it = font->characters.find(codepoint);
        if (it != font->characters.end())
            return &it->second;
    }

    // fallback
    it = font->characters.find(U'?');
    if (it != font->characters.end())
        return &it->second;

    return nullptr;
}



std::vector<unsigned char> GenerateFontGradient() {
    const int width = 40;
    std::vector<unsigned char> pixels(width * 4);

    for (int i = 0; i < width; ++i) {
        glm::vec4 color;

        // --- INSIDE (0 to 19) ---
        if (i < 20) {
            float t = (float)i / 19.0f;

            if (t < 0.75f) { // Slightly longer pink ramp
                float localT = t / 0.75f;
                color = glm::mix(glm::vec4(0.85, 0.55, 0.85, 1.0), glm::vec4(0.95, 0.75, 0.95, 1.0), localT);
            }
            else {
                // Brighter "Hot" white/yellow edge
                float localT = (t - 0.75f) / 0.25f;
                color = glm::mix(glm::vec4(0.95, 0.75, 0.95, 1.0), glm::vec4(1.0, 1.0, 0.85, 1.0), localT);
            }
        }
        // --- THE BLACK BORDER (20) ---
        else if (i == 20) {
            color = glm::vec4(0.0, 0.0, 0.0, 1.0);
        }
        // --- OUTSIDE GLOW (21 to 39) ---
        else {
            float t = (float)(i - 21) / 18.0f; // 0.0 at 21, 1.0 at 39

            // Starts as a vibrant Neon Purple, fades to a deep dark violet/transparent
            glm::vec4 neonPurple(0.6, 0.0, 1.0, 1.0);
            glm::vec4 deepViolet(0.1, 0.0, 0.2, 0.0);

            // The 2nd image has a more "stepped" or "blocked" glow 
            // We use a high power to keep it bright near the edge then drop off
            float alphaCurve = glm::pow(t, 1.5f);
            color = glm::mix(neonPurple, deepViolet, alphaCurve);
        }

        pixels[i * 4 + 0] = (unsigned char)(color.r * 255);
        pixels[i * 4 + 1] = (unsigned char)(color.g * 255);
        pixels[i * 4 + 2] = (unsigned char)(color.b * 255);
        pixels[i * 4 + 3] = (unsigned char)(color.a * 255);
    }
    return pixels;
}

Text::Text() {
    setFont(*extras::defaultFont);
    setCharacterSize(16);
}

Text::Text(Font& _font, std::string _string, float _characterSize) {
    setFont(_font);
    setString(_string);
    setCharacterSize(_characterSize);
}

void Text::setFont(Font& _font) {
    font = &_font;
}

void Text::setString(const std::string& _string) {
    string = _string;
}

void Text::setColor(const glm::vec4& _color) {
    color = _color;
}

void Text::setStrokeWidth(float _strokeWidth) {
    strokeWidth = std::clamp(_strokeWidth, 0.0f, 1.0f);
}

void Text::setFontWeight(int _fontWeight) {
    fontWeight = std::clamp(_fontWeight, 100, 900);
}

void Text::setHorizontalScale(float _horizontalScale) {
    horizontalScale = std::clamp(_horizontalScale, 0.1f, 4.0f);
}

void Text::setRenderMode(TextRenderMode _renderMode) {
    renderMode = _renderMode;
}

void Text::setLetterSpacing(float _letterSpacing) {
    letterSpacing = _letterSpacing;
}

void Text::setLineHeightScale(float _lineHeightScale) {
    lineHeightScale = std::max(_lineHeightScale, 0.0f);
}

void Text::setKerning(bool _kerning) {
    kerning = _kerning;
}

void Text::setCharacterSize(float _characterSize) {
    if (_characterSize != characterSize) {
        characterSize = _characterSize;
        // Preload glyphs at the base size if not done
        if (font && font->characters.empty()) {
            font->loadGlyphs();
        }
    }
}

namespace {
// Device pixels per window unit, so bitmap glyphs stay sharp on scaled displays.
float TextContentScale() {
    if (!extras::ActiveWindow) return 1.0f;
    int framebufferWidth = 0, framebufferHeight = 0, windowWidth = 0, windowHeight = 0;
    glfwGetFramebufferSize(extras::ActiveWindow, &framebufferWidth, &framebufferHeight);
    glfwGetWindowSize(extras::ActiveWindow, &windowWidth, &windowHeight);
    return windowWidth > 0 && framebufferWidth > 0
        ? static_cast<float>(framebufferWidth) / windowWidth : 1.0f;
}

// A glyph quad relative to its line's start and baseline (y grows downwards).
struct TextQuad {
    float x0, y0, x1, y1;
    float u0, vTop, u1, vBottom;
    float penStart, penEnd;
};

struct TextLine {
    size_t firstQuad = 0, endQuad = 0;
    float width = 0.0f;
};

// Shared by measuring and drawing so both always agree. Units are window pixels.
struct TextLayout {
    std::vector<TextQuad> quads;
    std::vector<TextLine> lines;
    float width = 0.0f;
    float ascender = 0.0f;  // Positive, above the baseline
    float descender = 0.0f; // Negative, below the baseline
    float lineStep = 0.0f;  // Baseline to baseline
    bool bitmap = false;
};

TextLayout LayoutText(const Text& text, const std::string& string, bool bitmap, float contentScale) {
    TextLayout layout;
    Font* font = text.font;
    if (!font) return layout;

    const int pixelSize = std::max(1, static_cast<int>(std::lround(text.characterSize * contentScale)));
    BitmapStrike* strike = bitmap ? font->getStrike(pixelSize, text.fontWeight) : nullptr;
    layout.bitmap = bitmap = strike != nullptr;
    const float scale = text.GetScale();

    float lineHeight = 0.0f;
    if (bitmap) {
        layout.ascender = strike->ascender / contentScale;
        layout.descender = strike->descender / contentScale;
        lineHeight = strike->lineHeight / contentScale;
    }
    else {
        layout.ascender = static_cast<float>(font->metrics.ascenderY) * scale;
        layout.descender = static_cast<float>(font->metrics.descenderY) * scale;
        lineHeight = static_cast<float>(font->metrics.lineHeight) * scale;
    }
    layout.lineStep = lineHeight * text.lineHeightScale;

    size_t lineStart = 0;
    while (true) {
        const size_t lineEnd = string.find('\n', lineStart);
        std::string line = string.substr(lineStart, lineEnd == std::string::npos ? std::string::npos : lineEnd - lineStart);
        if (!line.empty() && line.back() == '\r') line.pop_back();

        TextLine laidLine;
        laidLine.firstQuad = layout.quads.size();
        float pen = 0.0f;
        char32_t previous = 0;
        for (size_t byteIndex = 0; byteIndex < line.size();) {
            char32_t codepoint = 0;
            NextUtf8Codepoint(line, byteIndex, codepoint);
            if (previous) {
                pen += text.letterSpacing;
                if (text.kerning)
                    pen += bitmap
                        ? font->getBitmapKerning(previous, codepoint, pixelSize) / contentScale
                        : font->getKerning(previous, codepoint) * scale * text.horizontalScale;
            }

            if (bitmap) {
                const BitmapGlyph* found = font->getBitmapGlyph(codepoint, pixelSize, text.fontWeight);
                if (!found) continue;
                const BitmapGlyph glyph = *found; // The atlas may be rebuilt by the next lookup
                const float advance = glyph.Advance / contentScale * text.horizontalScale;
                if (glyph.Size.x > 0.0f && glyph.Size.y > 0.0f) {
                    // Whole device pixels keep hinted glyphs sharp.
                    const float x0 = (std::round(pen * contentScale) + glyph.Bearing.x * text.horizontalScale) / contentScale;
                    const float y0 = -glyph.Bearing.y / contentScale;
                    layout.quads.push_back({ x0, y0,
                        x0 + glyph.Size.x / contentScale * text.horizontalScale, y0 + glyph.Size.y / contentScale,
                        glyph.TexCoordStart.x, glyph.TexCoordStart.y, glyph.TexCoordEnd.x, glyph.TexCoordEnd.y,
                        pen, pen + advance });
                }
                pen += advance;
            }
            else {
                const Glyph* glyph = GetGlyph(font, codepoint);
                if (!glyph) continue;
                const float advance = glyph->Advance * scale * text.horizontalScale;
                if (glyph->Size.x > 0.0f) {
                    const float w = (glyph->Size.x / font->baseGlyphSize) * scale * text.horizontalScale;
                    const float h = (glyph->Size.y / font->baseGlyphSize) * scale;
                    const float x0 = pen + glyph->Bearing.x * scale * text.horizontalScale;
                    const float y0 = -glyph->Bearing.y * scale;
                    // MSDF atlas rows run bottom-up, so the glyph top uses TexCoordEnd.y.
                    layout.quads.push_back({ x0, y0, x0 + w, y0 + h,
                        glyph->TexCoordStart.x, glyph->TexCoordEnd.y, glyph->TexCoordEnd.x, glyph->TexCoordStart.y,
                        pen, pen + advance });
                }
                pen += advance;
            }
            previous = codepoint;
        }
        laidLine.endQuad = layout.quads.size();
        laidLine.width = pen;
        layout.width = std::max(layout.width, pen);
        layout.lines.push_back(laidLine);

        if (lineEnd == std::string::npos) break;
        lineStart = lineEnd + 1;
    }
    return layout;
}
}

bool Text::UsesBitmap(float rotationRadians) const {
    if (!font) return false;
    if (renderMode == TextRenderMode::BITMAP) return true;
    if (renderMode == TextRenderMode::SDF) return false;
    return rotationRadians == 0.0f && strokeWidth == 0.0f && horizontalScale == 1.0f &&
        characterSize * TextContentScale() <= 64.0f;
}

// Measure text size in pixels
TextMeasurement Text::MeasureText() const {
    return MeasureText(string);
}

TextMeasurement Text::MeasureText(const std::string& _string) const {
    if (!font || _string.empty()) return {};
    const TextLayout layout = LayoutText(*this, _string, UsesBitmap(), TextContentScale());
    TextMeasurement measurement;
    measurement.lineCount = static_cast<int>(layout.lines.size());
    measurement.maxWidth = layout.width;
    measurement.maxAscender = layout.ascender;
    measurement.maxDescender = layout.descender - (measurement.lineCount - 1) * layout.lineStep;
    measurement.totalHeight = measurement.maxAscender - measurement.maxDescender;
    return measurement;
}

float Text::GetCursorX(const std::string& str, int index) const {
    if (!font) return 0.0f;
    const size_t byteLimit = static_cast<size_t>(std::clamp(index, 0, static_cast<int>(str.size())));
    if (byteLimit == 0) return 0.0f;
    const TextLayout layout = LayoutText(*this, str.substr(0, byteLimit), UsesBitmap(), TextContentScale());
    return layout.lines.empty() ? 0.0f : layout.lines.back().width;
}

float Text::GetScale() const {
    return characterSize / font->metrics.emSize;
}

// The gap between one line's descender and the next line's ascender.
float Text::GetLineSpacing() const {
    if (!font) return 0.0f;
    const TextLayout layout = LayoutText(*this, std::string(), UsesBitmap(), TextContentScale());
    return layout.lineStep - (layout.ascender - layout.descender);
}

void Text::Draw(int posX, int posY, TextAlignmentX alignmentX, TextAlignmentY alignmentY, float rotationRadians) const {
    DrawPrecise(static_cast<float>(posX), static_cast<float>(posY), alignmentX, alignmentY, rotationRadians);
}

void Text::DrawPrecise(float posX, float posY, TextAlignmentX alignmentX, TextAlignmentY alignmentY, float rotationRadians) const {
    if (!font || string.empty()) return;

    const float contentScale = TextContentScale();
    const TextLayout layout = LayoutText(*this, string, UsesBitmap(rotationRadians), contentScale);
    if (layout.quads.empty()) return;

    // Vertical alignment uses the font's metrics, so it does not depend on the
    // particular glyphs in the string.
    const float lineCount = static_cast<float>(layout.lines.size());
    const float blockHeight = layout.ascender - layout.descender + (lineCount - 1.0f) * layout.lineStep;
    float firstBaseline = 0.0f; // Below posY, in the direction text flows
    switch (alignmentY) {
    case TextAlignmentY::TOP:      firstBaseline = layout.ascender; break;
    case TextAlignmentY::CENTER:   firstBaseline = layout.ascender - blockHeight * 0.5f; break;
    case TextAlignmentY::BASELINE: firstBaseline = 0.0f; break;
    case TextAlignmentY::BOTTOM:   firstBaseline = layout.descender - (lineCount - 1.0f) * layout.lineStep; break;
    }

    const bool yDown = extras::drawYDown;
    const float down = yDown ? 1.0f : -1.0f;
    const auto snap = [&](float value) {
        return layout.bitmap ? std::round(value * contentScale) / contentScale : value;
    };

    if (layout.bitmap) {
        BeginShaderMode(extras::BitmapTextShader);
        SetUniforms2D();
        BeginBlendingMode(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);
        extras::ActiveShader->setUniform("model", glm::mat4(1.0f));
        extras::ActiveShader->setUniform("atlas", font->bitmapAtlas, 0);
        extras::ActiveShader->setUniform("textColor", color);
    }
    else {
        BeginShaderMode(extras::DefaultTextShader);
        SetUniforms2D();
        BeginBlendingMode(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);

        extras::ActiveShader->setUniform("model", glm::mat4(1.0f));
        extras::ActiveShader->setUniform("MTSDF", font->glyphTexture, 0);

        extras::ActiveShader->setUniform("emRangeLow", float(font->emRangeLow));
        extras::ActiveShader->setUniform("emRangeHigh", float(font->emRangeHigh));
        extras::ActiveShader->setUniform("pxRange", float((font->emRangeHigh - font->emRangeLow) * font->baseGlyphSize));
        extras::ActiveShader->setUniform("strokeWidth", strokeWidth);
        const float weightEmbolden = ((static_cast<float>(fontWeight) - 400.0f) / 300.0f) * 0.025f;
        const float emRangeSpan = std::max(float(font->emRangeHigh - font->emRangeLow), 0.000001f);
        extras::ActiveShader->setUniform("fontWeightOffset", weightEmbolden / emRangeSpan);
        extras::ActiveShader->setUniform("time", float(GetTime()));
        extras::ActiveShader->setUniform("textColor", color);
    }

    bindVAO(global_text_vao);
    glBindBuffer(GL_ARRAY_BUFFER, global_text_vbo);

    // Draw in batches no larger than the shared vertex buffer.
    const size_t floatsPerQuad = 6 * 6;
    const size_t maxFloats = std::max<size_t>(extras::maxTextChars, 1) * floatsPerQuad;
    std::vector<float> vertexBuffer;
    vertexBuffer.reserve(std::min(layout.quads.size() * floatsPerQuad, maxFloats));
    const auto flush = [&]() {
        if (vertexBuffer.empty()) return;
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertexBuffer.size() * sizeof(float), vertexBuffer.data());
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertexBuffer.size() / 6));
        vertexBuffer.clear();
    };

    const float rotationCos = std::cos(rotationRadians);
    const float rotationSin = std::sin(rotationRadians);
    const auto emit = [&](float x, float y, float u, float v, float progress) {
        if (rotationRadians != 0.0f) {
            const float dx = x - posX, dy = y - posY;
            x = posX + rotationCos * dx - rotationSin * dy;
            y = posY + rotationSin * dx + rotationCos * dy;
        }
        vertexBuffer.insert(vertexBuffer.end(), { x, y, 0.0f, u, v, progress });
    };

    for (size_t lineIndex = 0; lineIndex < layout.lines.size(); ++lineIndex) {
        const TextLine& line = layout.lines[lineIndex];
        float lineX = posX;
        if (alignmentX == TextAlignmentX::CENTER) lineX -= line.width * 0.5f;
        else if (alignmentX == TextAlignmentX::RIGHT) lineX -= line.width;
        lineX = snap(lineX);
        const float baseline = snap(posY + down * (firstBaseline + lineIndex * layout.lineStep));

        for (size_t q = line.firstQuad; q < line.endQuad; ++q) {
            const TextQuad& quad = layout.quads[q];
            const float progressLeft = line.width > 0.0f ? glm::clamp(quad.penStart / line.width, 0.0f, 1.0f) : 0.0f;
            const float progressRight = line.width > 0.0f ? glm::clamp(quad.penEnd / line.width, 0.0f, 1.0f) : 0.0f;
            const float left = lineX + quad.x0, right = lineX + quad.x1;
            const float top = baseline + down * quad.y0, bottom = baseline + down * quad.y1;

            if (vertexBuffer.size() + floatsPerQuad > maxFloats) flush();
            emit(left, bottom, quad.u0, quad.vBottom, progressLeft);
            emit(left, top, quad.u0, quad.vTop, progressLeft);
            emit(right, top, quad.u1, quad.vTop, progressRight);
            emit(left, bottom, quad.u0, quad.vBottom, progressLeft);
            emit(right, top, quad.u1, quad.vTop, progressRight);
            emit(right, bottom, quad.u1, quad.vBottom, progressRight);
        }
    }
    flush();

    EndBlendingMode();
    EndShaderMode();
}

void DrawText(const std::string& text, float posX, float posY, float fontSize, glm::vec4 textColor, TextAlignmentX alignmentX, TextAlignmentY alignmentY) {
    DrawText(text, glm::vec2(posX, posY), fontSize, textColor, alignmentX, alignmentY);
}

void DrawText(const std::string& text, glm::vec2 pos, float fontSize, glm::vec4 textColor, TextAlignmentX alignmentX, TextAlignmentY alignmentY) {
    Text* renderer = extras::defaultTextRenderer;
    if (!renderer) return;

    renderer->setString(text);
    renderer->setCharacterSize(fontSize);
    renderer->setColor(textColor);
    renderer->DrawPrecise(pos.x, pos.y, alignmentX, alignmentY);
}

void DrawText(const std::string& text, glm::vec2 pos, float fontSize, glm::vec4 textColor, Font& font, TextAlignmentX alignmentX, TextAlignmentY alignmentY) {
    Text* renderer = extras::defaultTextRenderer;
    if (!renderer) return;

    Font* previous = renderer->font;
    renderer->setFont(font);
    renderer->setString(text);
    renderer->setCharacterSize(fontSize);
    renderer->setColor(textColor);
    renderer->DrawPrecise(pos.x, pos.y, alignmentX, alignmentY);
    renderer->font = previous;
}

void DrawTextWithBackground(std::string text, glm::vec2 pos, float fontSize, glm::vec4 textColor, glm::vec4 backGroundColor, int bgMargin) {
    TextMeasurement textSize = MeasureText(text, fontSize);
    const float top = extras::drawYDown
        ? pos.y - textSize.maxAscender - bgMargin
        : pos.y + textSize.maxDescender - bgMargin;
    DrawRectangle({ glm::vec2(pos.x - bgMargin, top), glm::vec2(textSize.maxWidth + (bgMargin * 2), textSize.totalHeight + (bgMargin * 2)) }, backGroundColor);
    DrawText(text, pos, fontSize, textColor);
}

TextMeasurement MeasureText(std::string text, float fontSize) {
    extras::defaultTextRenderer->setString(text);
    extras::defaultTextRenderer->setCharacterSize(fontSize);
    return extras::defaultTextRenderer->MeasureText();
}

GLuint global_text_vao, global_text_vbo;

void InitOpenGLResources(size_t maxCharacters) {
    extras::maxTextChars = maxCharacters;

    glGenVertexArrays(1, &global_text_vao);
    glGenBuffers(1, &global_text_vbo);

    glBindVertexArray(global_text_vao);
    glBindBuffer(GL_ARRAY_BUFFER, global_text_vbo);

    // Each character uses 6 vertices and 6 floats per vertex (x, y, z, u, v, c)
    size_t totalFloats = maxCharacters * 6 * 6;
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * totalFloats, nullptr, GL_DYNAMIC_DRAW);

    // Position attribute (vec3)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // TexCoord attribute (vec2)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // CharProgress attribute (float)
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}




std::string to_string_rounded(double value, int digits) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(digits) << value;
    return ss.str();
}

// Function to get the angle between two points in radians
float GetAngleBetweenPoints(const glm::vec2& point1, const glm::vec2& point2) {
    float deltaY = point2.y - point1.y;
    float deltaX = point2.x - point1.x;
    return std::atan2(deltaY, deltaX);
}



void setListenerPosition(const glm::vec3& position) {
    alListener3f(AL_POSITION, position.x, position.y, position.z);
}



AudioFilter::AudioFilter(FilterType t, float gain, float hfGain, float lfGain) : type(t) {
    create(gain, hfGain, lfGain);
}

AudioFilter::~AudioFilter() {
    if (filterID != 0 && extras::openAL->openAlLoaded) {
        alDeleteFilters(1, &filterID);
    }
}

ALuint AudioFilter::create(float gain, float hfGain, float lfGain) {
    if (!extras::openAL->openAlLoaded) return -1;

    alGenFilters(1, &filterID);
    if (alIsFilter(filterID)) {
        // Set the generic type
        alFilteri(filterID, AL_FILTER_TYPE, (ALint)type);

        // Apply parameters based on type
        adjust(gain, hfGain, lfGain);

        filterLoaded = true;
        extras::openAL->loadedFilters.push_back(filterID);
        return filterID;
    }
    return -1;
}

bool AudioFilter::adjust(float gain, float hfGain, float lfGain) {
    if (!extras::openAL->openAlLoaded || !alIsFilter(filterID)) return false;

    // Every filter has a master Gain
    alFilterf(filterID, AL_LOWPASS_GAIN, gain);

    if (type == FilterType::LowPass) {
        alFilterf(filterID, AL_LOWPASS_GAINHF, hfGain);
    }
    else if (type == FilterType::HighPass) {
        alFilterf(filterID, AL_HIGHPASS_GAINLF, lfGain);
    }
    else if (type == FilterType::BandPass) {
        alFilterf(filterID, AL_BANDPASS_GAINHF, hfGain);
        alFilterf(filterID, AL_BANDPASS_GAINLF, lfGain);
    }

    return true;
}



SoundEffect::SoundEffect(int sourceCount)
    : sourceCount(sourceCount), nextSourceIndex(0), stopThread(false) {
    if (extras::openAL == nullptr) {
        throw std::runtime_error("Failed to create sound effect as openAL is not initalized!");
    }

    alGenBuffers(1, &buffer);

    sources.resize(sourceCount);
    alGenSources(sourceCount, sources.data());

    CheckALErrors();

    extras::openAL->loadedSounds.push_back(this);

    // Start the volume monitoring thread
    volumeThread = std::thread(&SoundEffect::volumeMonitoringThread, this);
}

SoundEffect::SoundEffect(const std::string& filename, int sourceCount, bool forceMono)
    : sourceCount(sourceCount), nextSourceIndex(0), stopThread(false) {
    if (extras::openAL == nullptr) {
        throw std::runtime_error("Failed to create sound effect as openAL is not initalized!");
    }

    alGenBuffers(1, &buffer);

    sources.resize(sourceCount);
    alGenSources(sourceCount, sources.data());

    loadAudioFile(filename);

    if (forceMono && format == AL_FORMAT_STEREO_FLOAT32) {
        std::vector<float> monoSamples(samples.size() / 2);
        for (size_t frame = 0; frame < monoSamples.size(); ++frame) {
            monoSamples[frame] = 0.5f * (samples[frame * 2] + samples[frame * 2 + 1]);
        }
        samples = std::move(monoSamples);
        format = AL_FORMAT_MONO_FLOAT32;
    }

    bufferData();

    CheckALErrors();

    extras::openAL->loadedSounds.push_back(this);

    // Start the volume monitoring thread
    volumeThread = std::thread(&SoundEffect::volumeMonitoringThread, this);
}

void SoundEffect::CleanupSound() {
    if (deleted) return;

    stopThread = true;
    if (volumeThread.joinable()) volumeThread.join();

    for (ALuint source : sources) {
        alSourceStop(source);
        alSourcei(source, AL_BUFFER, 0);
    }
    if (!sources.empty()) {
        alDeleteSources((ALsizei)sources.size(), sources.data());
        sources.clear(); // Important: clear so destructor doesn't try again
    }
    if (buffer != 0) {
        alDeleteBuffers(1, &buffer);
        buffer = 0; // Important
    }
    deleted = true;
}

// Your destructor now just calls Cleanup
SoundEffect::~SoundEffect() {
    CleanupSound();
}

void SoundEffect::updateData(std::vector<float> _samples, ALenum _format, ALsizei _sampleRate) {
    samples = _samples;
    format = _format;
    sampleRate = _sampleRate;
    bufferData();
}

void SoundEffect::bufferData() {
    if (samples.empty()) {
        return;
    }

    for (ALuint source : sources) {
        alSourcei(source, AL_BUFFER, 0);
    }

    alBufferData(buffer, format, samples.data(), static_cast<ALsizei>(samples.size() * sizeof(float)), sampleRate);
    if (CheckALErrors()) {
        std::cerr << "Error loading buffer data" << std::endl;
        return;
    }

    for (ALuint source : sources) {
        alSourcei(source, AL_BUFFER, buffer);
    }
}

void SoundEffect::volumeMonitoringThread() {
    while (!stopThread) {
        if (extras::openAL->closing == true) {
            stop();
            break;
        }
        float volumeScale = calculateVolume();
        setVolume(volumeScale);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Adjust the delay as needed
    }
}

float SoundEffect::calculateVolume() {
    int totalSamples = samples.size() / sizeof(float);

    std::vector<float> combinedSamples(sampleRate, 0);  // Buffer to hold combined samples

    for (ALuint source : sources) {
        ALint state;
        alGetSourcei(source, AL_SOURCE_STATE, &state);
        if (state == AL_PLAYING) {
            ALint sampleOffset;
            alGetSourcei(source, AL_SAMPLE_OFFSET, &sampleOffset);

            // Calculate the number of samples to retrieve up to the next sampleRate
            int samplesToRetrieve = (std::min)(sampleRate, totalSamples - sampleOffset);

            for (int i = 0; i < samplesToRetrieve; ++i) {
                combinedSamples[i] += samples[sampleOffset + i];
            }
        }
    }

    float max = 0;
    for (auto sample : combinedSamples) {
        if (sample > max) {
            max = sample;
        }
    }

    if (max > 1) {
        return maxVolume * (1.0 / max);
    }
    else {
        return maxVolume;
    }

    //return maxVolume / max;
    //if (max > volumeScale) {
    //    return (volumeScale / max);
    //}
    //else {
    //    return volumeScale;
    //}
}

void SoundEffect::play(bool withPos) {
    ALuint source = sources[nextSourceIndex];
    // Only set the source to relative if withPos is false; otherwise ensure it is not relative
    alSourcei(source, AL_SOURCE_RELATIVE, withPos ? AL_FALSE : AL_TRUE);
    ALint state;
    alGetSourcei(source, AL_SOURCE_STATE, &state);
    ALint sampleOffset;
    alGetSourcei(source, AL_SAMPLE_OFFSET, &sampleOffset);
    if (state == AL_PLAYING && sampleOffset != 0) {
        alSourcePlay(source);
        nextSourceIndex = (nextSourceIndex + 1) % sourceCount;

        float volumeScale = calculateVolume();
        setVolume(volumeScale);
    }
    else if (state != AL_PLAYING) {
        alSourcePlay(source);
        nextSourceIndex = (nextSourceIndex + 1) % sourceCount;

        float volumeScale = calculateVolume();
        setVolume(volumeScale);
    }

    CheckALErrors();
}

bool SoundEffect::ensureMonoFormat() {
    if (format != AL_FORMAT_MONO16 && format != AL_FORMAT_MONO_FLOAT32) {
        std::cerr << "Warning: Trying to spatialize non-mono sound" << std::endl;
        return false;
    }
    return true;
}

void SoundEffect::playAt3DPosition(const glm::vec3& position) {
    if (!ensureMonoFormat()) return;

    ALuint source = sources[nextSourceIndex];
    alSource3f(source, AL_POSITION, position.x, position.y, position.z);
    setSourceDistanceModel(source, 6.0f, 0.8f, 80.0f);
    play(true);
}

void SoundEffect::playWith2DPan(float pan, float distance) {
    if (!ensureMonoFormat()) return;

    pan = Clamp(pan, -1.0, 1.0);
    distance = std::max(0.1f, distance); // Prevent zero or negative distances
    ALuint source = sources[nextSourceIndex];

    setSourceDistanceModel(source, 256.0, 0.75, 10000.0);
    alSource3f(source, AL_POSITION, pan * distance, -sqrtf(1.0f - pan * pan) * distance, 0.0f);
    play(true);
}

void SoundEffect::pause() {
    for (ALuint source : sources) {
        ALint state;
        alGetSourcei(source, AL_SOURCE_STATE, &state);
        if (state == AL_PLAYING) {
            alSourcePause(source);
        }
    }
}

void SoundEffect::stop() {
    for (ALuint source : sources) {
        alSourceStop(source);
    }
}

bool SoundEffect::isPlaying() const {
    for (ALuint source : sources) {
        ALint state;
        alGetSourcei(source, AL_SOURCE_STATE, &state);
        if (state == AL_PLAYING) {
            return true;
        }
    }
    return false;
}

void SoundEffect::attachFilter(ALuint filter) {
    for (ALuint source : sources) {
        alSourcei(source, AL_DIRECT_FILTER, filter);
    }
}

void SoundEffect::detachFilter(ALuint filter) {
    for (ALuint source : sources) {
        alSourcei(source, AL_DIRECT_FILTER, AL_FILTER_NULL);
    }
}

void SoundEffect::setVolume(float volume) {
    for (ALuint source : sources) {
        alSourcef(source, AL_GAIN, volume);
    }
}

void SoundEffect::setMaxVolume(float volume) {
    maxVolume = volume;
    for (ALuint source : sources) {
        alSourcef(source, AL_MAX_GAIN, volume);
    }
}

void SoundEffect::loadAudioFile(const std::string& filename) {
    SF_INFO sfInfo;
    SNDFILE* sndFile = sf_open(filename.c_str(), SFM_READ, &sfInfo);
    if (!sndFile) {
        throw std::runtime_error("Failed to open audio file: " + filename);
    }

    sampleRate = sfInfo.samplerate;
    if (sfInfo.channels == 1) {
        format = AL_FORMAT_MONO_FLOAT32;
    }
    else if (sfInfo.channels == 2) {
        format = AL_FORMAT_STEREO_FLOAT32;
    }
    else {
        sf_close(sndFile);
        throw std::runtime_error("Unsupported channel count: " + std::to_string(sfInfo.channels));
    }

    std::vector<float> floatSamples(sfInfo.frames * sfInfo.channels);
    sf_count_t numFrames = sf_readf_float(sndFile, floatSamples.data(), sfInfo.frames);
    if (numFrames < 1) {
        sf_close(sndFile);
        throw std::runtime_error("Failed to read samples from audio file: " + filename);
    }

    // Find the maximum absolute sample value
    float maxSample = 0.0f;
    for (auto sample : floatSamples) {
        if (std::abs(sample) > maxSample) {
            maxSample = std::abs(sample);
        }
    }

    // Normalize if the maximum sample value exceeds the float range
    if (maxSample > 1.0f) {
        float normalizationFactor = 1.0f / maxSample;
        for (auto& sample : floatSamples) {
            sample *= normalizationFactor;
        }
    }

    // Save normalized float samples
    samples = std::move(floatSamples);

    sf_close(sndFile);
}

void SoundEffect::applyFilter(const std::function<void(std::vector<float>&)>& filter) {
    filter(samples);
}



MusicSound::MusicSound(const std::string& filename, bool loop) : streaming(true), playing(false), looping(loop) {
    alGenSources(1, &source);
    // Make the source relative to the listener
    //alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);

    buffers.resize(4);
    alGenBuffers(4, buffers.data());

    loadAudioFile(filename);

    if (sndFile == nullptr) {
        //std::cerr << "Failed to load audio file" << std::endl;
        return;
    }

    fillBuffers();

    CheckALErrors();

    extras::openAL->loadedMusicSounds.push_back(this);

    streamingThread = std::thread(&MusicSound::streamAudio, this);
}

void MusicSound::CleanupMusic() {
    if (deleted) return;

    // 1. Stop the thread first!
    streaming = false;
    if (streamingThread.joinable()) {
        streamingThread.join();
    }

    // 2. Now clean up OpenAL handles while context is still alive
    if (source != 0) {
        alSourceStop(source);
        alSourcei(source, AL_BUFFER, 0); // Detach buffers
        alDeleteSources(1, &source);
        source = 0;
    }

    if (!buffers.empty()) {
        alDeleteBuffers(static_cast<ALsizei>(buffers.size()), buffers.data());
        buffers.clear();
    }

    if (sndFile) {
        sf_close(sndFile);
        sndFile = nullptr;
    }

    deleted = true;
}

// Your destructor now just calls the cleanup
MusicSound::~MusicSound() {
    CleanupMusic();
}

void MusicSound::play() {
    if (!playing) {
        playing = true;
        ALint state;
        alGetSourcei(source, AL_SOURCE_STATE, &state);
        if (state != AL_PLAYING) {
            alSourcePlay(source);
        }
    }

    CheckALErrors();
}

void MusicSound::pause() {
    ALint state;
    alGetSourcei(source, AL_SOURCE_STATE, &state);
    if (state == AL_PLAYING) {
        alSourcePause(source);
    }
}

void MusicSound::stop() {
    if (playing) {
        playing = false;

        // Rewind the sound file to the beginning
        sf_seek(sndFile, 0, SEEK_SET);

        // Refill buffers with the beginning of the audio file
        fillBuffers();
    }
}

bool MusicSound::isPlaying() const {
    ALint state;
    alGetSourcei(source, AL_SOURCE_STATE, &state);
    return state == AL_PLAYING;
}

void MusicSound::setVolume(float volume) {
    alSourcef(source, AL_MAX_GAIN, volume);
    alSourcef(source, AL_GAIN, volume);
}

void MusicSound::attachFilter(ALuint filter) {
    alSourcei(source, AL_DIRECT_FILTER, filter);
}

void MusicSound::detachFilter(ALuint filter) {
    alSourcei(source, AL_DIRECT_FILTER, AL_FILTER_NULL);
}

void MusicSound::loadAudioFile(const std::string& filename) {
    sndFile = sf_open(filename.c_str(), SFM_READ, &sfInfo);
    if (!sndFile) {
        std::cout << "Failed to open audio file: " + filename << std::endl;
        return;
    }

    if (sfInfo.channels == 1) {
        format = AL_FORMAT_MONO_FLOAT32;
    }
    else if (sfInfo.channels == 2) {
        format = AL_FORMAT_STEREO_FLOAT32;
    }
    else {
        sf_close(sndFile);
        sndFile = nullptr;
        throw std::runtime_error("Unsupported channel count: " + std::to_string(sfInfo.channels));
    }

    // Music is streamed, so only open the decoder here. The first buffers are
    // filled by fillBuffers(); decoding the complete track at startup only to
    // discard those samples was unnecessary.
    if (sfInfo.frames < 1 || sfInfo.samplerate < 1) {
        sf_close(sndFile);
        sndFile = nullptr;
        throw std::runtime_error("Audio file contains no playable samples: " + filename);
    }
}

sf_count_t MusicSound::readAudioFrames(float* destination, sf_count_t frameCount) {
    if (!sndFile || frameCount <= 0) return 0;

    sf_count_t framesRead = 0;
    bool wrapped = false;
    while (framesRead < frameCount) {
        const sf_count_t count = sf_readf_float(
            sndFile, destination + framesRead * sfInfo.channels, frameCount - framesRead);
        if (count > 0) {
            framesRead += count;
            continue;
        }
        if (!looping || wrapped || sf_seek(sndFile, 0, SEEK_SET) < 0) break;
        wrapped = true;
    }
    return framesRead;
}

void MusicSound::fillBuffers() {
    // Stop the source to safely modify buffer data
    alSourceStop(source);

    // Retrieve the number of processed buffers
    ALint processed = 0;
    alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);

    std::vector<ALuint> processedBuffers(processed);
    for (int i = 0; i < processed; ++i) {
        ALuint buffer;
        alSourceUnqueueBuffers(source, 1, &buffer);
        processedBuffers[i] = buffer;
    }

    // Refill processed buffers with new data
    std::vector<float> floatSamples(bufferSize);
    for (ALuint buffer : processedBuffers) {
        sf_count_t numFrames = readAudioFrames(floatSamples.data(), static_cast<sf_count_t>(bufferSize / sfInfo.channels));
        if (numFrames > 0) {
            alBufferData(buffer, format, floatSamples.data(), static_cast<ALsizei>(numFrames * sfInfo.channels * sizeof(float)), sfInfo.samplerate);
            alSourceQueueBuffers(source, 1, &buffer);
        }
    }

    // If no buffers are queued, queue all buffers
    ALint queued;
    alGetSourcei(source, AL_BUFFERS_QUEUED, &queued);
    if (queued == 0) {
        for (ALuint buffer : buffers) {
            sf_count_t numFrames = readAudioFrames(floatSamples.data(), static_cast<sf_count_t>(bufferSize / sfInfo.channels));
            if (numFrames > 0) {
                alBufferData(buffer, format, floatSamples.data(), static_cast<ALsizei>(numFrames * sfInfo.channels * sizeof(float)), sfInfo.samplerate);
                alSourceQueueBuffers(source, 1, &buffer);
            }
        }
    }

    // Resume playing if the source was playing before
    if (playing) {
        alSourcePlay(source);
    }
}

void MusicSound::streamAudio() {
    std::vector<float> floatSamples(bufferSize);

    while (streaming) {
        if (extras::openAL->closing == true) {
            alSourceStop(source);
            break;
        }
        if (playing) {
            ALint processed = 0;
            alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);

            for (int i = 0; i < processed; i++) {
                ALuint buffer;
                alSourceUnqueueBuffers(source, 1, &buffer);

                sf_count_t numFrames = readAudioFrames(floatSamples.data(), static_cast<sf_count_t>(bufferSize / sfInfo.channels));
                if (numFrames > 0) {
                    alBufferData(buffer, format, floatSamples.data(), static_cast<ALsizei>(numFrames * sfInfo.channels * sizeof(float)), sfInfo.samplerate);
                    alSourceQueueBuffers(source, 1, &buffer);
                }
            }

            ALint state;

            alGetSourcei(source, AL_SOURCE_STATE, &state);
            if (state != AL_PLAYING && processed != 0) {
                alSourcePlay(source);
            }
            alGetSourcei(source, AL_SOURCE_STATE, &state);
            if (state != AL_PLAYING && processed == 0) {
                stop();
            }
        }

        CheckALErrors();

        // Sleep for a longer duration as requested
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}



MusicManager::MusicManager() {

}

MusicManager::~MusicManager() {
    sounds.clear();
    //for (auto& sound : sounds) {
    //    if (sound != nullptr && sound->deleted == false) {
    //        delete sound;
    //    }
    //}
}

void MusicManager::addSound(const std::string& filename) {
    sounds.push_back(new MusicSound(filename));
}

// plays song only if no song is currently playing
void MusicManager::playMusic() {
    bool anyPlaying = false;
    for (auto sound : sounds) {
        if (sound->playing) {
            anyPlaying = true;
            break;
        }
    }

    if (!anyPlaying && sounds.size() != 0) {
        sounds[RNG<int>::getRange(0, sounds.size() - 1)]->play();
    }
}

void MusicManager::stopMusic() {
    for (auto sound : sounds) {
        sound->stop();
    }
}

void MusicManager::skipSong() {
    if (sounds.empty()) return;

    // 1. Find the currently playing song
    MusicSound* currentSound = nullptr;
    int currentIndex = -1;

    for (int i = 0; i < sounds.size(); ++i) {
        if (sounds[i]->playing) {
            currentSound = sounds[i];
            currentIndex = i;
            break;
        }
    }

    // 2. Scenario: Only one song in the list
    if (sounds.size() == 1) {
        if (currentSound) {
            currentSound->stop();
            currentSound->play(); // Restart it
        }
        else {
            sounds[0]->play(); // If nothing was playing, just start it
        }
        return;
    }

    // 3. Scenario: Multiple songs (Play a random one that isn't the current one)
    if (currentSound) {
        currentSound->stop();
    }

    int nextIndex = currentIndex;

    // Loop until we get a different index
    // If nothing was playing (currentIndex == -1), it just picks any random song
    while (nextIndex == currentIndex) {
        nextIndex = rand() % sounds.size();
    }

    sounds[nextIndex]->play();
}

void MusicManager::setVolume(float volume) {
    for (auto sound : sounds) {
        sound->setVolume(volume);
    }
}

void MusicManager::addFilter(ALuint filter) {
    for (auto sound : sounds) {
        sound->attachFilter(filter);
    }
}





glm::vec2 NDCtoScreen(glm::vec2 pos) {
    return pos * extras::activeCamera2D->currentViewportSize;
}





struct WrappedTextResult {
    std::vector<std::string> lines;
    std::vector<int> lineStartIndices;
    float totalHeight;
    float lineHeight;
};

static WrappedTextResult WrapText(const std::string& text, float availableWidth, float fontSize) {
    WrappedTextResult result;
    TextMeasurement baseM = MeasureText("Ag", fontSize);
    result.lineHeight = baseM.maxAscender + (-baseM.maxDescender) + 5.0f;

    if (availableWidth <= 0) return result;
    if (text.empty()) {
        result.lines.push_back("");
        result.lineStartIndices.push_back(0);
        result.totalHeight = result.lineHeight;
        return result;
    }

    std::string currentLine = "";
    int lineStartIdx = 0;

    for (int i = 0; i < (int)text.length(); ++i) {
        char c = text[i];

        // Manual Newline Support
        if (c == '\n') {
            result.lines.push_back(currentLine);
            result.lineStartIndices.push_back(lineStartIdx);
            currentLine = "";
            lineStartIdx = i + 1;
            continue;
        }

        std::string testLine = currentLine + c;
        float testWidth = MeasureText(testLine, fontSize).maxWidth;

        if (testWidth <= availableWidth) {
            currentLine += c;
        }
        else {
            // Push current line and start a new one
            result.lines.push_back(currentLine);
            result.lineStartIndices.push_back(lineStartIdx);

            // If we broke on a space, the next line starts after it
            // Otherwise, we break exactly at this character
            currentLine = c;
            lineStartIdx = i;
        }
    }

    // Push the final remaining text
    result.lines.push_back(currentLine);
    result.lineStartIndices.push_back(lineStartIdx);

    result.totalHeight = (float)result.lines.size() * result.lineHeight;
    return result;
}



void ScrollableContainer::Draw(const std::function<void(float, float)>& drawContentFunc) {
    // 1. Enable Clipping
    StartScissor(viewportRect, extras::activeCamera2D);

    // 2. Adjust drawing position based on offset
    // Content starts at the bottom of the viewport + offset
    float drawY = viewportRect.position.y + scrollOffset;

    // 3. Render the content
    drawContentFunc(viewportRect.position.x, drawY);

    // 4. Disable Clipping
    StopScissor();
}




int UILayout::CreateChild(TargetRectLayout childLayout) {
    children.push_back(UILayout(childLayout));
    children.back().parent = this;
    if (children.back().layout._depthLayer == -1) {
        children.back().layout._depthLayer = layout._depthLayer + 1;
    }
    return children.size() - 1;
}

void UILayout::RemapParents(UILayout* newParent) {
    this->parent = newParent;
    for (auto& child : children) {
        child.RemapParents(this); // Recursive call
    }
}

void UILayout::PassCalculateFixedMaxSize() {
    if (!layout.active) {
        return;
    }

    CalculateFixedMaxSize();
    for (auto& c : children) {
        c.PassCalculateFixedMaxSize();
    }
}

void UILayout::CalculateFixedMaxSize() {
    //if (layout._targetSize.x != -1 && layout._minSize.x == 0) {
    //    layout._minSize.x = layout._targetSize.x;
    //}
    //if (layout._targetSize.y != -1 && layout._minSize.y == 0) {
    //    layout._minSize.y = layout._targetSize.y;
    //}

    if (layout._maxSize.x == -1) {
        layout._maxSize.x = 100000;
    }
    if (layout._maxSize.y == -1) {
        layout._maxSize.y = 100000;
    }
}

void UILayout::PassConstraints() {
    if (!layout.active) {
        rect.size = { 0, 0 }; // Inactive nodes take up no space
        return;
    }

    // 1. Calculate what space is actually available for children inside this node
    float innerW = layout._maxSize.x - (layout.padding.left + layout.padding.right);
    float innerH = layout._maxSize.y - (layout.padding.top + layout.padding.bottom);

    // Ensure we don't pass negative values if padding is huge
    glm::vec2 childConstraint = glm::vec2(std::max(0.0f, innerW), std::max(0.0f, innerH));

    for (auto& c : children) {
        // 2. Force the child's maxSize to be no larger than the available space
        c.layout._maxSize.x = std::min(c.layout._maxSize.x, childConstraint.x);
        c.layout._maxSize.y = std::min(c.layout._maxSize.y, childConstraint.y);

        // 3. Recurse down the tree
        c.PassConstraints();
    }
}

void UILayout::PassWidthFit() {
    if (!layout.active) {
        return;
    }

    for (auto& c : children)
        c.PassWidthFit();

    ResolveWidthFit();
}

void UILayout::ResolveWidthFit() {

    // -------------------------------------------------
    // INTRINSIC TEXT WIDTH (non-wrapped text)
    // -------------------------------------------------

    std::string* textPtr = nullptr;

    if (std::holds_alternative<std::string>(layout.text))
        textPtr = &std::get<std::string>(layout.text);
    else if (std::holds_alternative<std::string*>(layout.text))
        textPtr = std::get<std::string*>(layout.text);

    if (textPtr && !layout.wrapText && layout._minSize.x == 0) {

        TextMeasurement m = extras::defaultTextRenderer->MeasureText(*textPtr);

        layout._minSize.x =
            m.maxWidth +
            layout.padding.left +
            layout.padding.right;

        calculatedLineHeight =
            m.maxAscender + (-m.maxDescender) +
            extras::defaultTextRenderer->GetLineSpacing();
    }

    float childrenNeededWidth = 0;
    bool horizontal = layout.childAxis == TargetRectLayout::HORIZONTAL;

    if (!children.empty()) {
        if (horizontal) {
            for (auto& c : children) {
                childrenNeededWidth += c.rect.size.x;
            }
            childrenNeededWidth += std::max((int)children.size() - 1, 0) * layout.childGap;
        }
        else {
            for (auto& c : children) {
                childrenNeededWidth = std::max(childrenNeededWidth, c.rect.size.x);
            }
        }
    }

    childrenNeededWidth += layout.padding.left + layout.padding.right;

    layout._minSize.x = std::max(layout._minSize.x, childrenNeededWidth);

    rect.size.x = std::max(layout._minSize.x, std::min(childrenNeededWidth, layout._maxSize.x));
}

void UILayout::PassFlexWidth() {
    if (!layout.active) return;

    ResolveFlexWidth();

    for (auto& c : children)
        c.PassFlexWidth();
}

void UILayout::ResolveFlexWidth() {
    std::string* textPtr = nullptr;

    if (std::holds_alternative<std::string>(layout.text))
        textPtr = &std::get<std::string>(layout.text);
    else if (std::holds_alternative<std::string*>(layout.text))
        textPtr = std::get<std::string*>(layout.text);

    // -------------------------------------------------
    // NON-WRAPPED TEXT
    // -------------------------------------------------
    if (!layout.wrapText) {
        wrappedLines.push_back(*textPtr);

        float totalHeight = calculatedLineHeight;
        layout._minSize.y = std::max(layout._minSize.y, totalHeight + layout.padding.top + layout.padding.bottom);
        return;
    }

    if (children.empty()) return;

    bool horizontal = layout.childAxis == TargetRectLayout::HORIZONTAL;

    if (!horizontal) {
        float availableWidth = rect.size.x - layout.padding.left - layout.padding.right;
        for (auto& c : children) {
            // Never let the cross-axis flex push a child below its min
            float targetW = std::max(availableWidth, c.layout._minSize.x);

            if (c.layout.widthGrowable || c.layout.widthShrinkable) {
                float actualMin = c.layout._minSize.x;
                float actualMax = std::max(actualMin, c.layout._maxSize.x);

                c.rect.size.x = std::clamp(targetW, actualMin, actualMax);
            }
        }
        return;
    }

    float available =
        rect.size.x
        - layout.padding.left
        - layout.padding.right
        - (children.size() - 1) * layout.childGap;

    float used = 0;

    for (auto& c : children)
        used += c.rect.size.x;

    float remaining = available - used;

    if (remaining > 0)
        GrowChildren(true, remaining);
    else if (remaining < 0)
        ShrinkChildren(true, remaining);
}

void UILayout::GrowChildren(bool horizontal, float remaining) {
    if (remaining <= 0 || children.empty()) return;

    while (remaining > 0.001f) {
        float minSize = FLT_MAX;
        int growableCount = 0;

        for (auto& c : children) {
            float& size = horizontal ? c.rect.size.x : c.rect.size.y;
            float max = horizontal ? c.layout._maxSize.x : c.layout._maxSize.y;
            // NEW: Check the growable flag
            bool canGrow = horizontal ? c.layout.widthGrowable : c.layout.heightGrowable;

            if (canGrow && size < max) {
                minSize = std::min(minSize, size);
                growableCount++;
            }
        }

        if (growableCount == 0) break;

        float nextMinSize = FLT_MAX;
        for (auto& c : children) {
            float& size = horizontal ? c.rect.size.x : c.rect.size.y;
            float max = horizontal ? c.layout._maxSize.x : c.layout._maxSize.y;
            bool canGrow = horizontal ? c.layout.widthGrowable : c.layout.heightGrowable;

            if (canGrow && size > minSize && size < nextMinSize && size < max) {
                nextMinSize = size;
            }
        }

        float targetSize = (nextMinSize == FLT_MAX) ? FLT_MAX : nextMinSize;
        float slice = remaining / growableCount;
        float totalGrownThisRound = 0;

        for (auto& c : children) {
            float& size = horizontal ? c.rect.size.x : c.rect.size.y;
            float max = horizontal ? c.layout._maxSize.x : c.layout._maxSize.y;
            bool canGrow = horizontal ? c.layout.widthGrowable : c.layout.heightGrowable;

            if (canGrow && size <= minSize && size < max) {
                float roomToGrow = std::min(targetSize - size, max - size);
                float growth = std::min(slice, roomToGrow);
                size += growth;
                remaining -= growth;
                totalGrownThisRound += growth;
            }
        }
        if (totalGrownThisRound <= 0) break;
    }
}

void UILayout::ShrinkChildren(bool horizontal, float remaining) {
    if (remaining >= 0 || children.empty()) return;

    while (remaining < -0.001f) {
        float maxSize = -FLT_MAX;
        int shrinkableCount = 0;

        for (auto& c : children) {
            float& size = horizontal ? c.rect.size.x : c.rect.size.y;
            float min = horizontal ? c.layout._minSize.x : c.layout._minSize.y;
            // NEW: Check the shrinkable flag
            bool canShrink = horizontal ? c.layout.widthShrinkable : c.layout.heightShrinkable;

            if (canShrink && size > min) {
                maxSize = std::max(maxSize, size);
                shrinkableCount++;
            }
        }

        if (shrinkableCount == 0) break;

        float nextMaxSize = -FLT_MAX;
        for (auto& c : children) {
            float& size = horizontal ? c.rect.size.x : c.rect.size.y;
            float min = horizontal ? c.layout._minSize.x : c.layout._minSize.y;
            bool canShrink = horizontal ? c.layout.widthShrinkable : c.layout.heightShrinkable;

            if (canShrink && size < maxSize && size > nextMaxSize && size > min) {
                nextMaxSize = size;
            }
        }

        float targetSize = (nextMaxSize == -FLT_MAX) ? -FLT_MAX : nextMaxSize;
        float slice = remaining / shrinkableCount;
        float totalShrunkThisRound = 0;

        for (auto& c : children) {
            float& size = horizontal ? c.rect.size.x : c.rect.size.y;
            float min = horizontal ? c.layout._minSize.x : c.layout._minSize.y;
            bool canShrink = horizontal ? c.layout.widthShrinkable : c.layout.heightShrinkable;

            if (canShrink && size >= maxSize && size > min) {
                float roomToShrink = std::max(targetSize - size, min - size);
                float shrink = std::max(slice, roomToShrink);
                size += shrink;
                remaining -= shrink;
                totalShrunkThisRound += shrink;
            }
        }
        if (totalShrunkThisRound >= 0) break;
    }
}

void UILayout::PassTextWrap() {
    if (!layout.active) return;

    ResolveWrapText();

    for (auto& c : children)
        c.PassTextWrap();
}

void UILayout::ResolveWrapText() {
    std::string* textPtr = nullptr;
    if (std::holds_alternative<std::string>(layout.text))
        textPtr = &std::get<std::string>(layout.text);
    else if (std::holds_alternative<std::string*>(layout.text))
        textPtr = std::get<std::string*>(layout.text);

    if (textPtr == nullptr) return;

    float availableWidth = rect.size.x - layout.padding.left - layout.padding.right;

    // Call the helper
    auto result = WrapText(*textPtr, availableWidth, layout.fontSize); // assuming 18pt font

    this->wrappedLines = result.lines;
    this->calculatedLineHeight = result.lineHeight;

    // Update the minSize for the layout engine
    layout._minSize.y = std::max(layout._minSize.y, result.totalHeight + layout.padding.top + layout.padding.bottom);
}

void UILayout::PassHeightFit() {
    if (!layout.active) return;

    for (auto& c : children)
        c.PassHeightFit();

    ResolveHeightFit();
}

void UILayout::ResolveHeightFit() {
    float childrenNeededHeight = 0;
    bool horizontal = layout.childAxis == TargetRectLayout::HORIZONTAL;

    if (!children.empty()) {
        if (!horizontal) { // Vertical Stack
            for (auto& c : children) {
                childrenNeededHeight += c.rect.size.y;
            }
            childrenNeededHeight += std::max((int)children.size() - 1, 0) * layout.childGap;
        }
        else { // Horizontal Stack
            for (auto& c : children) {
                childrenNeededHeight = std::max(childrenNeededHeight, c.rect.size.y);
            }
        }
    }

    childrenNeededHeight += layout.padding.top + layout.padding.bottom;

    // Update the requirement floor
    layout._minSize.y = std::max(layout._minSize.y, childrenNeededHeight);

    // Set the size, prioritizing the floor we just calculated
    rect.size.y = std::max(layout._minSize.y, std::min(childrenNeededHeight, layout._maxSize.y));
}

void UILayout::PassFlexHeight() {
    if (!layout.active) return;

    ResolveFlexHeight();

    for (auto& c : children)
        c.PassFlexHeight();
}

void UILayout::ResolveFlexHeight() {
    if (children.empty()) return;

    bool vertical = layout.childAxis == TargetRectLayout::VERTICAL;

    if (!vertical) {
        // --- CROSS-AXIS (Horizontal parent, vertical child growth) ---
        float availableHeight = rect.size.y - layout.padding.top - layout.padding.bottom;

        for (auto& c : children) {
            float currentH = c.rect.size.y;

            // Only stretch if the child is allowed to grow/shrink
            bool canGrow = (availableHeight > currentH && c.layout.heightGrowable);
            bool canShrink = (availableHeight < currentH && c.layout.heightShrinkable);

            if (canGrow || canShrink) {
                float actualMin = c.layout._minSize.y;
                float actualMax = std::max(actualMin, c.layout._maxSize.y);

                c.rect.size.y = std::clamp(availableHeight, actualMin, actualMax);
            }
        }
        return;
    }

    // Main-axis flex (Vertical stack)
    float available = rect.size.y
        - layout.padding.top
        - layout.padding.bottom
        - (std::max((int)children.size() - 1, 0) * layout.childGap);

    float used = 0;
    for (auto& c : children)
        used += c.rect.size.y;

    float remaining = available - used;

    if (remaining > 0)
        GrowChildren(false, remaining); // false = vertical
    else if (remaining < 0)
        ShrinkChildren(false, remaining);
}

void UILayout::PassPosition() {
    if (!layout.active) return;

    ResolvePositions();

    for (auto& c : children)
        c.PassPosition();
}

void UILayout::ResolvePositions() {
    if (children.empty()) return;

    bool horizontal = layout.childAxis == TargetRectLayout::HORIZONTAL;

    // Logic: 
    // If Horizontal: NORMAL = forward, REVERSED = backward.
    // If Vertical:   NORMAL = backward, REVERSED = forward.
    // This effectively "flips the meaning" of the order based on the axis.
    bool reversed = (layout.childOrder == TargetRectLayout::REVERSED);
    if (!horizontal) {
        reversed = !reversed;
    }

    // 1. Calculate total size of the stack for Main-Axis alignment
    float totalChildrenSize = 0;
    for (size_t i = 0; i < children.size(); ++i) {
        totalChildrenSize += horizontal ? children[i].rect.size.x : children[i].rect.size.y;
        if (i < children.size() - 1) totalChildrenSize += layout.childGap;
    }

    // 2. Base offsets
    float baseOffsetX = layout.padding.left;
    float baseOffsetY = layout.padding.top;

    // 3. Main-Axis Alignment
    if (horizontal) {
        float freeSpaceX = std::max(0.0f, (rect.size.x - layout.padding.left - layout.padding.right) - totalChildrenSize);
        if (layout.alignX == TargetRectLayout::CENTER)      baseOffsetX += freeSpaceX / 2.0f;
        else if (layout.alignX == TargetRectLayout::END)    baseOffsetX += freeSpaceX;
    }
    else {
        float freeSpaceY = std::max(0.0f, (rect.size.y - layout.padding.top - layout.padding.bottom) - totalChildrenSize);
        if (layout.alignY == TargetRectLayout::CENTER)      baseOffsetY += freeSpaceY / 2.0f;
        else if (layout.alignY == TargetRectLayout::END)    baseOffsetY += freeSpaceY;
    }

    float currentX = baseOffsetX;
    float currentY = baseOffsetY;

    // 4. Position children
    int numChildren = (int)children.size();
    for (int i = 0; i < numChildren; ++i) {
        // Use our flipped 'reversed' logic to pick the index
        int index = reversed ? (numChildren - 1 - i) : i;
        auto& c = children[index];

        c.rect.position = rect.position + glm::vec2(currentX, currentY);

        // 5. Cross-Axis Alignment (The "Center" fix)
        if (horizontal) {
            float availableH = rect.size.y - layout.padding.top - layout.padding.bottom;
            float freeY = availableH - c.rect.size.y;

            if (c.layout.alignY == TargetRectLayout::CENTER)      c.rect.position.y += freeY / 2.0f;
            else if (c.layout.alignY == TargetRectLayout::END)    c.rect.position.y += freeY;

            currentX += c.rect.size.x + layout.childGap;
        }
        else {
            float availableW = rect.size.x - layout.padding.left - layout.padding.right;
            float freeX = availableW - c.rect.size.x;

            if (c.layout.alignX == TargetRectLayout::CENTER)      c.rect.position.x += freeX / 2.0f;
            else if (c.layout.alignX == TargetRectLayout::END)    c.rect.position.x += freeX;

            currentY += c.rect.size.y + layout.childGap;
        }
    }
}

//void UILayout::PassRectUpdates() {
//    layout.boundRect = &this->rect;
//}

void UILayout::PassRender(bool glassy) {
    if (!layout.active) return;
    
    // 1. If this node is a scrollable area, start the clipping gate
    bool isClipping = scrollContainer.has_value();
    if (isClipping) {
        scrollContainer->viewportRect = this->rect; // Ensure viewport matches node size
        StartScissor(scrollContainer->viewportRect, extras::activeCamera2D);
    }

    if (glassy) {
        glm::vec2 rSize = rect.size;
        glm::vec2 rPos = rect.position;

        // Calculate true center for the Shader SDF
        glm::vec2 center = rPos + (rSize * 0.5f);

        // Update uniforms for this specific rectangle
        extras::ActiveShader->setUniform("shapeCenter", center);
        extras::ActiveShader->setUniform("isRect", true); // Enable Box SDF logic
        extras::ActiveShader->setUniform("rectSize", rSize);
        extras::ActiveShader->setUniform("cornerRadius", 10.0f); // rounded corners
        extras::ActiveShader->setUniform("tintStrength", 0.5f);
    }

    if (layout.drawFunction) {
        layout.drawFunction(this->rect);
    }
    else if (layout.backgroundColor != CLEAR) {
        DrawRectangle(rect, layout.backgroundColor);
    }

    std::string* textPtr = nullptr;

    if (std::holds_alternative<std::string>(layout.text))
        textPtr = &std::get<std::string>(layout.text);

    else if (std::holds_alternative<std::string*>(layout.text))
        textPtr = std::get<std::string*>(layout.text);

    if (textPtr != nullptr) {
        glm::vec2 linepos = rect.position;
        
        TextAlignmentX alignmentX = TextAlignmentX::LEFT;
        TextAlignmentY alignmentY = TextAlignmentY::BASELINE;
    
        if (layout.alignX == TargetRectLayout::Alignment::START) {
    
        }
        else if (layout.alignX == TargetRectLayout::Alignment::CENTER) {
            linepos.x += rect.size.x / 2.0;
            alignmentX = TextAlignmentX::CENTER;
        }
        else if (layout.alignX == TargetRectLayout::Alignment::END) {
            linepos.x += rect.size.x;
        }
    
        if (layout.alignY == TargetRectLayout::Alignment::START) {
    
        }
        else if (layout.alignY == TargetRectLayout::Alignment::CENTER) {
            linepos.y += rect.size.y / 2.0f;
            linepos.y -= (calculatedLineHeight * (wrappedLines.size() - 1)) / 2.0f;
            alignmentY = TextAlignmentY::CENTER;
        }
        else if (layout.alignY == TargetRectLayout::Alignment::END) {
            linepos.y += rect.size.y;
            alignmentY = TextAlignmentY::TOP;
        }
    
        for (int i = 0; i < wrappedLines.size(); i++) {
            DrawText(wrappedLines[i], linepos, 16, WHITE, alignmentX, alignmentY);
            linepos.y -= calculatedLineHeight;
        }
    }

    // 3. Render Children with offset
    for (auto& c : children) {
        if (isClipping) {
            // Apply the scroll offset to children positions before rendering
            // Note: We temporarily shift the child's rect so they draw in the right place
            Rect originalRect = c.rect;
            c.rect.position.y += scrollContainer->scrollOffset;

            c.PassRender(glassy);

            c.rect = originalRect; // Restore original for next frame
        }
        else {
            c.PassRender(glassy);
        }
    }

    // 4. Close the clipping gate
    if (isClipping) {
        StopScissor();
        // Draw Scrollbar here using scrollContainer->viewportRect and scrollContainer->contentHeight
    }
}



int UIManager::AddRootNode(TargetRectLayout layout) {
    initialRootNodes.push_back(UILayout(layout));
    if (initialRootNodes.back().layout._depthLayer == -1) {
        initialRootNodes.back().layout._depthLayer = 0;
    }
    return initialRootNodes.size() - 1;
}

void UIManager::ComputeUI()
{
    rootNodes = initialRootNodes;

    //for (auto& r : rootNodes) {
    //    r.RemapParents(nullptr); // Roots have no parent
    //}

    for (auto& r : rootNodes) r.PassCalculateFixedMaxSize();
    for (auto& r : rootNodes) r.PassConstraints();
    for (auto& r : rootNodes) r.PassWidthFit();
    for (auto& r : rootNodes) r.PassFlexWidth();
    for (auto& r : rootNodes) r.PassTextWrap();
    for (auto& r : rootNodes) r.PassHeightFit();
    for (auto& r : rootNodes) r.PassFlexHeight();
    for (auto& r : rootNodes) {
        float value = 0;
        for (auto& child : r.children) {
            value = std::max(value, (child.rect.size.y + r.layout.padding.top + r.layout.padding.bottom) - extras::activeCamera2D->currentViewportSize.y);
        }
        r.rect.position.y -= value;
    }
    for (auto& r : rootNodes) r.PassPosition();
    //for (auto& r : rootNodes) r.PassRectUpdates();
}

void UIManager::RenderUI(bool glassy, Texture* backgroundTexture) {
    if (glassy) {
        DrawRectangle(Rect(0, 0, extras::CurrentWindowWidth, extras::CurrentWindowHeight), *backgroundTexture);
        BeginShaderMode(extras::Default2DGeometryGlassShader);
        extras::Default2DGeometryGlassShader.setUniform("iResolution", GetCurrentWindowSize());
        extras::Default2DGeometryGlassShader.setUniform("blurredTexture", *backgroundTexture);
    }
    for (auto& r : rootNodes) r.PassRender(glassy);
    if (glassy) {
        EndShaderMode();
    }
}

// Helper function for recursive searching
UILayout* UIManager::FindNodeRecursive(UILayout* current, const std::string& name) {
    if (current->layout.name == name) {
        return current;
    }
    for (auto& child : current->children) {
        UILayout* found = FindNodeRecursive(&child, name);
        if (found) return found;
    }
    return nullptr;
}

UILayout* UIManager::FindNode(const std::string& name) {
    for (auto& root : initialRootNodes) {
        UILayout* found = FindNodeRecursive(&root, name);
        if (found) return found;
    }
    return nullptr;
}




Button::Button() {
    renderText.setFont(*extras::defaultFont);
    renderText.setString("");
    renderText.setCharacterSize(10); // in pixels, not points!
    //renderText.color = BLACK;
}

Button::Button(glm::vec2 position, glm::vec2 size, float fontSize, std::string text, glm::vec4 color) : rect({ position, size }), fontSize(fontSize), text(text), color(color) {
    renderText.setFont(*extras::defaultFont);
    renderText.setString(text);
    renderText.setCharacterSize(fontSize); // in pixels, not points!
    //renderText.color = BLACK;
}

Button::Button(Rect rect, float fontSize, std::string text, glm::vec4 color) : rect(rect), fontSize(fontSize), text(text), color(color) {
    renderText.setFont(*extras::defaultFont);
    renderText.setString(text);
    renderText.setCharacterSize(fontSize); // in pixels, not points!
    //renderText.color = BLACK;
}

bool Button::WasPressed() {
    return state == Button::States::PRESSED;
}

void Button::Update() {
    Update(extras::CurrentMouseScreenPosition);
}

void Button::Update(Camera2D cam) {
    Update(GetScreenToWorld2D(extras::CurrentMouseScreenPosition, &cam));
}

void Button::Update(glm::vec2 mouseWorldPos) {
    bool mouseDown = IsMouseButtonDown(GLFW_MOUSE_BUTTON_1);
    bool mousePressed = IsMouseButtonPressed(GLFW_MOUSE_BUTTON_1);
    glm::vec4 finalMargin = glm::vec4(0);
    if (useTexture) {
        glm::vec2 marginRatio = glm::vec2(rect.size.x / texture->width, rect.size.y / texture->height);
        glm::vec4 marginRatioVec4 = glm::vec4(marginRatio, marginRatio);
        finalMargin = margin * marginRatioVec4;
    }
    if (CheckPointInRectangle(mouseWorldPos, { glm::vec2(rect.position.x + finalMargin.x, rect.position.y + finalMargin.y), glm::vec2(rect.size.x + finalMargin.z, rect.size.y + finalMargin.w) })) {
        if (mousePressed) {
            state = Button::States::HELD;
            return;
        }
        else if (mouseDown && (state == Button::States::HELD || state == Button::States::WAS_HELD)) {
            state = Button::States::HELD;
            return;
        }
        else { // !mouseDown
            if (state == Button::States::HELD) {
                state = Button::States::PRESSED;
                return;
            }
            else {
                state = Button::States::HOVERING;
                return;
            }
        }
    }
    else if (!mouseDown) {
        state = Button::States::DEFAULT;
        return;
    }
    else if (state == Button::States::HELD || state == Button::States::WAS_HELD) {
        state = Button::States::WAS_HELD;
        return;
    }
    state = Button::States::DEFAULT;
    return;
}

void Button::Draw() {
    if (renderText.string != text) {
        renderText.setString(text);
    }

    glm::vec4 drawColor = color; //glm::mix(color, WHITE, 0.5);
    float startLum = (0.299f * drawColor.r + 0.587f * drawColor.g + 0.114f * drawColor.b);
    if (startLum >= 0.5) {
        if (state == Button::States::HELD) {
            drawColor = glm::mix(color, BLACK, 0.5);
        }
        else if (state == Button::States::HOVERING) {
            drawColor = glm::mix(color, BLACK, 0.25);
        }
    }
    else {
        if (state == Button::States::HELD) {
            drawColor = glm::mix(color, WHITE, 0.5);
        }
        else if (state == Button::States::HOVERING) {
            drawColor = glm::mix(color, WHITE, 0.25);
        }
    }

    if (useTexture) {
        DrawRectangle(rect, drawColor, *texture);
    }
    else {
        DrawRectangle(rect, drawColor);
    }

    //DrawTextCenteredVerticallyAndHorizontally(text, position.x + (size.x / 2), position.y + (size.y / 2), fontSize, BLACK);
    renderText.color = (startLum > 0.5f) ? BLACK : WHITE;
    renderText.setCharacterSize(fontSize);
    renderText.Draw(rect.position.x + (rect.size.x / 2), rect.position.y + (rect.size.y / 2), TextAlignmentX::CENTER, TextAlignmentY::CENTER);
}





TextInputBox::TextInputBox() 
    : defaultText(""), color(glm::vec4(0.9, 0.9, 0.9, 1.0)), textColor(BLACK) {

}

TextInputBox::TextInputBox(glm::vec2 pos, glm::vec2 size, float fontSize, std::string defaultText)
    : rect({ pos, size }), defaultText(defaultText), color(glm::vec4(0.9, 0.9, 0.9, 1.0)), textColor(BLACK) {
    renderText.setFont(*extras::defaultFont);
    renderText.setString(text);
    renderText.setCharacterSize(fontSize); // in pixels, not points!
    renderText.color = textColor;
}

void TextInputBox::clearSelection() {
    selectionStart = -1;
    selectionEnd = -1;
    lastCursorPosition = cursorPosition;
    isSelecting = false;
}

void TextInputBox::clear() {
    finalText = "";
    text = "";
    clearSelection();
    cursorPosition = 0;
}

void TextInputBox::deleteSelection() {
    if (selectionStart != -1 && selectionEnd != -1 && selectionStart != selectionEnd) {
        text.erase(selectionStart, selectionEnd - selectionStart);
        cursorPosition = selectionStart;
        clearSelection();
    }
}

// Change the signature to accept 'lineText'
int TextInputBox::findCursorPosition(const std::string& lineText, float clickX) {
    int tempPos = 0;
    float accumulatedWidth = 0;

    // Use 'lineText' instead of the global 'text'
    while (tempPos < (int)lineText.length()) {
        float charWidth = renderText.MeasureText(lineText.substr(tempPos, 1)).maxWidth;

        // Midpoint check: if click is past the half-way point of the char, snap to next index
        if (accumulatedWidth + (charWidth / 2.0f) > clickX) {
            break;
        }

        accumulatedWidth += charWidth;
        tempPos++;
    }

    return tempPos;
}

void TextInputBox::HandleKeys() {
    if (hasFocus) {
        for (auto kcbd : extras::keyCallbacksData) {
            int key = kcbd.key;
            int scancode = kcbd.scancode;
            int action = kcbd.action;
            int mods = kcbd.mods;
            if (action == GLFW_PRESS || action == GLFW_REPEAT) {
                bool shiftHeld = mods & GLFW_MOD_SHIFT;
                bool controlHeld = mods & GLFW_MOD_CONTROL;
                switch (key) {
                case GLFW_KEY_BACKSPACE:
                    if (selectionStart != -1 && selectionEnd != -1 && selectionStart != selectionEnd) {
                        deleteSelection();
                    }
                    else if (cursorPosition > 0 && text.length() > 0) {
                        text.erase(cursorPosition - 1, 1);
                        cursorPosition--;
                    }
                    break;
                case GLFW_KEY_DELETE:
                    if (selectionStart != -1 && selectionEnd != -1 && selectionStart != selectionEnd) {
                        deleteSelection();
                    }
                    else if (cursorPosition < text.size()) {
                        text.erase(cursorPosition, 1);
                    }
                    break;
                case GLFW_KEY_UP:
                case GLFW_KEY_DOWN: {
                    float wrapWidth = rect.size.x - (offsetFromLeft * 2);
                    auto wrap = WrapText(text, wrapWidth, renderText.characterSize);

                    // 1. Find current line index
                    int currentLineIdx = 0;
                    for (int i = 0; i < wrap.lineStartIndices.size(); ++i) {
                        if (cursorPosition >= wrap.lineStartIndices[i]) currentLineIdx = i;
                    }

                    // 2. Determine target line
                    int targetLineIdx = (key == GLFW_KEY_UP) ? currentLineIdx - 1 : currentLineIdx + 1;

                    if (targetLineIdx >= 0 && targetLineIdx < (int)wrap.lines.size()) {
                        float currentX = renderText.GetCursorX(wrap.lines[currentLineIdx],
                            cursorPosition - wrap.lineStartIndices[currentLineIdx]);

                        int localIdx = findCursorPosition(wrap.lines[targetLineIdx], currentX);
                        int newPos = wrap.lineStartIndices[targetLineIdx] + localIdx;

                        if (shiftHeld) {
                            // 1. Initialize selection if it doesn't exist
                            if (selectionStart == -1) {
                                selectionStart = selectionEnd = cursorPosition;
                                lastModifiedEnd = (key == GLFW_KEY_DOWN);
                            }

                            // 2. Identify the "Pivot" (the side that ISN'T moving)
                            int pivot = lastModifiedEnd ? selectionStart : selectionEnd;

                            // 3. Move the cursor
                            cursorPosition = newPos;

                            // 4. Re-calculate Start/End based on the pivot and the new position
                            selectionStart = std::min(pivot, cursorPosition);
                            selectionEnd = std::max(pivot, cursorPosition);

                            // 5. Update the flag: are we currently the 'end' or the 'start'?
                            lastModifiedEnd = (cursorPosition >= pivot);
                        }
                        else {
                            cursorPosition = newPos;
                            clearSelection();
                        }
                    }
                    break;
                }

                case GLFW_KEY_LEFT:
                    if (shiftHeld) {
                        if (cursorPosition > 0) {
                            cursorPosition--;

                            // First press of shift, initialize selection start and end if necessary
                            if (selectionStart == -1) {
                                selectionStart = selectionEnd = cursorPosition + 1;
                            }

                            if (cursorPosition < selectionStart) {
                                // If moving left beyond the current start, move the start
                                selectionStart = cursorPosition;
                                lastModifiedEnd = false;
                            }
                            else {
                                // Adjust selection range accordingly
                                if (lastModifiedEnd) {
                                    selectionEnd = cursorPosition;
                                }
                                else {
                                    selectionStart = std::min(selectionStart, cursorPosition);
                                }
                            }
                        }
                    }
                    else {
                        if (cursorPosition > 0) cursorPosition--;
                        clearSelection();
                    }
                    break;
                case GLFW_KEY_RIGHT:
                    if (shiftHeld) {
                        if (cursorPosition < text.size()) {
                            cursorPosition++;

                            // First press of shift, initialize selection start and end if necessary
                            if (selectionEnd == -1) {
                                selectionStart = selectionEnd = cursorPosition - 1;
                            }

                            if (cursorPosition > selectionEnd) {
                                // If moving right beyond the current end, move the end
                                selectionEnd = cursorPosition;
                                lastModifiedEnd = true;
                            }
                            else {
                                // Adjust selection range accordingly
                                if (!lastModifiedEnd) {
                                    selectionStart = cursorPosition;
                                }
                                else {
                                    selectionEnd = std::max(selectionEnd, cursorPosition);
                                }
                            }
                        }
                    }
                    else {
                        if (cursorPosition < text.size()) cursorPosition++;
                        clearSelection();
                    }
                    break;
                case GLFW_KEY_HOME:
                case GLFW_KEY_END: {
                    const int target = (key == GLFW_KEY_HOME) ? 0 : (int)text.size();
                    if (shiftHeld) {
                        // Extend from the side that isn't moving, as UP/DOWN do.
                        if (selectionStart == -1) {
                            selectionStart = selectionEnd = cursorPosition;
                            lastModifiedEnd = (key == GLFW_KEY_END);
                        }
                        int pivot = lastModifiedEnd ? selectionStart : selectionEnd;
                        cursorPosition = target;
                        selectionStart = std::min(pivot, cursorPosition);
                        selectionEnd = std::max(pivot, cursorPosition);
                        lastModifiedEnd = (cursorPosition >= pivot);
                    }
                    else {
                        cursorPosition = target;
                        clearSelection();
                    }
                    break;
                }
                case GLFW_KEY_V:
                    if (controlHeld) {
                        deleteSelection();
                        const char* clip = glfwGetClipboardString(extras::ActiveWindow);
                        if (clip) {
                            text.insert(cursorPosition, clip);
                            cursorPosition += strlen(clip);
                        }
                    }
                    break;
                case GLFW_KEY_C:
                    if (controlHeld && selectionStart != -1 && selectionEnd != -1) {
                        std::string selectedText = text.substr(selectionStart, selectionEnd - selectionStart);
                        glfwSetClipboardString(extras::ActiveWindow, selectedText.c_str());
                    }
                    break;
                case GLFW_KEY_X:
                    if (controlHeld && selectionStart != -1 && selectionEnd != -1) {
                        std::string selectedText = text.substr(selectionStart, selectionEnd - selectionStart);
                        glfwSetClipboardString(extras::ActiveWindow, selectedText.c_str());
                        deleteSelection();
                    }
                    break;
                case GLFW_KEY_A:
                    if (controlHeld) {
                        selectionStart = 0;
                        selectionEnd = text.length();
                        cursorPosition = text.length();
                    }
                    break;
                case GLFW_KEY_ENTER:
                    if (shiftHeld) {
                        // Manual Line Break
                        deleteSelection();
                        text.insert(cursorPosition, "\n");
                        cursorPosition++;
                        clearSelection();
                    }
                    else {
                        // Submit/Finish
                        finalText = text;
                        escapeCharPressed = true;
                    }
                    break;
                }
            }
        }

        for (auto codepointData : extras::codepointsData) {
            if (codepointData >= 32 && codepointData < 127) {
                deleteSelection();
                text.insert(text.begin() + cursorPosition, static_cast<char>(codepointData));
                cursorPosition++;
                clearSelection();
                ignoreMouse = true;
            }
        }
    }
}

void TextInputBox::Update(glm::vec2 mouseWorldPos) {
    const bool focusBeforeUpdate = hasFocus;
    const int cursorBeforeUpdate = cursorPosition;
    const int selectionStartBeforeUpdate = selectionStart;
    const int selectionEndBeforeUpdate = selectionEnd;
    const size_t textSizeBeforeUpdate = text.size();
    hadFocus = hasFocus;
    float wrapWidth = rect.size.x - (offsetFromLeft * 2);
    auto wrap = WrapText(text, wrapWidth, renderText.characterSize);

    // Update height logic (keep this)
    float minHeight = renderText.characterSize + 10.0f;
    rect.size.y = std::max(minHeight, wrap.totalHeight + 15.0f);

    // Vertical Line Detection for Bottom-Up coordinates:
    // Distance = (Top of Rect) - (Mouse Y)
    float topOfRect = rect.position.y + rect.size.y - 5.0f;
    float relativeY = topOfRect - mouseWorldPos.y;

    int newCursorPosition = 0;
    if (!wrap.lines.empty()) {
        int maxIndex = (int)wrap.lines.size() - 1;
        // relativeY will be positive as we move down from the top
        int lineIndex = std::clamp((int)(relativeY / wrap.lineHeight), 0, maxIndex);

        auto& targetLineText = wrap.lines[lineIndex];
        int lineStartIdx = wrap.lineStartIndices[lineIndex];
        float clickX = mouseWorldPos.x - (rect.position.x + offsetFromLeft + textOffsetX);

        newCursorPosition = lineStartIdx + findCursorPosition(targetLineText, clickX);
    }

    bool shiftPressed = IsKeyDown(GLFW_KEY_LEFT_SHIFT) || IsKeyDown(GLFW_KEY_RIGHT_SHIFT);

    if (IsMouseButtonPressed(GLFW_MOUSE_BUTTON_1)) {
        if (CheckPointInRectangle(mouseWorldPos, rect)) {
            hasFocus = true;

            if (!shiftPressed) {
                lastCursorPosition = newCursorPosition;
            }
        }
        else {
            //if (isSelecting == false) {
            hasFocus = false;
            finalText = text;
            //}
        }
    }

    if (IsMouseButtonDown(GLFW_MOUSE_BUTTON_1) && !ignoreMouse) {
        // Begin selection process or continue updating it
        if (isSelecting) {
            // Update the cursor position
            cursorPosition = newCursorPosition;

            // Determine if the cursor is within the existing selection range
            if (cursorPosition >= selectionStart && cursorPosition <= selectionEnd) {
                if (lastModifiedEnd) {
                    // Adjust the selection start or end based on last modification
                    selectionEnd = cursorPosition;
                }
                else {
                    selectionStart = cursorPosition;
                }
            }
            else {
                // Adjust selection based on whether cursor is lower or higher than the range
                if (cursorPosition < selectionStart) {
                    selectionStart = cursorPosition;
                    lastModifiedEnd = false; // Now the start was last modified
                }
                else if (cursorPosition > selectionEnd) {
                    selectionEnd = cursorPosition;
                    lastModifiedEnd = true; // Now the end was last modified
                }
            }
        }
        else {
            // Start a new selection
            isSelecting = true;
            selectionStart = newCursorPosition;
            selectionEnd = newCursorPosition;
            cursorPosition = newCursorPosition;
            lastModifiedEnd = true; // Track the end initially modified
            if (shiftPressed) {
                if (cursorPosition < lastCursorPosition) {
                    selectionStart = cursorPosition;
                    selectionEnd = lastCursorPosition;
                    lastModifiedEnd = false; // Now the start was last modified
                }
                else if (cursorPosition > lastCursorPosition) {
                    selectionStart = lastCursorPosition;
                    selectionEnd = cursorPosition;
                    lastModifiedEnd = true; // Now the end was last modified
                }
            }
        }
    }
    else if (IsMouseButtonReleased(GLFW_MOUSE_BUTTON_1)) {
        // Finalize the selection when the mouse button is released
        isSelecting = false;
    }

    ignoreMouse = false;
    escapeCharPressed = false;
    HandleKeys();
    if (autoDeselect && escapeCharPressed) hasFocus = false;

    if (hasFocus && (!focusBeforeUpdate || cursorPosition != cursorBeforeUpdate ||
        selectionStart != selectionStartBeforeUpdate || selectionEnd != selectionEndBeforeUpdate ||
        text.size() != textSizeBeforeUpdate)) {
        cursorBlinkStartTime = GetTime();
    }
}

bool TextInputBox::WasJustDeselected() {
    return hadFocus == true && hadFocus != hasFocus;
}

void TextInputBox::Update() {
    Update(extras::CurrentMouseScreenPosition);
}

void TextInputBox::Update(Camera2D cam) {
    Update(GetScreenToWorld2D(extras::CurrentMouseScreenPosition, &cam));
}

void TextInputBox::Draw() {
    DrawRectangle(rect, color);

    float wrapWidth = rect.size.x - (offsetFromLeft * 2);
    auto wrap = WrapText(text, wrapWidth, renderText.characterSize);

    // Loop backwards: Start at the last line index, end at 0
    for (int i = (int)wrap.lines.size() - 1; i >= 0; --i) {

        // 'visualLineIndex' converts the backwards 'i' into a 0-based 
        // offset from the bottom of the text block.
        int visualLineIndex = ((int)wrap.lines.size() - 1) - i;

        float lineY = rect.position.y + ((visualLineIndex + 0.5f) * wrap.lineHeight) + textOffsetY;
        float lineX = rect.position.x + offsetFromLeft + textOffsetX;
        const float textDrawX = static_cast<float>(static_cast<int>(lineX));
        const float textDrawY = static_cast<float>(static_cast<int>(lineY + (wrap.lineHeight * 0.5f)));
        bool hasSelectionOnLine = false;
        int selectedStartInLine = 0;
        int selectedEndInLine = 0;
        float selectedTextX = 0.0f;
        float selectedTextWidth = 0.0f;

        // --- 1. Draw Selection ---
        if (hasFocus && !text.empty() && selectionStart != -1 && selectionEnd != -1) {
            int lineStart = wrap.lineStartIndices[i];
            int lineEnd = (i + 1 < (int)wrap.lines.size()) ? wrap.lineStartIndices[i + 1] : (int)text.length();

            int drawStart = std::max(selectionStart, lineStart);
            int drawEnd = std::min(selectionEnd, lineEnd);

            if (drawStart < drawEnd) {
                selectedStartInLine = drawStart - lineStart;
                selectedEndInLine = drawEnd - lineStart;
                selectedTextX = textDrawX + renderText.GetCursorX(wrap.lines[i], selectedStartInLine);
                selectedTextWidth = renderText.GetCursorX(wrap.lines[i], selectedEndInLine) -
                    renderText.GetCursorX(wrap.lines[i], selectedStartInLine);
                hasSelectionOnLine = true;
            }
        }

        // --- 2. Draw Text ---
        if (text.empty()) {
            renderText.setString(defaultText);
            renderText.color = glm::mix(textColor, GRAY, 0.5);
        }
        else {
            renderText.setString(wrap.lines[i]);
            renderText.color = textColor;
        }
        renderText.DrawPrecise(textDrawX, textDrawY, TextAlignmentX::LEFT, TextAlignmentY::CENTER);
        if (hasSelectionOnLine) {
            DrawRectangle({ glm::vec2(selectedTextX, lineY), glm::vec2(selectedTextWidth, wrap.lineHeight) }, selectionColor);
        }
        if (invertSelection && hasSelectionOnLine) {
            renderText.setString(wrap.lines[i].substr(
                static_cast<size_t>(selectedStartInLine),
                static_cast<size_t>(selectedEndInLine - selectedStartInLine)));
            renderText.color = selectedTextColor;
            renderText.DrawPrecise(selectedTextX, textDrawY,
                TextAlignmentX::LEFT, TextAlignmentY::CENTER);
            renderText.setString(wrap.lines[i]);
            renderText.color = textColor;
        }

        // --- 3. Draw Cursor ---
        if (hasFocus) {
            int lineStart = wrap.lineStartIndices[i];
            int nextLineStart = (i + 1 < (int)wrap.lines.size()) ? wrap.lineStartIndices[i + 1] : (int)text.length() + 1;

            const bool cursorVisible = static_cast<int>((GetTime() - cursorBlinkStartTime) * 2.0) % 2 == 0;
            if (cursorVisible && cursorPosition >= lineStart && cursorPosition < nextLineStart) {
                float curX = textDrawX + renderText.GetCursorX(wrap.lines[i], cursorPosition - lineStart);
                DrawLine(curX, lineY, curX, lineY + wrap.lineHeight, cursorColor);
            }
        }
    }
}





SliderBar::SliderBar() : rect({ glm::vec2(0), glm::vec2(0) }), minValue(0), maxValue(1), value((minValue + maxValue) / 2.0), vertical(false) {}

SliderBar::SliderBar(glm::vec2 pos, glm::vec2 size, float minValue, float maxValue, bool vertical) : rect({ pos, size }), minValue(minValue), maxValue(maxValue), value((minValue + maxValue) / 2.0), vertical(vertical) {}

void SliderBar::Update() {
    Update(extras::CurrentMouseScreenPosition);
}

void SliderBar::Update(glm::vec2 mouseWorldPos)
{
    const float handleHalf = handleWidth * 0.5f;

    float effectiveSize = vertical
        ? rect.size.y - handleWidth
        : rect.size.x - handleWidth;

    float t = (value - minValue) / (maxValue - minValue);
    t = Clamp(t, 0.0f, 1.0f);

    glm::vec2 handlePos = rect.position;
    if (vertical)
        handlePos.y += handleHalf + t * effectiveSize;
    else
        handlePos.x += handleHalf + t * effectiveSize;

    Rect handleRect =
    {
        handlePos - glm::vec2(vertical ? 0 : handleHalf, vertical ? handleHalf : 0),
        vertical
            ? glm::vec2(rect.size.x, handleWidth)
            : glm::vec2(handleWidth, rect.size.y)
    };

    hovering = CheckPointInRectangle(mouseWorldPos, handleRect);

    if (!selected && IsMouseButtonPressed(GLFW_MOUSE_BUTTON_1))
    {
        if (CheckPointInRectangle(mouseWorldPos, rect))
            selected = true;
    }
    else if (selected && IsMouseButtonDown(GLFW_MOUSE_BUTTON_1))
    {
        float mouseCoord = vertical ? mouseWorldPos.y : mouseWorldPos.x;
        float start = vertical ? rect.position.y : rect.position.x;

        float normalized =
            (mouseCoord - start - handleHalf) / effectiveSize;

        if (minValue > maxValue)
            normalized = 1.0f - normalized; // invert movement direction

        float trueMin = std::min(minValue, maxValue);
        float trueMax = std::max(minValue, maxValue);

        value = trueMin + normalized * (trueMax - trueMin);
        value = Clamp(value, trueMin, trueMax);
    }
    else
    {
        selected = false;
    }
}

void SliderBar::Draw()
{
    const float handleHalf = handleWidth * 0.5f;

    float effectiveSize = vertical
        ? rect.size.y - handleWidth
        : rect.size.x - handleWidth;

    float t = (value - minValue) / (maxValue - minValue);
    t = Clamp(t, 0.0f, 1.0f);

    glm::vec2 handlePos = rect.position;
    if (vertical)
        handlePos.y += handleHalf + t * effectiveSize;
    else
        handlePos.x += handleHalf + t * effectiveSize;

    Rect handleRect =
    {
        handlePos - glm::vec2(vertical ? 0 : handleHalf, vertical ? handleHalf : 0),
        vertical
            ? glm::vec2(rect.size.x, handleWidth)
            : glm::vec2(handleWidth, rect.size.y)
    };

    glm::vec4 currHandleColor = handleColor;
    if (selected)
        currHandleColor = glm::mix(handleColor, BLACK, 0.5f);
    else if (hovering)
        currHandleColor = glm::mix(handleColor, WHITE, 0.5f);

    DrawRectangle(rect, backColor);
    DrawRectangle(handleRect, currHandleColor);
}






ScrollableTextBox::ScrollableTextBox()
    : textRenderer(), text("")
{
    lineHeight = GetLineHeight();
    //SetRect()  // call this once somewhere
}
//textRenderer.setCharacterSize(fontSize);
//textRenderer.setColor(fontColor);

ScrollableTextBox::ScrollableTextBox(std::string content)
    : textRenderer(), text(content)
{
    lineHeight = GetLineHeight();
    //SetRect()  // call this once somewhere
}

float ScrollableTextBox::GetLineHeight() const {
    TextMeasurement m = textRenderer.MeasureText("Ag");
    return m.maxAscender + (-m.maxDescender) + textRenderer.GetLineSpacing();
    //return textRenderer.characterSize + textRenderer.GetLineSpacing();
}

void ScrollableTextBox::SetRect(Rect _rect) {
    rect = _rect;
    innerRect = rect;
    innerRect.position += glm::vec2(padding, padding);
    innerRect.size -= glm::vec2(scrollBarWidth + (2 * padding), 2 * padding);
}

void ScrollableTextBox::Update() {
    Update(extras::CurrentMouseScreenPosition);
}

void ScrollableTextBox::Update(glm::vec2 mousePos) {
    Scroll(GetMouseWheelMove() * 15.0);
    Reflow(rect.size.x);
    if (GetContentHeight() > innerRect.size.y) {
        scrollSlider.minValue = getMaxOffset(); // refresh in case content changed
        scrollSlider.Update(mousePos);
        scrollOffset = scrollSlider.value;       // sync slider -> text scroll
    }
}

// Called whenever text changes or box width changes
void ScrollableTextBox::Reflow(float width)
{
    rect.size.x = width;
    SetRect(rect); // updates innerRect and slider

    lines.clear();
    std::istringstream ss(text);
    std::string originalLine;

    while (std::getline(ss, originalLine))
    {
        if (originalLine.empty()) { lines.push_back(""); continue; }

        // Step 1: Capture leading spaces
        size_t indentCount = 0;
        while (indentCount < originalLine.size() && originalLine[indentCount] == ' ')
            ++indentCount;

        std::string indent = originalLine.substr(0, indentCount);
        std::string content = originalLine.substr(indentCount);

        // Step 2: Word-wrap content only
        std::istringstream lineStream(content);
        std::string word;
        std::string line;

        while (lineStream >> word)
        {
            std::string testLine = line.empty() ? word : line + " " + word;
            TextMeasurement size = textRenderer.MeasureText(testLine);

            if (size.maxWidth <= innerRect.size.x)
            {
                line = testLine;
            }
            else
            {
                if (line.empty())
                {
                    std::string w = word;
                    while (!w.empty())
                    {
                        std::string part;
                        for (size_t i = 0; i < w.size(); ++i)
                        {
                            std::string temp = part + w[i];
                            TextMeasurement partSize = textRenderer.MeasureText(temp + "-");
                            if (partSize.maxWidth > innerRect.size.x) break;
                            part += w[i];
                        }
                        if (part.empty()) part = std::string(1, w[0]);
                        lines.push_back(indent + part + (w.size() > part.size() ? "-" : ""));
                        w.erase(0, part.size());
                    }
                }
                else
                {
                    lines.push_back(indent + line);
                    line = word;
                }
            }
        }

        if (!line.empty())
            lines.push_back(indent + line);
    }

    // Initialize slider
    scrollSlider.vertical = true;
    scrollSlider.rect = {
        { innerRect.position.x + innerRect.size.x, innerRect.position.y },
        { scrollBarWidth, innerRect.size.y }
    };
    scrollSlider.minValue = getMaxOffset();
    scrollSlider.maxValue = 0.0f;
    scrollSlider.value = scrollOffset;
}


float ScrollableTextBox::GetContentHeight() const { return lines.size() * lineHeight; }

float ScrollableTextBox::getMaxOffset() {
    return std::max(0.0f, GetContentHeight() - innerRect.size.y);
}

void ScrollableTextBox::Scroll(float delta)
{
    float maxOffset = getMaxOffset();
    scrollOffset = std::clamp(scrollOffset - delta, 0.0f, maxOffset);
}

void ScrollableTextBox::Draw() {
    DrawRectangle(rect, DARKGRAY);
    StartScissor(innerRect, extras::activeCamera2D);
    DrawRectangle(innerRect, GRAY);

    float boxBottom = innerRect.position.y;
    float boxTop = innerRect.position.y + innerRect.size.y;

    float y = (innerRect.position.y + 5) + innerRect.size.y - lineHeight + scrollOffset;

    for (const auto& line : lines)
    {
        float lineBottom = y;
        float lineTop = y + lineHeight;

        if (lineBottom >= boxTop)
        {
            y -= lineHeight;
            continue;
        }

        if (lineTop <= boxBottom)
            break;

        textRenderer.setString(line);
        textRenderer.Draw(innerRect.position.x, ceil(y) - padding);

        y -= lineHeight;
    }

    StopScissor();

    // Scrollbar
    if (GetContentHeight() > innerRect.size.y) {
        float ratio = innerRect.size.y / GetContentHeight();
        float scrollbarHeight = innerRect.size.y * ratio;
        scrollSlider.handleWidth = scrollbarHeight;

        scrollSlider.Draw();
    }
}




void SplitText(std::string& text, const Rect& rect, Text& textRenderer) {
    std::istringstream ss(text);
    std::string originalLine;
    std::string formattedText = "";

    float lineHeight = textRenderer.characterSize; // approx height per line
    float usedHeight = 0.0f;

    while (std::getline(ss, originalLine))
    {
        if (originalLine.empty())
        {
            // preserve empty lines (paragraph breaks)
            formattedText += "\n";
            usedHeight += lineHeight;
            if (usedHeight + lineHeight > rect.size.y)
            {
                text = formattedText;
                return;
            }
            continue;
        }

        std::istringstream lineStream(originalLine);
        std::string word;
        std::string line = "";

        while (lineStream >> word)
        {
            std::string testLine = line.empty() ? word : line + " " + word;
            TextMeasurement size = textRenderer.MeasureText(testLine);

            if (size.maxWidth <= rect.size.x)
            {
                line = testLine;
            }
            else
            {
                // Single word too long for width -> split
                if (line.empty())
                {
                    std::string w = word;
                    while (!w.empty())
                    {
                        std::string part;
                        for (size_t i = 0; i < w.size(); ++i)
                        {
                            std::string temp = part + w[i];
                            TextMeasurement partSize = textRenderer.MeasureText(temp + "-");
                            if (partSize.maxWidth > rect.size.x)
                                break;
                            part += w[i];
                        }

                        if (part.empty())
                            part = std::string(1, w[0]);

                        formattedText += part;
                        w.erase(0, part.size());
                        if (!w.empty())
                            formattedText += "-\n";
                        else
                            formattedText += "\n";

                        usedHeight += lineHeight;
                        if (usedHeight + lineHeight > rect.size.y)
                        {
                            text = formattedText;
                            return;
                        }
                    }
                }
                else
                {
                    // flush current line
                    formattedText += line + "\n";
                    usedHeight += lineHeight;
                    if (usedHeight + lineHeight > rect.size.y)
                    {
                        text = formattedText;
                        return;
                    }
                    line = word;
                }
            }
        }

        if (!line.empty())
        {
            formattedText += line + "\n";
            usedHeight += lineHeight;
            if (usedHeight > rect.size.y)
            {
                formattedText = formattedText.substr(0, formattedText.size() - (line.size() + 1));
                text = formattedText;
                return;
            }
        }
    }

    text = formattedText;
}





bool deserializeHeader(const std::vector<char>& buffer, uint16_t& mainType, uint16_t& subType, uint32_t& dataLength) {
    if (buffer.size() < sizeof(MessageHeader)) {
        std::cerr << "Buffer too small" << std::endl;
        return false;
    }

    MessageHeader header;
    std::memcpy(&header, buffer.data(), sizeof(MessageHeader));

    if (buffer.size() < sizeof(MessageHeader) + header.dataLength) {
        std::cerr << "Total size less than expected" << std::endl;
        return false;
    }

    mainType = header.mainType;
    subType = header.subType;
    dataLength = header.dataLength;

    return true;
}


NetworkLogger::NetworkLogger() {}

void NetworkLogger::LogNetworkUpload(int size) {
    total_up += 1;
    total_up_size += size;
}

void NetworkLogger::LogNetworkDownload(int size) {
    total_down += 1;
    total_down_size += size;
}

void NetworkLogger::Update(float dt) {
    totalTime += dt;
    if (totalTime >= 1) {
        ups_up = total_up / totalTime;
        ups_down = total_down / totalTime;
        ups_upSize = total_up_size;
        ups_downSize = total_down_size;
        total_up = 0;
        total_up_size = 0;
        total_down = 0;
        total_down_size = 0;
        totalTime = 0;
    }
}

NetworkManager::~NetworkManager() {
    if (server != nullptr) {
        enet_host_destroy(server);
    }
    if (client != nullptr) {
        enet_host_destroy(client);
    }
}

void NetworkManager::BroadcastNetworkMessage(const std::vector<char>& message) {
    if (server == nullptr) {
        std::cerr << "Server not initialized.\n";
        return;
    }

    networkLogger.LogNetworkUpload(message.size());

    // Create an ENet packet with the message data
    ENetPacket* packet = enet_packet_create(message.data(), message.size(), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(server, 0, packet);  // Send on Channel 0, typically you could use different channels for different types of messages
    enet_host_flush(server);  // Immediately attempts to send queued packets
}

void NetworkManager::SendNetworkMessageToPeer(const std::vector<char>& message, ENetPeer* target) {
    if (server == nullptr) {
        std::cerr << "Server not initialized.\n";
        return;
    }
    if (target == nullptr) {
        std::cerr << "Target is null.\n";
        return;
    }

    networkLogger.LogNetworkUpload(message.size());

    // Create an ENet packet with the message data
    ENetPacket* packet = enet_packet_create(message.data(), message.size(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(target, 0, packet);  // Send on Channel 0, typically you could use different channels for different types of messages
    enet_host_flush(server);  // Immediately attempts to send queued packets
}

void NetworkManager::SendNetworkMessageToServer(const std::vector<char>& message) {
    if (client == nullptr) {
        std::cerr << "Client not initialized.\n";
        return;
    }
    if (serverPeer == nullptr) {
        std::cerr << "Not connected to server.\n";
        return;
    }

    networkLogger.LogNetworkUpload(message.size());

    // Create an ENet packet with the message data
    ENetPacket* packet = enet_packet_create(message.data(), message.size(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(serverPeer, 0, packet);  // Send on Channel 0, typically you could use different channels for different types of messages
    enet_host_flush(client);  // Immediately attempts to send queued packets
}

std::vector<char> NetworkManager::ReceiveNetworkMessage(ENetEvent& event) {
    networkLogger.LogNetworkDownload(event.packet->dataLength);

    return std::vector<char>(event.packet->data, event.packet->data + event.packet->dataLength);
}

void NetworkManager::CreateServer(int port) {
    this->port = port; // Store the port for the UI

    ENetAddress address;

    //enet_address_set_host(&address, "localhost");
    address.host = ENET_HOST_ANY;  // Bind the server to all available interfaces
    address.port = port;  // Set the server port

    thisID = 0;
    nextClientId = 0;

    server = enet_host_create(&address,  // the address to bind the server host to
        32,      // allow up to 32 clients and/or outgoing connections
        1,       // allow only 1 channel to be used
        0,       // assume any amount of incoming bandwidth
        0);      // assume any amount of outgoing bandwidth

    if (server == nullptr) {
        fprintf(stderr, "Failed to create the server host.\n");
        //exit(EXIT_FAILURE);
    }
}

std::vector<std::vector<char>> NetworkManager::HandleServerEvents() {
    std::vector<std::vector<char>> serializedMessages; // Store messages for later processing

    // Process incoming events
    ENetEvent event;
    while (enet_host_service(server, &event, 0) > 0) {
        if (event.type == ENET_EVENT_TYPE_CONNECT) {
            nextClientId += 1;

            std::cout << "A client connected from: " << (event.peer->address.host >> 24) << event.peer->address.port << ", Assigned ID : " << nextClientId << std::endl;

            Connection connection;
            connection.peer = event.peer;
            connection.clientId = nextClientId;
            connections[nextClientId] = connection;

            auto serializedMessage = serializeMessage(CONNECT, 0, nextClientId);
            SendNetworkMessageToPeer(serializedMessage, event.peer);
            serializedMessages.push_back(serializedMessage); // Save message for later processing
        }
        else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
            std::cout << "Before erase:" << std::endl;
            for (const auto& kv : connections) {
                std::cout << "Key: " << kv.first << ", Peer: " << kv.second.peer << std::endl;
            }

            // Initialize an iterator to go through the unordered_map
            auto it = connections.begin();
            while (it != connections.end()) {
                // Check if the peer matches the disconnected peer
                if (it->second.peer == event.peer) {
                    std::cout << "Client " << it->second.clientId << " disconnected.\n";
                    auto serializedMessage = serializeMessage(DISCONNECT, 0, it->second.clientId);
                    SendNetworkMessageToPeer(serializedMessage, event.peer);
                    serializedMessages.push_back(serializedMessage); // Save message for later processing
                    // Erase and move the iterator safely
                    it = connections.erase(it);
                }
                else {
                    ++it; // Only increment if not erased
                }
            }

            std::cout << "After erase:" << std::endl;
            for (const auto& kv : connections) {
                std::cout << "Key: " << kv.first << ", Peer: " << kv.second.peer << std::endl;
            }

            event.peer->data = nullptr; // Reset the peer's client information
        }
        else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
            std::vector<char> receivedSerializedMessage = ReceiveNetworkMessage(event);
            serializedMessages.push_back(receivedSerializedMessage); // Save message for later processing
            enet_packet_destroy(event.packet); // Still destroy the packet immediately
        }
    }

    return serializedMessages;
}

void NetworkManager::StopServer() {
    std::cout << "Stopping server...\n";
    if (server != nullptr) {
        enet_host_destroy(server); // Disconnect all clients and destroy the server host
        connections.clear();
        server = nullptr; // Set to nullptr to avoid using a freed pointer
    }
}

void NetworkManager::CreateClient(std::string ipAddress, int port) {
    StopClient();
    if (port < 1 || port > 65535) {
        fprintf(stderr, "Invalid server port.\n");
        return;
    }
    client = enet_host_create(nullptr,  // no specific address, acts as a client
        1,       // allow only 1 connection (to the server)
        1,       // allow only 1 channel to be used
        0,       // assume any amount of incoming bandwidth
        0);      // assume any amount of outgoing bandwidth

    if (client == nullptr) {
        fprintf(stderr, "Failed to create the client host.\n");
        return;
    }

    ENetAddress address;
    if (enet_address_set_host(&address, ipAddress.c_str()) != 0) {
        fprintf(stderr, "Failed to resolve server address.\n");
        StopClient();
        return;
    }
    address.port = port;  // Server port

    serverPeer = enet_host_connect(client, &address, 2, 0);
    if (serverPeer == nullptr) {
        fprintf(stderr, "No available peers for initiating an ENet connection.\n");
        StopClient();
        return;
    }

    timeWaitedForConnectionAttempt = 0;
}

std::vector<std::vector<char>> NetworkManager::TryConnectToServer(float dt, float maxTime) {
    std::vector<std::vector<char>> serializedMessages; // Store messages for later processing

    if (client == nullptr) return serializedMessages;
    timeWaitedForConnectionAttempt += dt;
    ENetEvent event;
    while (enet_host_service(client, &event, 0) > 0) {
        if (event.type == ENET_EVENT_TYPE_CONNECT) {
            std::cout << "Successfully connected to the server!\n";
        }
        if (event.type == ENET_EVENT_TYPE_RECEIVE) {
            std::vector<char> receivedSerializedMessage = ReceiveNetworkMessage(event);
            serializedMessages.push_back(receivedSerializedMessage); // Save message for later processing

            // Deserialize the message header to determine the type of the data
            uint16_t mainType, subType;
            uint32_t dataLength;
            if (deserializeHeader(receivedSerializedMessage, mainType, subType, dataLength) &&
                mainType == CONNECT) {
                int receivedID = 0;
                if (deserializeData<int>(receivedSerializedMessage, receivedID) && receivedID > 0) {
                    thisID = receivedID;
                    std::cout << "Successfully Assigned ID : " << thisID << "\n";
                }
            }
            enet_packet_destroy(event.packet);  // It's crucial to destroy the packet after processing to avoid memory leaks
        }
    }

    if (thisID == 0 && timeWaitedForConnectionAttempt > maxTime) {
        std::cout << "Failed to connect to the server after: " << maxTime << " seconds!\n";
        StopClient();
        auto serializedMessage = serializeMessage(CONNECTION_ATTEMPT_TIMEOUT, 0, 0);
        serializedMessages.push_back(serializedMessage); // Save message for later processing
    }

    return serializedMessages;
}

std::vector<std::vector<char>> NetworkManager::HandleClientEvents() {
    std::vector<std::vector<char>> serializedMessages; // Store messages for later processing

    ENetEvent event;
    while (enet_host_service(client, &event, 0) > 0) {
        if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
            std::cout << "Disconnected from server.\n";
            StopClient();
            break; // since we set client to nullptr, we dont want to keep processing events
        }
        if (event.type == ENET_EVENT_TYPE_RECEIVE) {
            std::vector<char> serializedMessage = ReceiveNetworkMessage(event);
            serializedMessages.push_back(serializedMessage); // Save message for later processing
            enet_packet_destroy(event.packet); // Still destroy the packet immediately
        }
    }

    return serializedMessages;
}

void NetworkManager::StopClient() {
    std::cout << "Stopping client...\n";
    if (client != nullptr) {
        if (serverPeer != nullptr) enet_peer_disconnect_now(serverPeer, 0);
        enet_host_destroy(client); // Destroy the client host
        client = nullptr; // Set to nullptr to avoid using a freed pointer
    }
    serverPeer = nullptr;
    thisID = 0;
    timeWaitedForConnectionAttempt = 0;
}

















glm::vec4 hsvToRgb(float hue, float saturation, float value) {
    float h = hue, s = saturation, v = value;
    float c = v * s; // Chroma
    float x = c * (1 - fabs(fmod(h / 60.0f, 2) - 1));
    float m = v - c;
    glm::vec3 rgb;

    if (h >= 0 && h < 60)
        rgb = glm::vec3(c, x, 0);
    else if (h >= 60 && h < 120)
        rgb = glm::vec3(x, c, 0);
    else if (h >= 120 && h < 180)
        rgb = glm::vec3(0, c, x);
    else if (h >= 180 && h < 240)
        rgb = glm::vec3(0, x, c);
    else if (h >= 240 && h < 300)
        rgb = glm::vec3(x, 0, c);
    else if (h >= 300 && h <= 360)
        rgb = glm::vec3(c, 0, x);
    else
        rgb = glm::vec3(0, 0, 0); // If h is out of bounds, default to black

    rgb = (rgb + glm::vec3(m, m, m)); // Adjust for the value
    return glm::vec4(rgb, 1.0f); // Assuming the alpha value is always 1
}

glm::vec3 rgbToHsv(const glm::vec3& rgb) {
    float r = rgb.r, g = rgb.g, b = rgb.b;
    float max = std::max(r, std::max(g, b)), min = std::min(r, std::min(g, b));
    float h, s, v = max;

    float d = max - min;
    s = max == 0 ? 0 : d / max;

    if (max == min) {
        h = 0; // achromatic
    }
    else {
        if (max == r) {
            h = (g - b) / d + (g < b ? 6 : 0);
        }
        else if (max == g) {
            h = (b - r) / d + 2;
        }
        else if (max == b) {
            h = (r - g) / d + 4;
        }
        h /= 6;
    }

    return glm::vec3(h * 360, s, v); // Convert hue to degrees
}






std::string GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm = *std::localtime(&now_c);

    std::stringstream ss;
    // Format: YYYY-MM-DD_HH-MM-SS (e.g., 2026-03-10_13-05-30)
    ss << std::put_time(&now_tm, "%Y-%m-%d_%H-%M-%S");
    return ss.str();
}



ComfyUIManager::ComfyUIManager(const std::string& host, int port_num)
    : base_url(host), port(port_num) {
}

ComfyUIManager::~ComfyUIManager() {
    Shutdown();
}

void ComfyUIManager::StartImageGenerationJob(const std::string& promptText, const std::string& unique_job_id) {
    // Clean up any previously completed thread handle before starting a new one
    if (worker_thread.joinable() && !is_generating) {
        worker_thread.join();
    }

    if (is_generating) return;

    {
        std::lock_guard<std::mutex> lock(state_mutex);
        active_job_id = unique_job_id;
        is_generating = true;
        has_new_image = false;
        should_terminate = false; // Reset termination flag for the new job
    }

    // >>> FIX: Store the thread handle instead of calling .detach() <<<
    worker_thread = std::thread([this, promptText]() {
        this->AsyncGenerationWorker(promptText);
        });
}

void ComfyUIManager::Shutdown() {
    // Signal the worker thread to stop what it's doing immediately
    should_terminate = true;

    // If the thread is actively running, wait for it to finish its current loop step and exit cleanly
    if (worker_thread.joinable()) {
        std::cout << "[ComfyUI] Main engine requested exit. Waiting for background thread to close gracefully...\n";
        worker_thread.join();
        std::cout << "[ComfyUI] Background thread successfully joined and destroyed.\n";
    }
}

ComfyUIManager::CheckpointDetails ComfyUIManager::SelectAndInspectModel(int targetIndex) {
    CheckpointDetails details;
    httplib::Client cli(base_url, port);

    // Endpoint that lists file objects with descriptive parameter matrices
    auto res = cli.Get("/models/checkpoints");
    if (!res || res->status != 200) {
        std::cerr << "[ComfyUI Inspector] Failed to query checkpoints from endpoint.\n";
        return details;
    }

    try {
        auto parsedList = nlohmann::json::parse(res->body);
        if (!parsedList.is_array()) return details;

        std::cout << "\n=== AVAILABLE COMFYUI CHECKPOINTS ===\n";

        for (size_t i = 0; i < parsedList.size(); ++i) {
            std::string modelName = parsedList[i].get<std::string>();
            std::cout << "[" << i << "] " << modelName << "\n";

            // If this item matches the index requested by the user, populate details
            if (static_cast<int>(i) == targetIndex) {
                details.index = targetIndex;
                details.filename = modelName;
                details.is_currently_active = true;
            }
        }
        std::cout << "=======================================\n\n";

        if (details.index == -1) {
            std::cerr << "[ComfyUI Inspector] Provided index [" << targetIndex << "] is out of range.\n";
            return details;
        }

        std::cout << "[ComfyUI Inspector] Selected Model Profile Matrix:\n"
            << "  - Index: " << details.index << "\n"
            << "  - File:  " << details.filename << "\n";

        std::lock_guard<std::mutex> lock(state_mutex);
        current_checkpoint_name = details.filename;

    }
    catch (const std::exception& e) {
        std::cerr << "[ComfyUI Inspector Error] JSON alignment map processing failed: " << e.what() << "\n";
    }

    return details;
}

// Call this inside your active OpenGL update frame window loop
void ComfyUIManager::UpdateMainThreadTexture(Texture& targetTexture) {
    if (!has_new_image) return;

    std::lock_guard<std::mutex> lock(state_mutex);

    if (raw_rgba_pixels.empty() || img_width <= 0 || img_height <= 0) {
        has_new_image = false;
        return;
    }

    // 1. Allocate texture boundaries
    targetTexture.setupTexture(
        img_width, img_height,
        GL_FilterModes::LINEAR,
        GL_WrapModes::CLAMP_TO_EDGE,
        GL_InternalFormats::RGBA8,
        GL_ExternalFormats::RGBA,
        GL_DataFormats::UNSIGNED_BYTE,
        4
    );

    // 2. Direct transfer straight to your custom data pointer update method
    targetTexture.update(raw_rgba_pixels.data());

    // 3. Clear memory structures
    raw_rgba_pixels.clear();
    img_width = -1;
    img_height = -1;
    has_new_image = false;
}

void ComfyUIManager::AsyncGenerationWorker(const std::string& promptText) {
    httplib::Client cli(base_url, port);
    cli.set_read_timeout(60, 0);

    // Initial termination check before heavy operations
    if (should_terminate) {
        std::lock_guard<std::mutex> lock(state_mutex);
        is_generating = false;
        return;
    }

    std::string activeModel;
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        activeModel = current_checkpoint_name;
    }

    // 1. DYNAMIC FILENAME GENERATION
    // Generate a unique timestamp suffix (e.g., EngineOut_1718123456)
    // This forces ComfyUI to bypass its output cache and always populate the history array.
    auto now = std::chrono::system_clock::now();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    std::string uniquePrefix = "EngineOut_" + std::to_string(seconds);

    // Use your upgraded 64-bit Random Engine generator safely
    uint64_t massiveSeed = RNG<uint64_t>::getRange(0, 18446744073709551615ULL);

    nlohmann::json workflow = {
        {"3", {{"inputs", {{"seed", massiveSeed}, {"steps", 15}, {"cfg", 7.0}, {"sampler_name", "euler"}, {"scheduler", "normal"}, {"denoise", 1.0}, {"model", {"4", 0}}, {"positive", {"6", 0}}, {"negative", {"7", 0}}, {"latent_image", {"5", 0}}}}, {"class_type", "KSampler"}}},
        {"4", {{"inputs", {{"ckpt_name", activeModel}}}, {"class_type", "CheckpointLoaderSimple"}}},
        {"5", {{"inputs", {{"width", 512}, {"height", 512}, {"batch_size", 1}}}, {"class_type", "EmptyLatentImage"}}},
        {"6", {{"inputs", {{"text", promptText}, {"clip", {"4", 1}}}}, {"class_type", "CLIPTextEncode"}}},
        {"7", {{"inputs", {{"text", "lowres, text, error"}, {"clip", {"4", 1}}}}, {"class_type", "CLIPTextEncode"}}},
        {"8", {{"inputs", {{"samples", {"3", 0}}, {"vae", {"4", 2}}}}, {"class_type", "VAEDecode"}}},
        {"9", {{"inputs", {{"filename_prefix", uniquePrefix}, {"images", {"8", 0}}}}, {"class_type", "SaveImage"}}}
    };

    nlohmann::json payload = { {"prompt", workflow} };
    auto res = cli.Post("/prompt", payload.dump(), "application/json");
    if (!res || res->status != 200 || should_terminate) {
        std::lock_guard<std::mutex> lock(state_mutex);
        is_generating = false;
        return;
    }

    std::string prompt_uuid;
    try {
        prompt_uuid = nlohmann::json::parse(res->body)["prompt_id"].get<std::string>();
    }
    catch (...) {
        std::lock_guard<std::mutex> lock(state_mutex);
        is_generating = false;
        return;
    }

    // --- BULLETPROOF QUEUE-BASED POLLING ---
    bool is_done = false;
    while (!is_done) {
        // Fast interrupt if engine requested exit while generation is running
        if (should_terminate) {
            std::lock_guard<std::mutex> lock(state_mutex);
            is_generating = false;
            return;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        httplib::Client queueCli(base_url, port);
        auto queueRes = queueCli.Get("/queue");

        if (queueRes && queueRes->status == 200) {
            try {
                nlohmann::json queueData = nlohmann::json::parse(queueRes->body);
                bool found_in_queue = false;

                // Check both execution queue and pending queue arrays
                for (std::string queueType : {"queue_running", "queue_pending"}) {
                    if (queueData.contains(queueType) && queueData[queueType].is_array()) {
                        for (auto& item : queueData[queueType]) {
                            if (item.is_array() && item.size() > 1 && item[1] == prompt_uuid) {
                                found_in_queue = true;
                                break;
                            }
                        }
                    }
                    if (found_in_queue) break;
                }

                // If it's no longer anywhere in the active queue, ComfyUI has finished processing it!
                if (!found_in_queue || should_terminate) {
                    is_done = true;
                }
            }
            catch (...) {
                continue; // Transient JSON drop recovery
            }
        }
    }

    if (should_terminate) {
        std::lock_guard<std::mutex> lock(state_mutex);
        is_generating = false;
        return;
    }

    // --- STEP 4: ONE-TIME SAFE FETCH FROM HISTORY ---
    std::string filename = "";
    httplib::Client historyCli(base_url, port);
    auto historyRes = historyCli.Get("/history/" + prompt_uuid);

    if (historyRes && historyRes->status == 200) {
        try {
            nlohmann::json historyData = nlohmann::json::parse(historyRes->body);
            if (historyData.contains(prompt_uuid)) {
                auto& job = historyData[prompt_uuid];

                if (job.contains("outputs") && job["outputs"].contains("9")) {
                    auto& node9 = job["outputs"]["9"];

                    if (node9.contains("images") && node9["images"].is_array() && !node9["images"].empty()) {
                        auto& imgObj = node9["images"][0];

                        // Strict data safety check against null variants
                        if (imgObj.contains("filename") && imgObj["filename"].is_string()) {
                            filename = imgObj["filename"].get<std::string>();
                        }
                    }
                }
            }
        }
        catch (const std::exception& e) {
            std::cerr << "[ComfyUI] Safe history parse bypass caught: " << e.what() << "\n";
        }
    }

    if (filename.empty() || should_terminate) {
        std::cerr << "[ComfyUI] Aborting worker. Output filename unresolved or application closed.\n";
        std::lock_guard<std::mutex> lock(state_mutex);
        is_generating = false;
        return;
    }

    // Fetch standard PNG output stream body
    auto viewRes = cli.Get(("/view?filename=" + filename + "&type=output").c_str());
    if (!viewRes || viewRes->status != 200 || should_terminate) {
        std::lock_guard<std::mutex> lock(state_mutex);
        is_generating = false;
        return;
    }

    const std::string& compressedPngData = viewRes->body;

    // --- BACKGROUND THREAD PNG DECODE VIA STBI ---
    int width, height, components;
    unsigned char* decodedRawBytes = stbi_load_from_memory(
        reinterpret_cast<const unsigned char*>(compressedPngData.data()),
        static_cast<int>(compressedPngData.size()),
        &width, &height, &components, 4
    );

    if (!decodedRawBytes) {
        std::cerr << "[ComfyUI Worker] Background STB PNG decoding collapsed.\n";
        std::lock_guard<std::mutex> lock(state_mutex);
        is_generating = false;
        return;
    }

    // Copy raw, uncompressed pixel block vertically flipped directly to staging layout
    if (!should_terminate) {
        std::lock_guard<std::mutex> lock(state_mutex);
        img_width = width;
        img_height = height;

        size_t rowBytes = width * 4;
        raw_rgba_pixels.resize(width * height * 4);

        // Blit rows top-to-bottom from STB, writing them bottom-to-top into raw_rgba_pixels
        for (int y = 0; y < height; ++y) {
            unsigned char* sourceRow = decodedRawBytes + (y * rowBytes);
            size_t destRowOffset = (height - 1 - y) * rowBytes;
            std::memcpy(&raw_rgba_pixels[destRowOffset], sourceRow, rowBytes);
        }
        has_new_image = true;
    }

    // Free local thread tracking memory block
    stbi_image_free(decodedRawBytes);

    // Final clean unlatch
    std::lock_guard<std::mutex> lock(state_mutex);
    is_generating = false;
}








LLMManager::~LLMManager() {
    Shutdown();
}

void LLMManager::Shutdown() {
    should_terminate = true;
    if (worker_thread.joinable()) {
        std::cout << "[LLMManager] Clean exit requested. Joining active backend generation thread...\n";
        worker_thread.join();
        std::cout << "[LLMManager] Worker thread detached safely from system engine.\n";
    }
}

void LLMManager::StartWindowsClipboardImageJob(const std::string& prompt, const std::string& unique_msg_id) {
    // 1. Join any lingering finished threads before re-allocating
    if (worker_thread.joinable() && !active_stream_id.empty()) {
        worker_thread.join();
    }

    if (!OpenClipboard(nullptr)) {
        std::cerr << "[Clipboard Error] Cannot open system clipboard.\n";
        return;
    }

    // CASE 1: Copied file format processing
    if (IsClipboardFormatAvailable(CF_HDROP)) {
        HDROP hDrop = (HDROP)GetClipboardData(CF_HDROP);
        if (hDrop) {
            UINT fileCount = DragQueryFileA(hDrop, 0xFFFFFFFF, nullptr, 0);
            if (fileCount > 0) {
                char filePath[MAX_PATH];
                if (DragQueryFileA(hDrop, 0, filePath, MAX_PATH)) {
                    std::string path(filePath);
                    CloseClipboard();

                    std::string ext = std::filesystem::path(path).extension().string();
                    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
                        std::cout << "[Clipboard Engine] Found copied desktop file: " << path << "\n";
                        StartImageChatJob(prompt, path, unique_msg_id);
                        return;
                    }
                }
            }
        }
    }

    // CASE 2: Raw screenshot bits buffer handling
    HANDLE hClipboardData = GetClipboardData(CF_DIB);
    if (!hClipboardData) {
        std::cerr << "[Clipboard Info] No image files or raw screenshot data found in clipboard.\n";
        CloseClipboard();
        return;
    }

    LPBITMAPINFOHEADER lpbi = (LPBITMAPINFOHEADER)GlobalLock(hClipboardData);
    if (!lpbi) {
        std::cerr << "[Clipboard Error] Failed to lock global clipboard memory.\n";
        CloseClipboard();
        return;
    }

    int width = lpbi->biWidth;
    int height = lpbi->biHeight;
    int bpp = lpbi->biBitCount;
    int channels = bpp / 8;

    unsigned char* lpBits = (unsigned char*)(lpbi)+lpbi->biSize;
    if (lpbi->biClrUsed > 0) {
        lpBits += lpbi->biClrUsed * sizeof(RGBQUAD);
    }

    size_t pixel_data_size = width * height * channels;
    std::vector<unsigned char> rgba_pixels(pixel_data_size);

    for (int y = 0; y < height; ++y) {
        int target_y = height - 1 - y;
        for (int x = 0; x < width; ++x) {
            size_t src_idx = (y * width + x) * channels;
            size_t dst_idx = (target_y * width + x) * channels;

            rgba_pixels[dst_idx + 0] = lpBits[src_idx + 2]; // R
            rgba_pixels[dst_idx + 1] = lpBits[src_idx + 1]; // G
            rgba_pixels[dst_idx + 2] = lpBits[src_idx + 0]; // B
            if (channels == 4) {
                rgba_pixels[dst_idx + 3] = lpBits[src_idx + 3]; // A
            }
        }
    }

    GlobalUnlock(hClipboardData);
    CloseClipboard();

    std::string data_url = convert_raw_pixels_to_base64_png(rgba_pixels, width, height, channels);
    if (data_url.empty()) return;

    nlohmann::json input_payload = nlohmann::json::array({
        {{"type", "text"}, {"content", prompt}},
        {{"type", "image"}, {"data_url", data_url}}
        });

    PrepareForJob(unique_msg_id);

    worker_thread = std::thread([this, input_payload, unique_msg_id]() {
        this->AsyncChatWorker(input_payload, unique_msg_id);
        });
}

void LLMManager::StartChatJob(const std::string& prompt, const std::string& unique_msg_id, const std::string& system_prompt) {
    if (worker_thread.joinable()) { worker_thread.join(); }

    std::cout << "[DEBUG] Starting Chat Job: " << unique_msg_id << " | Prompt: " << prompt.substr(0, 20) << "..." << std::endl;

    PrepareForJob(unique_msg_id);

    worker_thread = std::thread([this, input_payload = nlohmann::json(prompt), unique_msg_id, system_prompt]() {
        this->AsyncChatWorker(input_payload, unique_msg_id, system_prompt);
        });
}

void LLMManager::StartTextureChatJob(const std::string& prompt, Texture& texture, const std::string& unique_msg_id) {
    if (worker_thread.joinable()) { worker_thread.join(); }

    if (texture.width <= 0 || texture.height <= 0) {
        std::cerr << "[Engine Error] Invalid texture dimensions provided to LLM manager." << std::endl;
        return;
    }

    int channels = (texture.nChannels > 0) ? texture.nChannels : texture.newNChannels;
    if (channels <= 0) channels = 4;

    bool should_flip = true;
    std::vector<unsigned char> raw_pixels = texture.getTextureDataAsUnsigned(should_flip);

    if (raw_pixels.empty()) {
        std::cerr << "[Engine Error] Texture data retrieval failed / vector empty." << std::endl;
        return;
    }

    std::string data_url = convert_raw_pixels_to_base64_png(raw_pixels, texture.width, texture.height, channels);
    if (data_url.empty()) {
        std::cerr << "[Engine Error] Data URL generation failed." << std::endl;
        return;
    }

    nlohmann::json input_payload = nlohmann::json::array();
    input_payload.push_back({ {"type", "text"}, {"content", prompt} });
    input_payload.push_back({ {"type", "image"}, {"data_url", data_url} });

    PrepareForJob(unique_msg_id);

    worker_thread = std::thread([this, input_payload, unique_msg_id]() {
        this->AsyncChatWorker(input_payload, unique_msg_id);
        });
}

void LLMManager::StartImageChatJob(const std::string& prompt, const std::string& image_path, const std::string& unique_msg_id) {
    if (worker_thread.joinable()) { worker_thread.join(); }

    std::cout << "[DEBUG] Starting Disk File Image Job: " << unique_msg_id << " | File: " << image_path << std::endl;

    std::ifstream file(image_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[Engine Error] Could not open image from disk path: " << image_path << std::endl;
        return;
    }

    std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(file), {});
    std::string base64_image = base64_encode(std::string(buffer.begin(), buffer.end()));

    std::string mime_type = "image/png";
    std::string ext = std::filesystem::path(image_path).extension().string();
    if (ext == ".jpg" || ext == ".jpeg") {
        mime_type = "image/jpeg";
    }
    else if (ext == ".bmp") {
        mime_type = "image/bmp";
    }

    std::string data_url = "data:" + mime_type + ";base64," + base64_image;

    nlohmann::json input_payload = nlohmann::json::array({
        {{"type", "text"}, {"content", prompt}},
        {{"type", "image"}, {"data_url", data_url}}
        });

    PrepareForJob(unique_msg_id);

    worker_thread = std::thread([this, input_payload, unique_msg_id]() {
        this->AsyncChatWorker(input_payload, unique_msg_id);
        });
}

// --- Existing GPU / Model State Methods ---

std::vector<LLMManager::ModelStatus> LLMManager::GetAvailableModelsFromServer() {
    std::vector<ModelStatus> model_list;
    httplib::Client cli(base_url, port);
    httplib::Headers headers;

    auto res = cli.Get("/api/v1/models", headers);
    if (res && res->status == 200) {
        try {
            auto parsed = nlohmann::json::parse(res->body);
            if (parsed.contains("models")) {
                for (auto& m : parsed["models"]) {
                    if (m.contains("type") && m["type"] == "llm" && m.contains("key")) {
                        ModelStatus model;
                        model.key = m["key"].get<std::string>();
                        model.is_loaded = m.contains("loaded_instances") &&
                            m["loaded_instances"].is_array() &&
                            !m["loaded_instances"].empty();
                        model_list.push_back(model);
                    }
                }
            }
        }
        catch (const std::exception& e) {
            std::cerr << "[Engine Error] Model listing parse crash: " << e.what() << "\n";
        }
    }
    return model_list;
}

bool LLMManager::IsModelLoadedOnServer(const std::string& model_key) {
    auto current_models = GetAvailableModelsFromServer();
    for (const auto& m : current_models) {
        if (m.key == model_key) return m.is_loaded;
    }
    return false;
}

bool LLMManager::UnloadModel(const std::string& model_key) {
    httplib::Client cli(base_url, port);
    nlohmann::json payload = { {"instance_id", model_key} };
    httplib::Headers headers = { { "Content-Type", "application/json" } };
    auto res = cli.Post("/api/v1/models/unload", headers, payload.dump(), "application/json");

    if (res && res->status == 200) {
        if (active_model_id == model_key) active_model_id = "";
        return true;
    }
    return false;
}

bool LLMManager::LoadModelToGPU(const std::string& model_key) {
    httplib::Client cli(base_url, port);
    cli.set_read_timeout(120, 0);
    nlohmann::json payload = {
        {"model", model_key},
        {"flash_attention", true},
        {"offload_kv_cache_to_gpu", true}
    };
    httplib::Headers headers = { { "Content-Type", "application/json" } };
    auto res = cli.Post("/api/v1/models/load", headers, payload.dump(), "application/json");
    return (res && res->status == 200);
}

void LLMManager::PrepareForJob(const std::string& unique_msg_id) {
    std::lock_guard<std::mutex> lock(state_mutex);
    active_stream_id = unique_msg_id;
    generated_text = "Thinking...";
    has_new_text = true;
    should_terminate = false; // Reset the context marker for a new query
}

// Single Consolidated Stream Processing Worker
void LLMManager::AsyncChatWorker(const nlohmann::json& input_field, std::string unique_msg_id, std::string system_prompt) {
    httplib::Client cli(base_url, port);
    cli.set_connection_timeout(3, 0);
    cli.set_read_timeout(15, 0);

    if (should_terminate) return;

    std::string current_context_id;
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        current_context_id = last_response_id;
    }

    nlohmann::json payload = {
        {"model", active_model_id},
        {"input", input_field},
        {"stream", true},
        {"max_output_tokens", 5000},
        {"temperature", 0.7}
    };
    
    if (!system_prompt.empty()) {
        payload["system_prompt"] = system_prompt;
    }

    if (!current_context_id.empty()) {
        payload["previous_response_id"] = current_context_id;
    }

    httplib::Headers headers = { { "Content-Type", "application/json" } };
    std::string stream_buffer = "";

    auto res = cli.Post("/api/v1/chat", headers, payload.dump(), "application/json",
        [&](const char* data, size_t data_len) {
            // >>> INTUITIVE INTERRUPT: Stop reading from socket if engine is shutting down <<<
            if (should_terminate) return false;

            stream_buffer.append(data, data_len);
            size_t newline_pos;

            while ((newline_pos = stream_buffer.find('\n')) != std::string::npos) {
                if (should_terminate) return false;

                std::string line = stream_buffer.substr(0, newline_pos);
                stream_buffer.erase(0, newline_pos + 1);

                while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
                if (line.empty()) continue;
                if (line.find("[DONE]") != std::string::npos) return true;
                if (line.rfind("event: ", 0) == 0) continue;
                if (line.rfind("data: ", 0) != 0) continue;

                std::string json_str = line.substr(6);
                try {
                    auto parsed = nlohmann::json::parse(json_str);

                    if (parsed.contains("type") && parsed["type"] == "chat.end") {
                        if (parsed.contains("result") && parsed["result"].contains("response_id")) {
                            std::lock_guard<std::mutex> lock(state_mutex);
                            last_response_id = parsed["result"]["response_id"].get<std::string>();
                            std::cout << "[DEBUG] Updated last_response_id: " << last_response_id << std::endl;
                        }
                    }

                    if (parsed.contains("type") && parsed["type"] == "message.delta") {
                        if (parsed.contains("content")) {
                            std::string token = parsed["content"].get<std::string>();
                            if (!token.empty() && !should_terminate) {
                                std::lock_guard<std::mutex> lock(state_mutex);
                                if (generated_text == "Thinking...") generated_text = "";
                                generated_text += token;
                                has_new_text = true;
                            }
                        }
                    }
                }
                catch (const nlohmann::json::parse_error& e) {
                    std::cerr << "[JSON Error] " << e.what() << std::endl;
                }
            }
            return true;
        }
    );

    if (!res && !should_terminate) {
        std::cerr << "[ERROR] HTTP Multimodal Request failed." << std::endl;
    }
}