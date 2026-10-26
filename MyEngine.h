//============================================================
//  MyEngine
//  © 2026 FervidFiend. All rights reserved.
//
//  File: MyEngine.h
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



#pragma once

#define USEGLEW // (USEGLEW : USEGLAD)
#define GLM_ENABLE_EXPERIMENTAL

#ifdef USEGLEW
#include <GL/glew.h>        // default opengl loader
#endif

#ifdef USEGLAD
#include <glad/glad.h>    // alternative opengl loader
#endif

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <CL/opencl.hpp>

#include <map>
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <regex>
#include <variant>
#include <numeric>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
//#include <windows.h>
//#include <tbb/tbb.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <msdfgen.h>
#include <msdfgen-ext.h>

#include <random>
#include <type_traits>

#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/map.hpp>
#include <zstd.h>  // Include the Zstandard library    // depends on windows.h

#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>

#include <sndfile.h>

#include <enet/enet.h>    // depends on windows.h

#include <opencv2/opencv.hpp> // Include OpenCV headers

#include <btBulletDynamicsCommon.h>
#include <BulletDynamics/Character/btKinematicCharacterController.h>
#include <BulletCollision/CollisionShapes/btConvexShape.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <Box2D/Box2D.h>


#include <nlohmann/json.hpp>
#include <httplib.h>
#include <shellapi.h> // Required for DragQueryFile

#ifdef DrawText
#undef DrawText
#endif

const glm::vec4 LIGHTGRAY = glm::vec4(200.0f / 255.0f, 200.0f / 255.0f, 200.0f / 255.0f, 255.0f / 255.0f);  // Light Gray
const glm::vec4 GRAY = glm::vec4(130.0f / 255.0f, 130.0f / 255.0f, 130.0f / 255.0f, 255.0f / 255.0f);  // Gray
const glm::vec4 DARKGRAY = glm::vec4(80.0f / 255.0f, 80.0f / 255.0f, 80.0f / 255.0f, 255.0f / 255.0f);     // Dark Gray
const glm::vec4 YELLOW = glm::vec4(253.0f / 255.0f, 249.0f / 255.0f, 0.0f / 255.0f, 255.0f / 255.0f);    // Yellow
const glm::vec4 GOLD = glm::vec4(255.0f / 255.0f, 203.0f / 255.0f, 0.0f / 255.0f, 255.0f / 255.0f);    // Gold
const glm::vec4 ORANGE = glm::vec4(255.0f / 255.0f, 161.0f / 255.0f, 0.0f / 255.0f, 255.0f / 255.0f);    // Orange
const glm::vec4 PINK = glm::vec4(255.0f / 255.0f, 109.0f / 255.0f, 194.0f / 255.0f, 255.0f / 255.0f);  // Pink
const glm::vec4 RED = glm::vec4(230.0f / 255.0f, 41.0f / 255.0f, 55.0f / 255.0f, 255.0f / 255.0f);    // Red
const glm::vec4 MAROON = glm::vec4(190.0f / 255.0f, 33.0f / 255.0f, 55.0f / 255.0f, 255.0f / 255.0f);    // Maroon
const glm::vec4 GREEN = glm::vec4(0.0f / 255.0f, 228.0f / 255.0f, 48.0f / 255.0f, 255.0f / 255.0f);     // Green
const glm::vec4 LIME = glm::vec4(0.0f / 255.0f, 158.0f / 255.0f, 47.0f / 255.0f, 255.0f / 255.0f);     // Lime
const glm::vec4 DARKGREEN = glm::vec4(0.0f / 255.0f, 117.0f / 255.0f, 44.0f / 255.0f, 255.0f / 255.0f);     // Dark Green
const glm::vec4 LIGHTBLUE = glm::vec4(102.0f / 255.0f, 191.0f / 255.0f, 255.0f / 255.0f, 255.0f / 255.0f);  // Light Blue
const glm::vec4 BLUE = glm::vec4(0.0f / 255.0f, 121.0f / 255.0f, 241.0f / 255.0f, 255.0f / 255.0f);    // Blue
const glm::vec4 DARKBLUE = glm::vec4(0.0f / 255.0f, 82.0f / 255.0f, 172.0f / 255.0f, 255.0f / 255.0f);     // Dark Blue
const glm::vec4 PURPLE = glm::vec4(200.0f / 255.0f, 122.0f / 255.0f, 255.0f / 255.0f, 255.0f / 255.0f);  // Purple
const glm::vec4 VIOLET = glm::vec4(135.0f / 255.0f, 60.0f / 255.0f, 190.0f / 255.0f, 255.0f / 255.0f);   // Violet
const glm::vec4 DARKPURPLE = glm::vec4(112.0f / 255.0f, 31.0f / 255.0f, 126.0f / 255.0f, 255.0f / 255.0f);   // Dark Purple
const glm::vec4 BEIGE = glm::vec4(211.0f / 255.0f, 176.0f / 255.0f, 131.0f / 255.0f, 255.0f / 255.0f);  // Beige
const glm::vec4 BROWN = glm::vec4(127.0f / 255.0f, 106.0f / 255.0f, 79.0f / 255.0f, 255.0f / 255.0f);   // Brown
const glm::vec4 DARKBROWN = glm::vec4(76.0f / 255.0f, 63.0f / 255.0f, 47.0f / 255.0f, 255.0f / 255.0f);     // Dark Brown

const glm::vec4 WHITE = glm::vec4(255.0f / 255.0f, 255.0f / 255.0f, 255.0f / 255.0f, 255.0f / 255.0f);  // White
const glm::vec4 BLACK = glm::vec4(0.0f / 255.0f, 0.0f / 255.0f, 0.0f / 255.0f, 255.0f / 255.0f);        // Black
const glm::vec4 CLEAR = glm::vec4(0.0f / 255.0f, 0.0f / 255.0f, 0.0f / 255.0f, 0.0f / 255.0f);          // Clear (Transparent)
const glm::vec4 BLANK = glm::vec4(0.0f / 255.0f, 0.0f / 255.0f, 0.0f / 255.0f, 0.0f / 255.0f);          // Blank (Transparent)
const glm::vec4 MAGENTA = glm::vec4(255.0f / 255.0f, 0.0f / 255.0f, 255.0f / 255.0f, 255.0f / 255.0f);    // Magenta
const glm::vec4 RAYWHITE = glm::vec4(245.0f / 255.0f, 245.0f / 255.0f, 245.0f / 255.0f, 255.0f / 255.0f);  // White (raylib logo color)

#define PI double(3.1415926535897932384626433832795)
#define TWO_PI double(6.283185307179586476925286766559)


template <typename T>
struct WrapValue {
    T value; // The current value
    T min;   // The minimum value
    T max;   // The maximum value

    // Constructor to initialize the min and max values, with default value set to min
    WrapValue(T min, T max) : value(min), min(min), max(max) {}

    // Method to wrap the value within the range [min, max]
    void wrap() {
        if (value > max) {
            value = min + (value - max - 1);
        }
        else if (value < min) {
            value = max - (min - value - 1);
        }
    }

    // Overload equality operator ==
    bool operator==(const WrapValue& other) const {
        return value == other.value && min == other.min && max == other.max;
    }

    // Overload equality operator ==
    bool operator==(const T& other) const {
        return value == other;
    }

    // Overload operator +=
    WrapValue& operator+=(const T& otherValue) {
        value += otherValue;
        wrap();
        return *this;
    }

    // Overload operator -=
    WrapValue& operator-=(const T& otherValue) {
        value -= otherValue;
        wrap();
        return *this;
    }

    // Overload operator *=
    WrapValue& operator*=(const T& otherValue) {
        value *= otherValue;
        wrap();
        return *this;
    }

    // Overload operator /=
    WrapValue& operator/=(const T& otherValue) {
        if (otherValue != 0) {
            value /= otherValue;
            wrap();
        }
        return *this;
    }

    // Overload operator +
    WrapValue operator+(const T& otherValue) const {
        WrapValue result = *this;
        result += otherValue;
        return result;
    }

    // Overload operator -
    WrapValue operator-(const T& otherValue) const {
        WrapValue result = *this;
        result -= otherValue;
        return result;
    }

    // Overload operator *
    WrapValue operator*(const T& otherValue) const {
        WrapValue result = *this;
        result *= otherValue;
        return result;
    }

    // Overload operator /
    WrapValue operator/(const T& otherValue) const {
        WrapValue result = *this;
        result /= otherValue;
        return result;
    }

    // Overload assignment operator to directly assign a value
    WrapValue& operator=(const T& newValue) {
        value = newValue;
        wrap();
        return *this;
    }
};

struct Rect {
    glm::vec2 position = glm::vec2(0);
    glm::vec2 size = glm::vec2(0);

    Rect();
    Rect(glm::vec2 position, glm::vec2 size);
    Rect(float x, float y, float sizeX, float sizeY);

    glm::vec2 GetCenter() const;
    glm::vec2 GetBottomLeft();
    glm::vec2 GetBottomRight();
    glm::vec2 GetTopLeft();
    glm::vec2 GetTopRight();

    bool operator==(const Rect& other) const {
        return position == other.position &&
            size == other.size;
    }

    bool operator!=(const Rect& other) const {
        return !(*this == other);
    }
};

//struct Box {
//    glm::vec3 position;
//    glm::vec3 size;
//
//    Box();
//    Box(glm::vec3 position, glm::vec3 size);
//};




// forward declarations for some things
struct Shader;
class Texture;
class Image;

class SoundEffect;
class MusicSound;


class OpenAL {
public:
    ALCdevice* device = nullptr;
    ALCcontext* context = nullptr;
    std::atomic<bool> openAlLoaded = false;
    std::atomic<bool> closing = false;

    std::vector<SoundEffect*> loadedSounds;
    std::vector<MusicSound*> loadedMusicSounds;
    std::vector<ALuint> loadedFilters;

    glm::vec3 listenerPosition;

    OpenAL();
    ~OpenAL();

    void SetupOpenAL();

    void CloseOpenAL();

    void setListenerProperties(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up);
};



// Camera2D class declaration
class Camera2D {
public:
    glm::vec2 offset;         // Camera offset (displacement from target)
    glm::vec2 target;         // Camera target (rotation and zoom origin)
    float rotation;           // Camera rotation in degrees
    glm::vec2 zoom;           // Camera zoom (scaling), should be 1.0f by default
    bool rotated = false;

    glm::vec2 currentViewportSize;
    //glm::vec2 lastViewportSize;

    Camera2D(glm::vec2 offset = glm::vec2(0.0f, 0.0f), glm::vec2 target = glm::vec2(0.0f, 0.0f), float rotation = 0.0f, glm::vec2 zoom = glm::vec2(1.0f, 1.0f));
    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix() const;
    glm::mat4 GetInverseViewMatrix() const;
    glm::mat4 GetInverseProjectionMatrix() const;

    void CenterViewportInWindow();
};


class TopDown2DCameraController {
public:
    Camera2D camera;

    TopDown2DCameraController();

    void Update(int button);
};


// Camera3D class declaration
class Camera3D {
public:
    // doesnt change much
    glm::vec3 offset;
    glm::vec3 worldUp;
    float fovy;
    float nearClip;
    float farClip;

    // changes often
    glm::vec3 position;
    glm::vec3 target;
    glm::vec3 right;
    glm::vec3 forward;
    glm::vec3 up;

    Camera3D(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 offset = glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f), float fovy = 90.0f, float nearClip = 1e-2f, float farClip = 1e5f);

    glm::mat4 GetViewMatrix() const;

    glm::mat4 GetProjectionMatrix() const;

    void update(glm::quat orientation, glm::vec3 newPos);
};



// ===================== RandomDevice =====================
class RandomDevice {
public:
    static std::random_device rd;
    static std::mt19937_64 gen;

    RandomDevice() = default;
    static void reseed(uint64_t seed);
};

// ===================== GLM Vector Trait =====================
template<typename T>
struct is_glm_vec : std::false_type {};

template<> struct is_glm_vec<glm::vec2> : std::true_type {};
template<> struct is_glm_vec<glm::vec3> : std::true_type {};
template<> struct is_glm_vec<glm::vec4> : std::true_type {};

template<typename T>
inline constexpr bool is_glm_vec_v = is_glm_vec<T>::value;

// ===================== Scalar RNG =====================
template<typename T, typename Enable = void>
class RNG;

// Specialization for arithmetic types (int, float, double)
template<typename T>
class RNG<T, std::enable_if_t<std::is_arithmetic_v<T>>> : public RandomDevice {
public:
    static T getRange(T min, T max) {
        if constexpr (std::is_integral_v<T>) {
            std::uniform_int_distribution<T> dist(min, max);
            return dist(gen);
        }
        else {
            std::uniform_real_distribution<T> dist(min, max);
            return dist(gen);
        }
    }

    static T getNormalRange(T mean, T stddev) {
        std::normal_distribution<double> dist(
            static_cast<double>(mean),
            static_cast<double>(stddev)
        );
        double result = dist(gen);

        if constexpr (std::is_integral_v<T>)
            return static_cast<T>(std::round(result));
        else
            return static_cast<T>(result);
    }
};

// ===================== GLM Vector RNG =====================
template<typename T>
class RNG<T, std::enable_if_t<is_glm_vec_v<T>>> : public RandomDevice {
public:
    using Scalar = typename T::value_type;

    static T getRange(const T& min, const T& max) {
        T result;
        for (int i = 0; i < result.length(); ++i) {
            Scalar mi = std::min(min[i], max[i]);
            Scalar ma = std::max(min[i], max[i]);
            result[i] = RNG<Scalar>::getRange(mi, ma);
        }
        return result;
    }

    static T getNormalRange(const T& mean, const T& stddev) {
        T result;
        for (int i = 0; i < result.length(); ++i) {
            result[i] = RNG<Scalar>::getNormalRange(mean[i], stddev[i]);
        }
        return result;
    }
};


template <typename T>
// Ensure that percent is a fraction (e.g., 0.1 for 10%)
T getRoughly(T value, double percent) {
    double randomFactor = RNG<double>::getRange(-value * percent, value * percent);

    // Convert result back to type T if necessary
    return value + static_cast<T>(randomFactor);
}


class Clock {
public:
    double rawStartTime;
    double startTime;

    Clock();

    double getRawElapsedTime();
    double getElapsedTime();

    void restart();
};



class PerfLogger {
private:
    std::vector<double> times;
    double total = 0.0;

public:
    double GetFps();
    void Update(double dt);

    void PrintStatistics();
};

// external call to extras::perfLogger.GetFps();
double GetFps();


static void window_refresh_callback(GLFWwindow* window);
static void window_pos_callback(GLFWwindow* window, int xpos, int ypos);
//static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
static void window_size_callback(GLFWwindow* window, int width, int height);
static void window_focus_callback(GLFWwindow* window, int focused);
static void mouse_wheel_callback(GLFWwindow* window, double xoffset, double yoffset);
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
static void char_callback(GLFWwindow* window, unsigned int codepoint);

// Initialize GLFW
void SetupGLFW();
#ifdef USEGLEW
// Initialize GLEW
void SetupGLEW();
#endif
#ifdef USEGLAD
// Initialize GLAD
void SetupGLAD();
#endif

//void SetupCL();
void InitLib();
void printActiveBufferSpecs();
GLFWwindow* InitWindow(int width, int height, const char* title);
void CloseWindow();
void EnableVsync();
void DisableVsync();
// Get true elapsed time in seconds since InitWindow()
double GetRawTime();
// Get elapsed time in seconds since InitWindow()
double GetTime();
double GetRawFrameTime();
double GetFrameTime();
bool WindowSizeChanged();
glm::vec2 GetCurrentWindowSize();
void SetTargetFPS(int fps);
// true excludes modal move/resize time; false includes it in the next frame delta.
void SetPauseOnWindowDrag(bool pause);
void LimitFPS();
void SetCurrentActiveWindow(GLFWwindow* window);
void CheckActiveWindowSet();
void DumpCurrentGlDataToConsole();

bool CheckGLErrors();

// Check collision between two circles
bool CheckCollisionCircles(glm::vec2 center1, float radius1, glm::vec2 center2, float radius2);

// Function to check for intersection between two line segments
bool CheckLineIntersection(const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& q1, const glm::vec2& q2, glm::vec2& outIntersection, float& outT);

// Function to check collision between a line segment and a rectangle
bool CheckCollisionLineRect(const glm::vec2& start, const glm::vec2& end, const Rect& rect, glm::vec2& intersectionPoint);

// Function to check collision between a line segment and a circle
bool CheckCollisionLineCircle(const glm::vec2& start, const glm::vec2& end, const glm::vec2& circleCenter, float radius);

// Function to check if a line segment intersects with a triangle
bool CheckCollisionLineTriangle(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& triA, const glm::vec3& triB, const glm::vec3& triC, glm::vec3& intersectionPoint);

// Function to check collision between two triangles
bool CheckTriangleTriangleCollision(const glm::vec3& A0, const glm::vec3& A1, const glm::vec3& A2, const glm::vec3& B0, const glm::vec3& B1, const glm::vec3& B2);

// Function to check collision between two rectangles
bool CheckCollisionRec(const Rect& rec1, const Rect& rec2, glm::vec2* overlap = nullptr);

bool CheckPointInRectangle(glm::vec2 point, Rect rect);

bool CheckPointInCircle(glm::vec2 point, glm::vec2 origin, float radius);

// Check if a key has been pressed once
bool IsKeyPressed(int key);

// Check if a key has been pressed again (Only PLATFORM_DESKTOP)
bool IsKeyPressedRepeat(int key);

// Check if a key is being pressed
bool IsKeyDown(int key);

// Check if a key has been released once
bool IsKeyReleased(int key);

// Check if a key is NOT being pressed
bool IsKeyUp(int key);





// Check if a gamepad is available
bool IsGamepadAvailable(int gamepad);

// Get gamepad internal name id
const char* GetGamepadName(int gamepad);

// Check if a gamepad button has been pressed once
bool IsGamepadButtonPressed(int gamepad, int button);

// Check if a gamepad button is being pressed
bool IsGamepadButtonDown(int gamepad, int button);

// Check if a gamepad button has been released once
bool IsGamepadButtonReleased(int gamepad, int button);

// Check if a gamepad button is not being pressed
bool IsGamepadButtonUp(int gamepad, int button);

// Get the last gamepad button pressed (not provided by GLFW directly)
int GetGamepadButtonPressed(void);

// Get gamepad axis count for a gamepad
int GetGamepadAxisCount(int gamepad);

// Get axis movement value for a gamepad axis
float GetGamepadAxisMovement(int gamepad, int axis, float deadzone = 0.1f);

// Set internal gamepad mappings (uses SDL_GameControllerDB format)
int SetGamepadMappings(const char* mappings);





// Check if a mouse button is being pressed
bool IsMouseButtonDown(int button);

// Check if a mouse button is NOT being pressed
bool IsMouseButtonUp(int button);

// Check if a mouse button has been pressed once
bool IsMouseButtonPressed(int button);

// Check if a mouse button has been released once
bool IsMouseButtonReleased(int button);

// Get mouse position XY in range 0.0-1.0 (can go above and below 0 and 1 because of mouse out of window boundary
glm::vec2 GetMousePosition();

// Returns normalized window coordinates in NDC range [-1.0, 1.0]
glm::vec2 GetMousePositionNDC();

// Convert Mouse position XY range to 0.0-CurrentViewportSize
glm::vec2 GetMouseToScreen(glm::vec2 mousePos, Camera2D* camera = nullptr);

// Get the ratio of viewport size to window size
glm::vec2 GetViewportScale(Camera2D* camera = nullptr);

// Get the world space position for a 2d camera screen space position
glm::vec2 GetScreenToWorld2D(glm::vec2 screenPos, Camera2D* camera = nullptr);

// Get the screen space position for a 2d world space position    
glm::vec2 GetWorldToScreen2D(glm::vec2 position, Camera2D* camera);

// Get the world space position for a 3d camera screen space position
glm::vec2 GetScreenToWorld3D(glm::vec2 screenPos, Camera3D* camera = nullptr);

// Get the screen space position for a 3d world space position    
glm::vec2 GetWorldToScreen3D(glm::vec3 position, Camera3D* camera);

// Get mouse delta between frames in range 0.0-1.0 (screen space)
glm::vec2 GetMouseDelta();

// Get mouse wheel movement for X or Y, whichever is larger
float GetMouseWheelMove();

// Get mouse wheel movement for both X and Y
glm::vec2 GetMouseWheelMoveV();

// GL and CL shared context setup (platform-specific, assuming Windows here for example)
//cl::Context createSharedContext(cl::Device& device);

// Initialize OpenGL texture and share with OpenCL
//GLuint createSharedTexture(int width, int height);

cl::Kernel setupKernel(const cl::Context& context, const std::string& source, const std::string& sourceFunctionName);


struct Candle {
    float open;
    float high;
    float low;
    float close;
    float volume;

    Candle() {}
    Candle(float open, float high, float low, float close, float volume) : open(open), high(high), low(low), close(close), volume(volume) {}
};

// expose default shaders
extern const char* DefaultVertexShaderSource;
extern const char* DefaultChartVertexShaderSource;
extern const char* DefaultLineChartFragmentShaderSource;
extern const char* DefaultBarChartFragmentShaderSource;


// Function to compile shaders and create a shader program
GLuint loadShaderFile(const char* source, int type);
GLuint LinkShaders(std::vector<GLuint> shaders);
GLuint LinkComputeShader(GLuint computeShader);
void LoadDefaultShaders();

const char* getGLErrorString(GLenum error);


bool WindowShouldClose();


GLuint getCurrentActiveProgram();

void printActiveUniforms(GLuint program);


// Set background color (framebuffer clear color)
void ClearBackground(glm::vec4 color);

void StartScissor(Rect rect, Camera2D* cam = nullptr);

void StopScissor();

// Setup canvas (framebuffer) to start drawing
void BeginDrawing(void);

// 
void PollCustomEvents();

// End canvas drawing and swap buffers (double buffering)
void EndDrawing(void);

//   !!! Currently must be called after BeginShaderMode() !!!
void BeginMode2D(Camera2D& _camera);

void EndMode2D();

//   !!! Currently must be called after BeginShaderMode() !!!
void BeginMode3D(Camera3D& _camera);

void EndMode3D();

// Begin custom shader drawing   !!! Currently must be called before any changes to the mode 2d or 3d !!!
void BeginShaderMode(Shader& shader);

// End custom shader drawing (use default shader)
void EndShaderMode(void);

enum DepthModes {
    UNINITIALIZED,
    NEVER,
    LESS,
    EQUAL,
    LEQUAL,
    GREATER,
    NOTEQUAL,
    GEQUAL,
    ALWAYS
};

void SwitchDepthMode(DepthModes depthMode);
void BeginDepthMode(DepthModes depthMode, bool enableDepth = true);
void EndDepthMode(bool disableDepth = false);
void BeginInverseZ();
void EndInverseZ();
/*
USEFUL OpenGL Blend Modes (Photoshop equivalents + Use Cases)

1) Add / Linear Dodge (Glow)   BeginBlendingMode(GL_ONE, GL_ONE)   Photoshop: Linear Dodge (Add)   Use: Fire, sparks, explosions, glow effects.
2) Normal (Transparency)       BeginBlendingMode(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)   Photoshop: Normal   Use: UI, sprites, text, decals.
3) Premultiplied Alpha         BeginBlendingMode(GL_ONE, GL_ONE_MINUS_SRC_ALPHA)   Photoshop: Normal (Premultiplied)   Use: Smoke, fog, soft particles, smooth edges.
4) Additive Alpha (Soft Glow)  BeginBlendingMode(GL_SRC_ALPHA, GL_ONE)   Photoshop: Linear Dodge (Add) + opacity   Use: Lens flares, magic glows, soft light effects.
5) Screen (Lighten)            BeginBlendingMode(GL_ONE, GL_ONE_MINUS_SRC_COLOR)   Photoshop: Screen   Use: Bloom, haze, soft light overlays.
6) Multiply (Darken)           BeginBlendingMode(GL_DST_COLOR, GL_ZERO)   Photoshop: Multiply   Use: Shadows, dirt, grime, dark overlays.
7) Overlay (Approximation)     BeginBlendingMode(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR)   Photoshop: Overlay (approximation)   Use: Contrast boost, stylized lighting.
8) Subtract                    BeginBlendingMode(GL_ZERO, GL_ONE_MINUS_SRC_COLOR)   Photoshop: Subtract   Use: Negative effects, sci-fi distortion, void effects.
*/
void BeginBlendingMode(GLint srcAlpha, GLint dstAlpha);
void EndBlendingMode();

using UniformValue = std::variant<uint, int, glm::ivec2, glm::ivec3, glm::ivec4, float, glm::vec2, glm::vec3, glm::vec4, double, glm::dvec2, glm::dvec3, glm::dvec4, glm::mat2, glm::mat3, glm::mat4>;

struct UniformComparator {
    const UniformValue& value;

    template<typename T>
    bool operator()(const T& v) const {
        // Your comparison logic here, assuming equality operators are defined for glm types
        const T* stored = std::get_if<T>(&value);
        return stored && (*stored == v);
    }
};

bool isUniformValueEqual(const UniformValue& uValue, const UniformValue& value);

void bindVAO(GLuint vao);

enum class GL_AccessTypes { READ_ONLY, WRITE_ONLY, READ_WRITE };

struct Shader {
    std::string name = "";
    GLuint vertexShader = -1;
    GLuint geometryShader = -1;
    GLuint fragmentShader = -1;
    GLuint computeShader = -1;
    GLuint shader = -1; // is actually the program | should probs be renamed at some point

    // texture unit, textureID
    std::unordered_map<GLuint, GLuint> boundTextureUnits;
    // image unit
    std::unordered_set<GLuint> boundImageUnits;

    std::unordered_map<std::string, GLint> loadedUniformLocations;

    std::unordered_map<std::string, UniformValue> loadedUniformValues;

    enum Types {
        VERT,
        GEO,
        FRAG,
        COMPUTE,
    };

    Shader();
    Shader(std::string _name);
    Shader(std::string _name, const char* fragSource);
    Shader(std::string _name, const char* vertSource, const char* fragSource);

    bool loadFromFile(const char* source, int type);

    bool loadFromFile(const char* vertSource, const char* fragSource);
    bool loadFromFile(const char* vertSource, const char* geometrySource, const char* fragSource);

    void end();

    GLint SafeGetUniformLocation(std::string uniform);

    GLint passedUniformChecks(std::string uniform, UniformValue value);

    void setUniform(std::string uniform, Texture& texture, int unit = 0);

    void setUniform(std::string uniform, uint value);

    void setUniform(std::string uniform, int value);
    void setUniform(std::string uniform, glm::ivec2 value);
    void setUniform(std::string uniform, glm::ivec3 value);
    void setUniform(std::string uniform, glm::ivec4 value);

    void setUniform(std::string uniform, float value);
    void setUniform(std::string uniform, glm::vec2 value);
    void setUniform(std::string uniform, glm::vec3 value);
    void setUniform(std::string uniform, glm::vec4 value);

    void setUniform(std::string uniform, double value);
    void setUniform(std::string uniform, glm::dvec2 value);
    void setUniform(std::string uniform, glm::dvec3 value);
    void setUniform(std::string uniform, glm::dvec4 value);

    void setUniform(std::string uniform, glm::mat4 value);

    void setUniform(std::string uniform, float* values, size_t count);
    void setUniform(std::string uniform, glm::vec2* values, size_t count);

    // Warning! expensive operations
    void setUniform(std::string uniform, std::vector<float>& values);
    void setUniform(std::string uniform, std::vector<glm::vec2>& values);
    void setUniform(std::string uniform, std::vector<glm::vec3>& values);

    void bindImage(Texture& texture, int unit, GL_AccessTypes access);
};


void premultiplyAlphas(unsigned char* data, int width, int height, int channels);

std::string base64_encode(const std::string& in);
std::string convert_raw_pixels_to_base64_png(const std::vector<unsigned char>& pixels, int width, int height, int channels);


class Image {
public:
    GLFWimage image;
    std::vector<unsigned char> imageData;

    // Copy assignment operator
    Image& operator=(const Image& other);

    Image();
    Image(const std::string& path, bool flipped = true);
    Image(int width, int height);

    cv::Mat ToCVImage();
    void loadFromFile(const std::string& path, bool flipped = true, bool _preMultiplyAlphas = false);
    void saveToFile(const std::string& path);


};

void SaveScreenshot(int width, int height, const std::string& path);

enum class GL_FilterModes { NEAREST, LINEAR, MIPMAP, MIPMAP_NEAREST };
enum class GL_WrapModes { CLAMP, CLAMP_TO_EDGE, CLAMP_TO_BORDER, REPEAT, MIRRORED_REPEAT };

enum class GL_InternalFormats {
    // int-uint
    R8, RG8, RGB8, RGBA8, // 1 byte per channel
    R16, RG16, RGB16, RGBA16, // 2 bytes per channel
    // floats
    R16F, RG16F, RGB16F, RGBA16F, // 2 bytes per channel
    R32F, RG32F, RGB32F, RGBA32F, // 4 bytes per channel
    // depth
    DEPTH_COMPONENT24, DEPTH_COMPONENT32,
    DEPTH_COMPONENT32F,
};

enum class GL_ExternalFormats { RED, RG, RGB, RGBA, DEPTH_COMPONENT };

enum class GL_DataFormats {
    BYTE, UNSIGNED_BYTE, // 1 byte
    SHORT, UNSIGNED_SHORT, // 2 bytes
    INT, UNSIGNED_INT, // 4 bytes
    HALF_FLOAT, // 2 bytes
    FLOAT,
}; // 4 bytes

// MUST BE CREATED INSIDE A FUNCTION/MUST HAVE LIFETIME
class Texture {
public:
    GLuint textureID;
    // TODO: implement bindless textures
    //GLuint64 handle;
    int width = -1;
    int height = -1;
    GL_FilterModes filterMode;
    GL_WrapModes wrapMode;
    GL_InternalFormats newInternalGLFormat;
    GL_ExternalFormats newExternalGLFormat;
    GL_DataFormats dataFormat;
    int newNChannels;

    int unit = 0;

    GLint internalGLFormat; // 
    GLint externalGLFormat; // base texture format
    int nChannels;

    //Image image;
    std::vector<unsigned char> textureData;

    // Copy assignment operator
    Texture& operator=(const Texture& other);

    Texture();
    // useMipmaps builds alpha-aware mip levels (SVG and PNG alike).
    Texture(const std::string& path, bool flipped = true, bool _preMultiplyAlphas = true, bool useMipmaps = false, float svgRasterScale = 4.0f);
    Texture(int width, int height, GL_FilterModes filterMode = GL_FilterModes::LINEAR, GL_WrapModes wrapMode = GL_WrapModes::CLAMP_TO_EDGE, GL_InternalFormats newInternalGLFormat = GL_InternalFormats::RGBA32F, GL_ExternalFormats newExternalGLFormat = GL_ExternalFormats::RGBA, GL_DataFormats dataFormat = GL_DataFormats::FLOAT, int newNChannels = 4);
    Texture(int width, int height, glm::vec4 color);
    Texture(int width, int height, std::vector<unsigned char> _textureData);
    ~Texture();

    void bind(int _unit = 0);
    void unbind();
    void bindForImageAccess(int imageUnit, GL_AccessTypes access);
    void unbindFromImageAccess();

    void clear();

    void loadFromFile(const std::string& path, bool flipped = true, bool _preMultiplyAlphas = false, bool useMipmaps = false, float svgRasterScale = 4.0f);
    void saveToFile(const std::string& path);
    void setupTexture(int width, int height, GL_FilterModes filterMode, GL_WrapModes wrapMode, GL_InternalFormats newInternalGLFormat, GL_ExternalFormats newExternalGLFormat, GL_DataFormats dataFormat, int newNChannels);
    void updateSamplerDetails(GL_FilterModes filterMode, GL_WrapModes wrapMode);
    bool updateSafetyCheck(std::vector<float>& data);
    void updateFormat(GL_InternalFormats newInternalGLFormat, GL_ExternalFormats newExternalGLFormat, GL_DataFormats dataFormat, int newNChannels);
    void update(unsigned char* data);
    void update(std::vector<float>& data);
    void update(std::vector<glm::vec4>& data);
    void update(glm::vec4 data);
    std::vector<float> getTextureDataAsFloats() const;
    std::vector<unsigned char> FlipData(std::vector<unsigned char> pixels);
    std::vector<unsigned char> getTextureDataAsUnsigned(bool flipped = false);
    void generateMipmap();
    // Builds mip levels from textureData, weighting colour by alpha so
    // transparent pixels do not darken edges; kept by updateSamplerDetails.
    void generateAlphaAwareMipmaps(bool premultiplied);
    bool alphaAwareMipmaps = false;
    // TODO: implement bindless textures
    //void makeTextureHandleResident();
    //void makeTextureHandleNonResident();
};

class FrameBuffer {
public:
    GLuint fbo;      // Framebuffer object
    Texture* texture = nullptr;  // Texture attached to the framebuffer
    GLuint rbo;      // Renderbuffer object for depth and stencil
    int width, height;  // Dimensions of the framebuffer
    glm::vec4 color = WHITE;

    int prevViewportWidth;
    int prevViewportHeight;

    glm::vec4 customRect;

    FrameBuffer(int width, int height, GL_FilterModes filterMode = GL_FilterModes::LINEAR, GL_WrapModes wrapMode = GL_WrapModes::CLAMP_TO_EDGE, GL_InternalFormats newInternalGLFormat = GL_InternalFormats::RGBA32F, GL_ExternalFormats newExternalGLFormat = GL_ExternalFormats::RGBA, GL_DataFormats dataFormat = GL_DataFormats::FLOAT, int newNChannels = 4);
    FrameBuffer(const std::string& filepath);
    //FrameBuffer(Texture _texture);
    ~FrameBuffer();

    // Custom Move Constructor
    FrameBuffer(FrameBuffer&& other) noexcept;
    // Custom Move Assignment Operator
    FrameBuffer& operator=(FrameBuffer&& other) noexcept;

    void startDrawing();

    void stopDrawing();

    void drawFramebufferToScreen();
    void drawFramebufferToScreen(Rect rect);

    std::vector<float> getTextureDataAsFloatsDirect();

private:
    void setupFramebuffer();
    void setupDepthFramebuffer();

    void cleanup();
};

// Texture Atlas
class AnimatedTexture {
public:
    Texture texture;
    int frameWidth;
    int frameHeight;
    int nFramesX;
    int nFramesY;
    int nTotalFrames;

    int frame = 0;        // current frame index in the entire atlas
    float timePassed = 0;

    float timePerFrame;
    int loopbackFrame;
    int endFrame;
    int step;

    AnimatedTexture(glm::vec2 frameSize, std::string texturePath);
    AnimatedTexture(int nFramesX, int nFramesY, std::string texturePath);

    void Update(double dt);
    void UpdateAnimationFrames(float _timePerFrame, int _startFrame, int _loopbackFrame, int _endFrame, int _step);
    void Draw(Rect rect, int frame);
};

//FrameBuffer loadImageToTexture(const char* filepath);

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec3 color;

    bool operator==(const Vertex& other) const
    {
        return position == other.position &&
            normal == other.normal &&
            texCoord == other.texCoord &&
            color == other.color;
    }
};

// Triangle struct to store vertex positions
struct Triangle {
    Vertex vertexA;
    Vertex vertexB;
    Vertex vertexC;
};

struct MaterialData {
    std::string materialName = "";
    std::vector<Vertex> vertices;
    GLuint VBO = 0;
    GLuint VAO = 0;
    Texture* colorTexture = nullptr;
    glm::vec3 diffuseColor;
};

class Model {
public:
    glm::vec3 position = { 0.0, 0.0, 0.0 };
    glm::vec3 scale = { 1.0, 1.0, 1.0 };
    glm::quat orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec4 color = WHITE; // Default lighting color

    std::vector<MaterialData> allMateralsData;
    std::unordered_map<std::string, int> storedPositionPairanymore;

    std::vector<Triangle> triangles;

    float inflation = 0.0f;

    // Default Constructor
    Model();

    // Constructor to load from obj
    Model(std::string objFile);

    Model(const std::vector<Triangle>& tris, Texture& texture);

    //Model(const char* objFile, const char* textureFile);

    // Destructor to clean up resources
    ~Model();

    // Custom Copy Constructor
    Model(const Model& other);
    // Custom Move Constructor
    Model(Model&& other) noexcept;
    // Custom Move Assignment Operator
    Model& operator=(Model&& other) noexcept;

    void LoadModelDataFromOBJ(const std::string& filename, bool useModelNormals = true, float thresholdAngle = 45.0f);
    void LoadModelTexturesFromMTL(const std::string& basePath, const std::string& filename);

    void LoadFromTriangles(const std::vector<Triangle>& tris, Texture& texture);
    void UpdateFromTriangles(const std::vector<Triangle>& tris);
    void ClearGeometry();

    void loadVAOVBO();

    void updateSamplerDetails(GL_FilterModes filterMode, GL_WrapModes wrapMode);

    void Draw(bool noColor = false);

    void CleanUp();
};

double sin01(double x);

double Clamp(double value, double min, double max);

double Round(double value, int decimals);

bool IsValidInteger(const std::string& str);

bool IsValidFloat(const std::string& str);

int safe_stoi(const std::string& str, int default_value);

float safe_stof(const std::string& str, float default_value);

glm::vec2 safeNormalize(glm::vec2 v);

glm::vec3 safeNormalize(glm::vec3 v);

glm::vec4 getRainbow(float t);

// Get current screen width
int GetScreenWidth(void);

// Get current screen height
int GetScreenHeight(void);

glm::vec2 WASDtoVec2();

class BaseController {
public:
    glm::quat orientation = glm::quat(1, 0, 0, 0); // identity

    glm::vec2 sensitivity = glm::vec2(0.1f);

    glm::vec3 positionBeforeAnyMovement = glm::vec3(0, 2, 0);
    glm::vec3 positionBeforeInputMovement = glm::vec3(0, 2, 0);
    glm::vec3 position = glm::vec3(0, 2, 0);
    glm::vec3 velocity = glm::vec3(0, 0, 0);
    float moveSpeed = 12.0f;
    float verticalSpeed = 12.0f;

    void update(float deltaTime, Camera3D& camera);
};

class ThirdPersonController {
public:
    Camera3D camera;
    BaseController baseController;

    // TODO: Reimplement ThirdPersonController

    ThirdPersonController();

    void update(float deltaTime);
};

class FirstPersonController {
public:
    Camera3D camera;
    BaseController baseController;

    FirstPersonController();

    void update(float deltaTime);

    glm::vec3 getViewPos();
};

// Setup VAO, VBO and EBO
extern GLuint global_rectangle_vao, global_rectangle_vbo, global_rectangle_ebo;
extern GLuint global_circle_vao, global_circle_vbo;
extern GLuint global_line_vao, global_line_vbo;

extern GLuint global_triangle_vao, global_triangle_vbo;
extern std::vector<Vertex> global_triangle_vertices; // pre-allocate 3 vertices

extern GLuint global_cube_vao, global_cube_vbo;
extern std::vector<Vertex> global_cube_vertices;

// Vertex structure matching location 0: Pos, 1: Normal, 2: TexCoords, 3: Color (optional)
struct SphereVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
};

// Global handles for your sphere VAO/VBO/EBO
extern GLuint global_sphere_vao, global_sphere_vbo, global_sphere_ebo, global_sphere_instance_vbo;
extern size_t global_sphere_index_count;

extern GLuint global_wireframe_cube_vao, global_wireframe_cube_vbo, global_wireframe_cube_ebo;
extern std::vector<glm::vec3> global_wireframe_cube_vertices;
extern std::vector<GLuint> global_wireframe_cube_indices;

extern GLuint global_skybox_cube_vao, global_skybox_cube_vbo, global_skybox_cube_ebo;
extern std::vector<Vertex> global_skybox_cube_vertices;
extern std::vector<GLuint> global_skybox_cube_indices;

void SetupGlobalGLElements();

void CleanupGlobalGLElements();

void SetUniforms2D();

void SetUniforms3D();

int calculateCircleSegments(float radius);

// 2D
// Draw a color-filled circle
void DrawCircle(float centerX, float centerY, float radius, glm::vec4 color, int num_segments = 0);
// Draw a wireframe circle
void DrawCircleWires(float centerX, float centerY, float radius, glm::vec4 color, int num_segments = 0);
// Draw a color-filled rectangle
void DrawRectangle(Rect rect, glm::vec4 color);
// Draw a color-filled rotated rectangle
void DrawRectangle(Rect rect, float angle, glm::vec4 color);
// Draw a texture-filled rectangle
void DrawRectangle(Rect rect, Texture& texture, glm::vec4* customRect = nullptr);
// Draw a texture-filled rotated rectangle
void DrawRectangle(Rect rect, float angle, Texture& texture, glm::vec4* customRect = nullptr);
// Draw a tinted texture-filled rectangle
void DrawRectangle(Rect rect, glm::vec4 color, Texture& texture, glm::vec4* customRect = nullptr);
// Draw a tinted texture-filled rotated rectangle
void DrawRectangle(Rect rect, float angle, glm::vec4 color, Texture& texture, bool mix = false, glm::vec4* customRect = nullptr);
// Draw a texture on the unit square mapped by an arbitrary model matrix (rotation, skew, scale).
void DrawTexturedQuad(const glm::mat4& model, Texture& texture, glm::vec4 color = glm::vec4(1.0f));
// Draw a wireframe rectangle
void DrawRectangleWires(Rect rect, glm::vec4 color);
// Draw a color-filled triangle
void DrawTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, glm::vec4 color);
// Draw a line
void DrawLine(float startX, float startY, float endX, float endY, glm::vec4 color);
void DrawLine(glm::vec2 start, glm::vec2 end, glm::vec4 color);
// Draw a color-filled line with thickness
void DrawLine(float startX, float startY, float endX, float endY, glm::vec4 color, float thickness, float dashLength = 1.0f, float gapLength = 0.0f);
// Batch draw many color-filled lines
void BatchDrawLines(std::vector<glm::vec2> points, glm::vec4 color, float thickness = 1.0f, float dashLength = 1.0f, float gapLength = 0.0f);

// 3D 
// Draw a plane XZ
void DrawPlane(glm::vec3 centerPos, glm::vec2 size, glm::vec4 color);
// Draw a Textured plane XZ
void DrawPlane(glm::vec3 centerPos, glm::vec2 size, Texture& texture, glm::vec4* customRect = nullptr);
// Draw a plane XZ wires
void DrawPlaneWires(glm::vec3 centerPos, glm::vec2 size, glm::vec4 color);
// Draw cube
void DrawCube(glm::vec3 position, glm::vec3 size, glm::vec4 color);
// Draw Textured Cube
void DrawCube(glm::vec3 position, glm::vec3 size, Texture& texture, glm::vec4* customRect = nullptr);
// Draw cube wires
void DrawCubeWires(glm::vec3 position, glm::vec3 size, glm::vec4 color);
// Draw Sphere Outline
void DrawSphereOutline(glm::vec3 position, glm::vec3 radius, glm::vec3 rotation, float outlineThickness = 0.03f);
// Draw Sphere
void DrawSphere(glm::vec3 position, float radius, glm::vec3 rotation = glm::vec3(0.0), glm::vec4 color = glm::vec4(1.0f), float seed = 0.0f);
// Draw Skybox
void DrawSkybox(FirstPersonController& fpc, Texture& cubemapTexture);

struct FT_FaceRec_;

struct Glyph {
    glm::vec2 Size;           // Size of glyph
    glm::vec2 Bearing;        // Offset from baseline to left/top of glyph
    float Advance;            // Horizontal offset to advance to next glyph
    glm::vec2 TexCoordStart;   // Start texture coordinates in atlas
    glm::vec2 TexCoordEnd;     // End texture coordinates in atlas
    glm::vec2 VisualOffset;   // NEW: Offset to move the quad to include padding
};

// A glyph rasterised (and hinted) by FreeType at one pixel size.
struct BitmapGlyph {
    glm::vec2 Size = glm::vec2(0.0f);    // Bitmap size in pixels
    glm::vec2 Bearing = glm::vec2(0.0f); // Left/top offset from the pen on the baseline, in pixels
    float Advance = 0.0f;                // Pen advance in pixels
    glm::vec2 TexCoordStart = glm::vec2(0.0f);
    glm::vec2 TexCoordEnd = glm::vec2(0.0f);
};

// One rasterised pixel size (and weight) of a font, with its vertical metrics.
struct BitmapStrike {
    float ascender = 0.0f;   // Pixels above the baseline (positive)
    float descender = 0.0f;  // Pixels below the baseline (negative)
    float lineHeight = 0.0f; // Baseline-to-baseline distance in pixels
    std::map<char32_t, BitmapGlyph> glyphs;
};

enum class FontHinting {
    NONE,  // Unhinted outlines (smoothest shapes, softest edges)
    LIGHT, // Vertical-only hinting (FreeType default for modern UIs)
    FULL   // Classic TrueType bytecode hinting (v35), like Windows GDI and Flash device text
};

struct Font {
    // msdf stuff
    msdfgen::FreetypeHandle* ft = nullptr;
    msdfgen::FontHandle* font = nullptr;
    msdfgen::FontMetrics metrics;

    // atlas stuff
    int offsetX = 0;
    int offsetY = 0;
    int rowHeight = 0;
    const int padding = 2; // gap in atlas between glyphs

    // font stuff
    int baseGlyphSize = 48;  // Default font size used to generate glyphs
    double emRangeLow = -0.15; // represents how far "outside" the glyph the distance field extends.
    double emRangeHigh = 0.1; // represents how far "inside" the glyph the distance field tracks.

    Texture glyphTexture;
    std::map<char32_t, Glyph> characters;

    // bitmap stuff: hinted FreeType glyphs for crisp small, unrotated text
    std::filesystem::path path;
    FT_FaceRec_* bitmapFace = nullptr;
    FontHinting hinting = FontHinting::LIGHT;
    float coverageGamma = 1.0f; // Above 1 thickens anti-aliased edges (GDI-like); 1 keeps FreeType's linear coverage
    Texture bitmapAtlas;
    int bitmapOffsetX = 0;
    int bitmapOffsetY = 0;
    int bitmapRowHeight = 0;
    std::map<int, BitmapStrike> strikes; // Keyed by pixel size and weight

    Font(std::string path);
    Font(GL_FilterModes mode = GL_FilterModes::LINEAR);
    ~Font();
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    bool loadFromFile(const std::filesystem::path& filename, int _baseGlyphSize = 48);
    bool loadGlyph(char32_t codepoint);
    bool loadGlyphs(const std::string& charset = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~");
    void clear();

    // Bitmap glyphs, strikes and kerning (sizes are device pixels).
    BitmapStrike* getStrike(int pixelSize, int weight = 400);
    const BitmapGlyph* getBitmapGlyph(char32_t codepoint, int pixelSize, int weight = 400);
    float getKerning(char32_t left, char32_t right);                      // In em units
    float getBitmapKerning(char32_t left, char32_t right, int pixelSize); // In pixels
    void setHinting(FontHinting mode);
    void setCoverageGamma(float gamma);
    void clearBitmaps();

private:
    bool openBitmapFace();
};

// Text extents from the font's metrics (not the ink of particular glyphs), so
// strings of one font and size always measure the same height.
struct TextMeasurement {
    float maxWidth = 0.0f;     // Widest line's advance width
    float maxAscender = 0.0f;  // Font ascender above the first baseline (positive)
    float maxDescender = 0.0f; // Lowest descender below the first baseline (negative; includes later lines)
    float totalHeight = 0.0f;  // maxAscender - maxDescender
    int lineCount = 0;
};

enum class TextRenderMode {
    AUTO,   // Hinted bitmaps for unrotated, unstretched text up to 64px; MSDF otherwise
    BITMAP, // Always hinted bitmaps (pixel-snapped)
    SDF     // Always multi-channel signed distance fields (scalable, rotatable, effects)
};

enum class TextAlignmentX {
    LEFT,
    CENTER,
    RIGHT
};

enum class TextAlignmentY {
    TOP,
    CENTER,
    BASELINE,
    BOTTOM
};

struct Text {
    Font* font = nullptr;
    std::string string;
    float characterSize = 30.0f; // Desired render size in pixels
    glm::vec4 color = WHITE;
    float strokeWidth = 0.0f; // Extra MSDF coverage in screen pixels
    int fontWeight = 400; // CSS-style weight: 400 regular, 700 bold
    float horizontalScale = 1.0f; // Width scale for SWF text fields with authored glyph spacing
    TextRenderMode renderMode = TextRenderMode::AUTO;
    float letterSpacing = 0.0f;   // Extra pixels between glyphs
    float lineHeightScale = 1.0f; // Multiple of the font's own line height
    bool kerning = true;          // Apply the font's kerning pairs

    bool background = false;
    glm::vec2 backgroundBorder = glm::vec2(1.0f);
    glm::vec4 backgroundColor = BLACK;

    Text();
    Text(Font& font, std::string string = "", float characterSize = 30.0f);

    void setFont(Font& _font);
    void setString(const std::string& _string);
    void setColor(const glm::vec4& _color);
    void setCharacterSize(float _characterSize); // in pixels
    void setStrokeWidth(float _strokeWidth);
    void setFontWeight(int _fontWeight);
    void setHorizontalScale(float _horizontalScale);
    void setRenderMode(TextRenderMode _renderMode);
    void setLetterSpacing(float _letterSpacing);
    void setLineHeightScale(float _lineHeightScale);
    void setKerning(bool _kerning);

    // Whether this text draws as hinted bitmaps with the given rotation.
    bool UsesBitmap(float rotationRadians = 0.0f) const;

    TextMeasurement MeasureText() const;
    TextMeasurement MeasureText(const std::string& _string) const;

    float GetCursorX(const std::string& str, int index) const;

    float GetScale() const;

    float GetLineSpacing() const;

    void Draw(int posX, int posY, TextAlignmentX alignmentX = TextAlignmentX::LEFT, TextAlignmentY alignmentY = TextAlignmentY::BASELINE, float rotationRadians = 0.0f) const;
    void DrawPrecise(float posX, float posY, TextAlignmentX alignmentX = TextAlignmentX::LEFT, TextAlignmentY alignmentY = TextAlignmentY::BASELINE, float rotationRadians = 0.0f) const;
};

void DrawText(const std::string& text, float posX, float posY, float fontSize, glm::vec4 textColor, TextAlignmentX alignmentX = TextAlignmentX::LEFT, TextAlignmentY alignmentY = TextAlignmentY::BASELINE);
void DrawText(const std::string& text, glm::vec2 pos, float fontSize, glm::vec4 textColor, TextAlignmentX alignmentX = TextAlignmentX::LEFT, TextAlignmentY alignmentY = TextAlignmentY::BASELINE);
// Draws with a specific font instead of the default one.
void DrawText(const std::string& text, glm::vec2 pos, float fontSize, glm::vec4 textColor, Font& font, TextAlignmentX alignmentX = TextAlignmentX::LEFT, TextAlignmentY alignmentY = TextAlignmentY::BASELINE);

void DrawTextWithBackground(std::string text, glm::vec2 pos, float fontSize, glm::vec4 textColor = WHITE, glm::vec4 backGroundColor = BLACK, int bgMargin = 2);

TextMeasurement MeasureText(std::string text, float fontSize);

extern GLuint global_text_vao, global_text_vbo;

void InitOpenGLResources(size_t maxCharacters = 4096);


std::string to_string_rounded(double value, int digits);

// Function to get the angle between two points in radians
float GetAngleBetweenPoints(const glm::vec2& point1, const glm::vec2& point2);

void setListenerPosition(const glm::vec3& position);

// Namespace to avoid conflicts
namespace CleanupHandler {
    BOOL WINAPI ConsoleHandler(DWORD event);
}



enum class FilterType {
    LowPass = 0x0001,  // AL_FILTER_LOWPASS
    HighPass = 0x0002, // AL_FILTER_HIGHPASS
    BandPass = 0x0003  // AL_FILTER_BANDPASS
};

class AudioFilter {
public:
    bool filterLoaded = false;
    ALuint filterID = 0;
    FilterType type;

    // Constructor for any type
    AudioFilter(FilterType t, float gain, float hfGain, float lfGain = 1.0f);
    ~AudioFilter();

    ALuint create(float gain, float hfGain, float lfGain);
    bool adjust(float gain, float hfGain, float lfGain);
};

class SoundEffect {
public:
    std::atomic<bool> deleted = false;

    std::atomic<float> volume = 1.0;
    std::atomic<float> maxVolume = 1.0;

    SoundEffect(int sourceCount = 8);
    SoundEffect(const std::string& filename, int sourceCount = 8, bool forceMono = false);
    void CleanupSound();
    ~SoundEffect();
    void updateData(std::vector<float> _samples, ALenum _format, ALsizei _sampleRate);
    void bufferData();
    float calculateVolume();
    void play(bool withPos = false);
    bool ensureMonoFormat();
    void playAt3DPosition(const glm::vec3& position);
    void playWith2DPan(float pan, float distance);
    void pause();
    void stop();
    bool isPlaying() const;
    void attachFilter(ALuint filter);
    void detachFilter(ALuint filter);
    void setMaxVolume(float volume);
    void applyFilter(const std::function<void(std::vector<float>&)>& filter);

private:
    ALuint buffer;
    std::vector<ALuint> sources;
    std::vector<float> samples;
    ALenum format;
    ALsizei sampleRate;
    int sourceCount;
    int nextSourceIndex;
    std::thread volumeThread;
    std::atomic<bool> stopThread;

    void setVolume(float volume);
    void loadAudioFile(const std::string& filename);
    void volumeMonitoringThread();
};

class MusicSound {
public:
    std::atomic<bool> deleted = false;

    std::atomic<bool> playing;

    MusicSound(const std::string& filename, bool loop = false);
    void CleanupMusic();
    ~MusicSound();
    void play();
    void pause();
    void stop();
    bool isPlaying() const;
    void setVolume(float volume);
    void attachFilter(ALuint filter);
    void detachFilter(ALuint filter);

private:
    ALuint source;
    std::vector<ALuint> buffers;
    const size_t bufferSize = 8192;
    SF_INFO sfInfo;
    SNDFILE* sndFile;
    std::atomic<bool> streaming;
    std::thread streamingThread;
    ALenum format;
    int freq;
    bool looping;

    void loadAudioFile(const std::string& filename);
    sf_count_t readAudioFrames(float* destination, sf_count_t frameCount);
    void streamAudio();
    void fillBuffers();
};

class MusicManager {
public:
    std::vector<MusicSound*> sounds;

    MusicManager();
    ~MusicManager();
    void addSound(const std::string& filename);
    void playMusic();
    void stopMusic();
    void skipSong();
    void setVolume(float volume);
    void addFilter(ALuint filter);
};


glm::vec2 NDCtoScreen(glm::vec2 pos);


struct Padding {
    float left = 0, right = 0, top = 0, bottom = 0;
};

struct TargetRectLayout {
    std::string name = "";
    //Rect* boundRect = nullptr;
    std::function<void(const Rect&)> drawFunction = nullptr;

    glm::vec4 backgroundColor = CLEAR;

    //glm::vec2 _targetSize = glm::vec2(0);
    glm::vec2 _minSize = glm::vec2(0);
    glm::vec2 _maxSize = glm::vec2(-1);
    Padding padding;
    float childGap = 0;

    bool widthGrowable = true;
    bool heightGrowable = true;
    bool widthShrinkable = true;
    bool heightShrinkable = true;

    enum Axis { HORIZONTAL, VERTICAL };
    Axis childAxis = HORIZONTAL;

    enum ChildOrder { NORMAL, REVERSED };
    ChildOrder childOrder = NORMAL;

    enum Alignment { START, CENTER, END };
    Alignment alignX = CENTER;
    Alignment alignY = END;

    bool wrapText = true;
    std::variant<std::monostate, std::string, std::string*> text;
    float fontSize = 16.0f; // Necessary for measurement

    int _depthLayer = -1;

    bool active = true;
};

class ScrollableContainer {
public:
    Rect viewportRect; // The area where content is visible
    float scrollOffset = 0.0f;
    float contentHeight = 0.0f;

    void Draw(const std::function<void(float, float)>& drawContentFunc);
};

struct UILayout
{
    TargetRectLayout layout; 
    std::optional<ScrollableContainer> scrollContainer;

    UILayout* parent = nullptr;
    std::vector<UILayout> children;

    Rect rect;

    // Results of wrapping stored here for rendering
    std::vector<std::string> wrappedLines;
    float calculatedLineHeight = 0;

    int CreateChild(TargetRectLayout childLayout);

    void RemapParents(UILayout* newParent);

    /*-----------------------
        PASS -1
        FIX MAX SIZE DEFAULT
        top-down
    -----------------------*/
    void PassCalculateFixedMaxSize();
    void CalculateFixedMaxSize();

    /*-----------------------
        PASS 0
        CONSTRAINT PROPAGATION
        top-down
    -----------------------*/
    void PassConstraints();

    /*-----------------------
        PASS 1
        WIDTH FIT
        bottom-up
    -----------------------*/
    void PassWidthFit();

    void ResolveWidthFit();

    /*-----------------------
        PASS 2
        FLEX WIDTH
        top-down
    -----------------------*/
    void PassFlexWidth();

    void ResolveFlexWidth();

    void GrowChildren(bool horizontal, float remaining);

    void ShrinkChildren(bool horizontal, float remaining);

    /*-----------------------
        PASS 3
        TEXT WRAP
        top-down
    -----------------------*/
    void PassTextWrap();

    void ResolveWrapText();

    /*-----------------------
        PASS 4
        HEIGHT FIT
        bottom-up
    -----------------------*/
    void PassHeightFit();

    void ResolveHeightFit();

    /*-----------------------
        PASS 5
        FLEX HEIGHT
        top-down
    -----------------------*/
    void PassFlexHeight();

    void ResolveFlexHeight();

    /*-----------------------
        PASS 6
        POSITION
        top-down
    -----------------------*/
    void PassPosition();

    void ResolvePositions();

    /*-----------------------
        PASS 7
        RUN UPDATES
        top-down
    -----------------------*/
    //void PassRectUpdates();

    /*-----------------------
        PASS 8
        RENDER RECTS
        top-down
    -----------------------*/
    void PassRender(bool glassy);
};

class UIManager {
public:
    std::vector<UILayout> initialRootNodes;
    std::vector<UILayout> rootNodes;

    int AddRootNode(TargetRectLayout layout);

    void ComputeUI();

    void RenderUI(bool glassy = false, Texture* backgroundTexture = nullptr);

    // Helper function for recursive searching
    UILayout* FindNodeRecursive(UILayout* current, const std::string& name);

    UILayout* FindNode(const std::string& name);
};

class Button {
public:
    enum States {
        DEFAULT,
        HOVERING,
        HELD,
        PRESSED,
        WAS_HELD,
    };
    Rect rect;
    float fontSize;
    std::string text;
    Button::States state = DEFAULT;
    glm::vec4 color = DARKGRAY;
    Text renderText;

    glm::vec4 margin = glm::vec4(0);

    bool useTexture = false;
    Texture* texture = nullptr;

    Button();
    Button(glm::vec2 position, glm::vec2 size, float fontSize, std::string text, glm::vec4 color = DARKGRAY);
    Button(Rect rect, float fontSize, std::string text, glm::vec4 color = DARKGRAY);
    bool WasPressed();
    void Update();
    void Update(Camera2D cam);
    void Update(glm::vec2 mouseWorldPos);
    void Draw();
};

class TextInputBox {
public:
    Rect rect;
    std::string defaultText;
    std::string text;
    std::string finalText;
    bool hadFocus = false;
    bool hasFocus = false;
    glm::vec4 color;
    glm::vec4 textColor;
    glm::vec4 selectionColor = glm::vec4(0.4f, 0.4f, 0.8f, 0.5f);
    glm::vec4 selectedTextColor = WHITE;
    glm::vec4 cursorColor = BLACK;
    bool invertSelection = false;
    int offsetFromLeft = 5;
    float textOffsetX = 0.0f;
    float textOffsetY = 0.0f;
    int cursorPosition = 0;
    int lastCursorPosition = -1;
    double cursorBlinkStartTime = 0.0;
    int selectionStart = -1;
    int selectionEnd = -1;
    bool isSelecting = false;
    bool lastModifiedEnd = false;
    bool ignoreMouse = false;
    bool escapeCharPressed = false;
    bool autoDeselect = true;
    Text renderText;

    TextInputBox();
    TextInputBox(glm::vec2 pos, glm::vec2 size, float fontSize, std::string defaultText = "");
    void clearSelection();
    void clear();
    void deleteSelection();
    int findCursorPosition(const std::string& lineText, float clickX);
    void HandleKeys();
    void Update(glm::vec2 mouseWorldPos);
    bool WasJustDeselected();
    void Update();
    void Update(Camera2D cam);
    void Draw();
};

class SliderBar {
public:
    Rect rect;
    float minValue;
    float maxValue;
    bool vertical;
    float value; // Current value of the slider (minValue to maxValue)
    bool hovering = false;
    bool selected = false;

    glm::vec4 backColor = LIGHTGRAY;
    glm::vec4 handleColor = glm::mix(LIGHTGRAY, BLUE, 0.2);

    int handleWidth = 10;

    glm::vec4 backMargin = glm::vec4(0);
    glm::vec4 handleMargin = glm::vec4(0);

    bool useTexture = false;
    Texture* backTexture = nullptr;
    Texture* handleTexture = nullptr;

    SliderBar();
    SliderBar(glm::vec2 pos, glm::vec2 size, float minValue, float maxValue, bool vertical = false);
    void Update();
    void Update(glm::vec2 mouseWorldPos);
    void Draw();
};

class ScrollableTextBox {
public:
    Rect rect;                      // Layout-controlled rectangle
    Rect innerRect;
    Text textRenderer;              // Font rendering
    SliderBar scrollSlider;
    std::string text;               // Original text
    std::vector<std::string> lines; // Wrapped lines
    float lineHeight;
    float scrollOffset = 0.0f;
    float scrollBarWidth = 10.0f;

    glm::vec4 fontColor = GRAY;
    float fontSize = 14.0f;

    float padding = 5.0f;

    ScrollableTextBox();
    ScrollableTextBox(std::string content);

    float GetLineHeight() const;
    void SetRect(Rect _rect);

    void Update();
    void Update(glm::vec2 mousePos);

    // Called whenever text changes or box width changes
    void Reflow(float width);

    float GetContentHeight() const;

    float getMaxOffset();

    void Scroll(float delta);

    void Draw();
};


template <typename T, typename Predicate>
void EraseIf(std::vector<T>& vec, Predicate pred) {
    vec.erase(
        std::remove_if(vec.begin(), vec.end(), pred),
        vec.end()
    );
}

void SplitText(std::string& text, const Rect& rect, Text& textRenderer);



// cereal vec2, 3, 4 serialization 
namespace cereal {
    template <class Archive>
    void serialize(Archive& archive, glm::vec2& v) {
        archive(v.x, v.y);
    }

    template <class Archive>
    void serialize(Archive& archive, glm::vec3& v) {
        archive(v.x, v.y, v.z);
    }

    template <class Archive>
    void serialize(Archive& archive, glm::vec4& v) {
        archive(v.x, v.y, v.z, v.w);
    }
    
    // Quaternion (w, x, y, z)
    template <class Archive>
    void serialize(Archive& archive, glm::quat& q) {
        archive(q.w, q.x, q.y, q.z);
    }

    // Matrix 4x4
    template <class Archive>
    void serialize(Archive& archive, glm::mat4& m) {
        // GLM matrices can be accessed as an array of 4 column vectors
        archive(m[0], m[1], m[2], m[3]);
    }
}



struct MessageHeader {
    uint16_t mainType;   // Main message type as an integer
    uint16_t subType;    // Sub message type as an integer
    uint32_t dataLength;     // Length of the data following the header
};

enum BaseMessageTypes {
    CONNECT,
    DISCONNECT,
    CONNECTION_ATTEMPT_TIMEOUT,
    CUSTOM,
};

class Connection {
public:
    int clientId;
    ENetPeer* peer; // Pointer to the peer, useful for sending messages directly
};


template<typename T>
std::vector<char> serializeMessage(uint16_t mainType, uint16_t subType, const T& data) {
    // Serialize data using cereal
    std::stringstream ss;
    {
        cereal::BinaryOutputArchive oarchive(ss); // Create an output archive
        oarchive(data); // Serialize the data
    }

    std::string serializedData = ss.str();
    std::vector<char> buffer(sizeof(MessageHeader) + serializedData.size());

    // Setup the header
    MessageHeader header = { mainType, subType, static_cast<uint32_t>(serializedData.size()) };

    // Combine the header and the serialized data
    std::memcpy(buffer.data(), &header, sizeof(header));
    std::memcpy(buffer.data() + sizeof(header), serializedData.data(), serializedData.size());

    return buffer;
}

template<typename T>
std::vector<char> serializeAndCompressMessage(uint16_t mainType, uint16_t subType, const T& data) {
    // Serialize just the data to be compressed
    std::stringstream ss;
    {
        cereal::BinaryOutputArchive oarchive(ss);
        oarchive(data); // Serialize the data
    }
    std::string serializedData = ss.str();
    size_t originalSize = serializedData.size();  // Store original size for compression ratio calculation
    size_t compressedSize = ZSTD_compressBound(originalSize);
    std::vector<char> compressedData(compressedSize);

    // Compress the serialized data
    size_t actualCompressedSize = ZSTD_compress(compressedData.data(), compressedSize, serializedData.data(), originalSize, 1);
    if (ZSTD_isError(actualCompressedSize)) {
        throw std::runtime_error(ZSTD_getErrorName(actualCompressedSize));
    }
    compressedData.resize(actualCompressedSize);

    // Calculate and print the compression ratio
    // double compressionRatio = (double)actualCompressedSize / (double)originalSize;
    //std::cout << "Compression Ratio: " << compressionRatio << " (Lower is better)" << std::endl;

    // Setup the header with the size of the compressed data
    MessageHeader header = { mainType, subType, static_cast<uint32_t>(actualCompressedSize) };
    std::vector<char> buffer(sizeof(header) + actualCompressedSize);
    std::memcpy(buffer.data(), &header, sizeof(header));
    std::memcpy(buffer.data() + sizeof(header), compressedData.data(), actualCompressedSize);

    return buffer;
}

bool deserializeHeader(const std::vector<char>& buffer, uint16_t& mainType, uint16_t& subType, uint32_t& dataLength);

template<typename T>
bool deserializeData(const std::vector<char>& buffer, T& data) {
    if (buffer.size() < sizeof(MessageHeader)) {
        std::cerr << "Buffer too small to contain header" << std::endl;
        return false;
    }

    // Extract serialized data skipping the header
    std::string serializedData(buffer.begin() + sizeof(MessageHeader), buffer.end());

    std::stringstream ss(serializedData);
    try {
        cereal::BinaryInputArchive iarchive(ss); // Create an input archive
        iarchive(data); // Deserialize the data
    }
    catch (const cereal::Exception& e) {
        std::cerr << "Failed to deserialize: " << e.what() << std::endl;
        return false;
    }

    return true;
}

template<typename T>
bool decompressAndDeserializeData(const std::vector<char>& buffer, T& data) {
    // Check if the buffer is large enough to contain a header
    if (buffer.size() < sizeof(MessageHeader)) {
        throw std::runtime_error("Buffer too small to contain a valid message header.");
    }

    // Cast the buffer to a header to read metadata
    const MessageHeader* header = reinterpret_cast<const MessageHeader*>(buffer.data());

    // Calculate the starting point of the compressed data
    const char* compressedData = buffer.data() + sizeof(MessageHeader);
    size_t compressedSize = header->dataLength;

    // Retrieve the decompressed size from the compressed data
    size_t decompressedSize = ZSTD_getFrameContentSize(compressedData, compressedSize);
    if (decompressedSize == ZSTD_CONTENTSIZE_ERROR) {
        throw std::runtime_error("Invalid compressed data");
    }
    else if (decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN) {
        throw std::runtime_error("Unknown decompressed size");
    }

    // Allocate space for decompressed data
    std::vector<char> decompressedData(decompressedSize);
    size_t actualDecompressedSize = ZSTD_decompress(decompressedData.data(), decompressedSize, compressedData, compressedSize);
    if (ZSTD_isError(actualDecompressedSize)) {
        throw std::runtime_error(ZSTD_getErrorName(actualDecompressedSize));
    }

    // Ensure that all expected data was decompressed
    if (actualDecompressedSize != decompressedSize) {
        throw std::runtime_error("Decompressed data size does not match expected size");
    }

    // Deserialize the decompressed data using Cereal
    std::stringstream ss(std::string(decompressedData.begin(), decompressedData.end()));
    try {
        cereal::BinaryInputArchive iarchive(ss); // Create an input archive
        iarchive(data); // Deserialize the data
        return true;
    }
    catch (const cereal::Exception& e) {
        std::cerr << "Failed to deserialize: " << e.what() << std::endl;
        return false;
    }
}

class NetworkLogger {
public:
    float total_up = 0;
    int total_up_size = 0;
    float total_down = 0;
    int total_down_size = 0;
    float totalTime = 0;
    float ups_up = 0;
    float ups_down = 0;
    float ups_upSize = 0;
    float ups_downSize = 0;

    NetworkLogger();
    void LogNetworkUpload(int size);
    void LogNetworkDownload(int size);
    void Update(float dt);
};

class NetworkManager {
public:
    ENetHost* server = nullptr;
    ENetHost* client = nullptr;
    ENetPeer* serverPeer = nullptr;
    std::unordered_map<int, Connection> connections;
    int thisID = 0;
    int nextClientId = 0; // To assign unique IDs to each client
    float timeWaitedForConnectionAttempt = 0;
    NetworkLogger networkLogger;
    
    int port = 0;
    std::string privateIP = "Unknown";

    ~NetworkManager();
    void BroadcastNetworkMessage(const std::vector<char>& message);
    void SendNetworkMessageToPeer(const std::vector<char>& message, ENetPeer* target);
    void SendNetworkMessageToServer(const std::vector<char>& message);
    std::vector<char> ReceiveNetworkMessage(ENetEvent& event);
    void CreateServer(int port = 12345);
    std::vector<std::vector<char>> HandleServerEvents();
    void StopServer();
    void CreateClient(std::string ipAddress = "localhost", int port = 12345);
    std::vector<std::vector<char>> TryConnectToServer(float dt, float maxTime);
    std::vector<std::vector<char>> HandleClientEvents();
    void StopClient();
};





glm::vec4 hsvToRgb(float hue, float saturation, float value);
glm::vec3 rgbToHsv(const glm::vec3& rgb);



















namespace Ease {

    inline float Clamp01(float t) {
        return std::clamp(t, 0.0f, 1.0f);
    }

    // Scalar easing functions
    inline float Linear(float t) {
        // Linear ping-pong 0->1->0
        t = fmodf(t, 2.0f);
        float linear = (t <= 1.0f) ? t : (2.0f - t);
        return linear;
    }

    inline float InQuad(float t) {
        t = Clamp01(t);
        return t * t;
    }

    inline float OutQuad(float t) {
        t = Clamp01(t);
        return t * (2.0f - t);
    }

    inline float InOutQuad(float t) {
        t = Clamp01(t);
        return (t < 0.5f) ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
    }

    inline float InCubic(float t) {
        t = Clamp01(t);
        return t * t * t;
    }

    inline float OutCubic(float t) {
        t = Clamp01(t);
        t -= 1.0f;
        return t * t * t + 1.0f;
    }

    inline float InOutCubic(float t) {
        t = Clamp01(t);
        return (t < 0.5f) ? 4.0f * t * t * t : (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f;
    }

    inline float InSine(float t) {
        t = Clamp01(t);
        return 1.0f - std::cos(t * TWO_PI);
    }

    inline float OutSine(float t) {
        t = Clamp01(t);
        return std::sin(t * TWO_PI);
    }

    inline float InOutSine(float t) {
        t = Clamp01(t);
        return -(std::cos(PI * t) - 1.0f) / 2.0f;
    }

    inline float InElastic(float t) {
        t = Clamp01(t);
        return std::sin(13.0f * TWO_PI * t) * std::pow(2.0f, 10.0f * (t - 1.0f));
    }

    inline float OutElastic(float t) {
        t = Clamp01(t);
        return std::sin(-13.0f * TWO_PI * (t + 1.0f)) * std::pow(2.0f, -10.0f * t) + 1.0f;
    }

    inline float OutBounce(float t) {
        t = Clamp01(t);
        if (t < 1 / 2.75f)
            return 7.5625f * t * t;
        else if (t < 2 / 2.75f) {
            t -= 1.5f / 2.75f;
            return 7.5625f * t * t + 0.75f;
        }
        else if (t < 2.5f / 2.75f) {
            t -= 2.25f / 2.75f;
            return 7.5625f * t * t + 0.9375f;
        }
        else {
            t -= 2.625f / 2.75f;
            return 7.5625f * t * t + 0.984375f;
        }
    }

    inline float InBounce(float t) {
        t = Clamp01(t);
        return 1.0f - OutBounce(1.0f - t);
    }

    inline float InOutBounce(float t) {
        t = Clamp01(t);
        return (t < 0.5f)
            ? (1.0f - OutBounce(1.0f - 2.0f * t)) * 0.5f
            : (1.0f + OutBounce(2.0f * t - 1.0f)) * 0.5f;
    }

    // Template-based LERP using easing
    template<typename T, typename EaseFn>
    inline T Interpolate(const T& from, const T& to, float t, EaseFn ease) {
        float e = ease(Clamp01(t));
        return from + (to - from) * e;
    }


    // Specialization for glm types using glm::mix
    template<typename EaseFn>
    inline glm::vec2 Interpolate(const glm::vec2& from, const glm::vec2& to, float t, EaseFn ease) {
        return glm::mix(from, to, ease(Clamp01(t)));
    }

    template<typename EaseFn>
    inline glm::vec3 Interpolate(const glm::vec3& from, const glm::vec3& to, float t, EaseFn ease) {
        return glm::mix(from, to, ease(Clamp01(t)));
    }

    template<typename EaseFn>
    inline glm::vec4 Interpolate(const glm::vec4& from, const glm::vec4& to, float t, EaseFn ease) {
        return glm::mix(from, to, ease(Clamp01(t)));
    }
}














std::string GetTimestamp();








class ComfyUIManager {
public:
    std::atomic<bool> is_generating{ false };
    std::atomic<bool> has_new_image{ false };
    std::string active_job_id = "";
    std::mutex state_mutex;

private:
    std::string base_url;
    int port;

    // Staging array to hold raw bytes directly for your texture class
    std::vector<unsigned char> raw_rgba_pixels;
    int img_width = -1;
    int img_height = -1;

    std::string current_checkpoint_name = "v1-5-pruned-emaonly.safetensors"; 
    
    std::thread worker_thread;
    std::atomic<bool> should_terminate{ false };

public:
    struct CheckpointDetails {
        int index = -1;
        std::string filename = "";
        bool is_currently_active = false;
    };

    ComfyUIManager(const std::string& host = "127.0.0.1", int port_num = 8188);

    ~ComfyUIManager();

    void StartImageGenerationJob(const std::string& promptText, const std::string& unique_job_id); 
    
    void Shutdown();

    CheckpointDetails SelectAndInspectModel(int targetIndex);

    // Call this inside your active OpenGL update frame window loop
    void UpdateMainThreadTexture(Texture& targetTexture);

private:
    void AsyncGenerationWorker(const std::string& promptText);
};




class LLMManager {
public:
    struct ModelStatus {
        std::string key;
        bool is_loaded;
    };

    // Thread-safe communication state
    std::atomic<bool> has_new_text{ false };
    std::string generated_text = "";
    std::string active_stream_id = "";
    std::mutex state_mutex;

    std::string active_model_id = "";
    bool is_model_loading_async = false;

private:
    std::string last_response_id = "";
    std::string base_url = "127.0.0.1";
    int port = 1234; 
    
    std::thread worker_thread;
    std::atomic<bool> should_terminate{ false };

public:
    LLMManager() = default;

    ~LLMManager();

    void Shutdown();

    // --- Public Interface API ---

    void StartWindowsClipboardImageJob(const std::string& prompt, const std::string& unique_msg_id);

    // 1. Text-only Job
    void StartChatJob(const std::string& prompt, const std::string& unique_msg_id, const std::string& system_prompt = "");

    // 2. Multimodal Image Job
    void StartTextureChatJob(const std::string& prompt, Texture& texture, const std::string& unique_msg_id);

    // 3. Local File Image Job
    void StartImageChatJob(const std::string& prompt, const std::string& image_path, const std::string& unique_msg_id);

    // --- Existing GPU / Model State Methods ---

    std::vector<ModelStatus> GetAvailableModelsFromServer();

    bool IsModelLoadedOnServer(const std::string& model_key);

    bool UnloadModel(const std::string& model_key);

    bool LoadModelToGPU(const std::string& model_key);

private:
    void PrepareForJob(const std::string& unique_msg_id);

    // Single Consolidated Stream Processing Worker
    void AsyncChatWorker(const nlohmann::json& input_field, std::string unique_msg_id, std::string system_prompt = "");
};
















struct keyCallbackData {
    int key;
    int scancode;
    int action;
    int mods;
};

namespace extras {
    extern OpenAL* openAL;

    extern bool optimizeGLCalls; // skips redundant unbindings : disable for debug mode
    extern bool draw3DMirrored;
    extern bool drawYDown;

    extern bool useInverseZBuffer;

    extern bool initialized;
    extern GLFWwindow* ActiveWindow;

    extern PerfLogger perfLogger;

    extern GLint LastSrcAlpha;
    extern GLint LastDstAlpha;
    extern GLint CurrentSrcAlpha;
    extern GLint CurrentDstAlpha;

    extern std::vector<DepthModes> LastActiveDepthStack;
    extern DepthModes CurrentActiveDepth;

    extern Shader Default2DShader;
    extern Shader Default3DShader;
    extern Shader DefaultSkyboxShader;
    extern Shader DefaultTextShader;
    extern Shader BitmapTextShader;
    extern Shader DefaultLineShader;
    extern Shader Default2DBlurShader;
    extern Shader Default2DGeometryGlassShader;

    extern std::vector<Shader*> LastActiveShaderStack;
    extern Shader* ActiveShader;
    extern int targetFps;
    extern bool WindowSizeChanged;
    extern bool WindowFrozen;

    extern size_t maxTextChars;
    extern float fontScale;
    extern Font* defaultFont;

    extern Text* defaultTextRenderer;

    extern double rawFreezeStartTime;
    //extern double startTime; // window init finished time
    extern double lastRawFrameTime;
    extern double lastFrameTime; // 

    extern double totalTimeFrozen; // total time window spent in a frozen state | used for offsetting clock timings

    extern double rawFrameDeltaTime; // easy lookup var for the raw last frame time (dt)
    extern double frameDeltaTime; // easy lookup var for the last frame time (dt)

    extern std::map<int, bool> keyState;
    extern std::map<int, std::map<int, bool>> buttonState;
    extern std::map<int, bool> lastMouseState;
    extern std::map<int, bool> mouseState;
    extern glm::vec2 lastMousePosition;
    extern glm::vec2 mouseWheelOffset;

    // Current mouse position XY in range 0.0-1.0 (can go above and below 0 and 1 because of mouse out of window boundary (after calling PollCustomEvents())
    extern glm::vec2 CurrentMousePosition;
    // Current mouse position XY range to 0.0-CurrentViewportSize (after calling PollCustomEvents())
    extern glm::vec2 CurrentMouseScreenPosition;

    extern Camera2D* DefaultCamera;
    extern Camera2D* activeCamera2D;
    extern Camera3D* activeCamera3D;

    extern int CurrentWindowWidth;
    extern int CurrentWindowHeight;

    extern bool WindowFocused;
    extern bool CaptureMouse;

    extern std::vector<keyCallbackData> keyCallbacksData;
    extern std::vector<unsigned int> codepointsData;

    extern void (*renderFunction)();
}



