#include "Game.h"
#include "SwfClip.h"
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <set>

// Reconstructed from frame_7/DoAction.as. The original movie runs at 24 Hz.
namespace {
constexpr float Tick = 1.f / 24;
// Flash's distance() to a missing clip is NaN. AVM1 compiles a <= b as
// !(a > b), so "<=" against NaN passes while "<" and ">" fail; the original's
// targeting depends on that, and so does this port.
constexpr float NaN = std::numeric_limits<float>::quiet_NaN();
bool AtMost(float a, float b) { return !(a > b); }
enum class Screen {
  Splash,
  Title,
  Instructions,
  Difficulty,
  Intro,
  Landing,
  Scores,
  Play,
  Pause,
  Shop,
  Won,
  Lost,
  Missing
};
struct Actor {
  glm::vec2 pos{};
  float hp = 0, maxHp = 0, armor = 0, radius = 20;
  float rotation = 0; // clip._rotation (a flyer's own _rotation), degrees
  int kind = 0;       // Aliens: 0 creep, 1 shooter, 2 flyer
  int slot = 0;       // The hero/enemy number in the original's grid
  int load = 0;       // reload, atload, shooterload or flyreload
  bool friendly = false, removed = false;
  std::shared_ptr<swf::Clip> clip; // Original movie clip (unit or building)
  float cannon = 0;                // Turret cannon _rotation, degrees
  float jitter = 1;                // Alternates clip.eff._y while firing
  glm::vec2 shock{};               // shockx/shocky knockback
  bool voiced = false;             // herodummy's first-shot voice (voc)
  bool building = false;           // Turret under construction (build)
  float progress = 0;              // Its construction counter (coun)
  // Pose when the current step began (NaN until the first), for drawing
  // between steps.
  glm::vec2 lastPos{NaN, NaN};
  float lastRotation = NaN, lastCannon = NaN;
};
// The onEnterFrame a projectile runs: frame_7 shot, shotur, shotfly, shotgnd.
enum class Flight { Shot, Shotur, Shotfly, Shotgnd };
// A shot's h: true for the hero, false for marines and Spitfire turrets,
// undefined for Terminator shells and alien shots.
enum class Owner { None, Hero, Dummy };
struct Bullet {
  glm::vec2 pos{}, origin{};
  float v = 0, rotation = 0; // Speed per frame; _rotation, degrees
  Flight flight = Flight::Shot;
  Owner h = Owner::None;
  int mode = 0; // weaponmode of the hero's shots
  std::shared_ptr<swf::Clip> clip;
  bool gone = false;
  glm::vec2 lastPos{NaN, NaN}; // As Actor::lastPos
};
// An attached effect clip with the original's onEnterFrame fades.
struct Effect {
  enum class Fade { None, Hole, Bling, Smoke };
  std::shared_ptr<swf::Clip> clip;
  glm::vec2 pos{};
  float rotation = 0, scale = 1, alpha = 100; // Flash units
  Fade fade = Fade::None;
  bool ground = false; // Drawn below units: scorch marks and corpses
  glm::vec2 lastPos{NaN, NaN}; // As Actor::lastPos
  float lastRotation = NaN, lastScale = NaN, lastAlpha = NaN;
};
// DefineButton2_119 and the shop's rollOver text: equipment number n is
// items[n - 1]. Shields also set heroshock, to one value on purchase and (a
// quirk) another on sale.
struct Item {
  const char *name, *info;
  int price;
  float armor, hp, energy, regen, shock, sellShock;
};
const std::array<Item, 18> items{{
    {"Common Mask", "Cash: 300, Armor:+5", 300, 5, 0, 0, 0, 0, 0},
    {"Gladiator Mask", "Cash: 700, Armor:+10, HP:+100", 700, 10, 100, 0, 0, 0, 0},
    {"Demon Mask", "Cash: 2900, Armor:+25, HP:+500", 2900, 25, 500, 0, 0, 0, 0},
    {"Mega Fusion Power Cell", "Cash: 800, Energy:+300", 800, 0, 0, 300, 0, 0, 0},
    {"Giga Fusion Power Cell", "Cash: 1200, Energy:+800", 1200, 0, 0, 800, 0, 0, 0},
    {"Tera Fusion Power Cell", "Cash: 2900, Energy:+2000", 2900, 0, 0, 2000, 0, 0, 0},
    {"Thick Steel Shield", "Cash: 1000, Armor:+5, shock:-20%", 1000, 10, 0, 0, 0, .8f, .2f},
    {"Solid Titanium Shield", "Cash: 1900, Armor:+10, shock:-50%", 1900, 20, 0, 0, 0, .5f, .5f},
    {"Black Diamond Shield", "Cash: 3900, Armor:+25, shock:-90%", 3900, 25, 0, 0, 0, .1f, .9f},
    {"Soldier Armor", "Cash: 1000, Armor:+10, HP:+100", 1000, 10, 100, 0, 0, 0, 0},
    {"Knight Armor", "Cash: 1800, Armor:+20, HP:+500", 1800, 20, 500, 0, 0, 0, 0},
    {"King Armor", "Cash: 2500, Armor:+25, HP:+1000", 2500, 25, 1000, 0, 0, 0, 0},
    {"Soldier Shoulders", "Cash: 300, Armor:+3", 300, 3, 0, 0, 0, 0, 0},
    {"Knight Shoulders", "Cash: 500, Armor:+5", 500, 5, 0, 0, 0, 0, 0},
    {"King Shoulders", "Cash: 1200, Armor:+10", 1200, 10, 0, 0, 0, 0, 0},
    {"Plasma Generator", "Cash: 1000, Generated:+0.25", 1000, 0, 0, 0, .25f, 0, 0},
    {"Proton Generator", "Cash: 1800, Generated:+0.5", 1800, 0, 0, 0, .5f, 0, 0},
    {"Micro Nuclear Generator", "Cash: 2500, Generated:+0.75", 2500, 0, 0, 0, .75f, 0, 0}}};
// Shop numbers 25-28 (turrets) and 29-34 (weapons).
struct Deal {
  const char *name, *info;
  int price;
};
const Deal turretDeals[] = {
    {"Single Spitfire Turret", "Cash: 2000, Damage:20, Fast Fire", 2000},
    {"Twin Spitfire Turret", "Cash: 3800, Damage:20, Fast Fire", 3800},
    {"Single Terminator Turret", "Cash: 4800, Damage:500, Slow Reload", 4800},
    {"Twin Terminator Turret", "Cash: 6200, Damage:500, Slow Reload", 6200}};
const Deal weaponDeals[] = {
    {"Plasma Accelerator", "Cash: 2000, Damage:40", 2000},
    {"Neutron Accelerator", "Cash: 4000, Damage:80", 4000},
    {"Proton Accelerator", "Cash: 6000, Damage:200", 6000},
    {"Grenade Launcher", "Cash: 1000, Damage:+100, Splash, energy:50", 1000},
    {"Cannon Bolt Gun", "Cash: 3000, Damage:+200, Splash, energy:75", 3000},
    {"Rocket Launcher", "Cash: 5500, Damage:+500, Splash, long range, energy:100", 5500}};
// Exported sound linkage names and their DefineSound ids (n_n.mp3).
const std::pair<const char *, int> soundIds[] = {
    {"move", 1},       {"button", 2},     {"win", 3},        {"sho", 5},
    {"mann1", 6},      {"mann2", 7},      {"stup", 8},       {"clk", 9},
    {"clink", 10},     {"err", 11},       {"shote", 12},     {"shipp", 13},
    {"rocket2", 14},   {"rocket1", 15},   {"launc", 16},     {"pick", 17},
    {"ouch3", 18},     {"ouch2", 19},     {"ouch", 20},      {"up", 21},
    {"man3", 22},      {"man2", 23},      {"man1", 24},      {"macgun2", 25},
    {"macgun", 26},    {"laser3", 27},    {"laser2", 28},    {"laser1", 29},
    {"hurt", 30},      {"herodead2", 31}, {"herodead3", 32}, {"gnd", 33},
    {"man5", 34},      {"man4", 35},      {"fire", 36},      {"fire2", 37},
    {"alarm", 38},     {"exps", 39},      {"edeads", 40},    {"herodead4", 41},
    {"defeat", 42},    {"deadcreep2", 43}, {"deadcreep", 44}, {"atcrot", 45},
    {"crot", 46},      {"ch", 47},        {"att", 48},       {"txtx", 49}};
Screen screen = Screen::Title;
Actor player;
std::array<Actor, 10> buildings;
std::vector<Actor> units;
std::vector<Bullet> bullets;
std::vector<Effect> effects;
std::unordered_map<std::string, std::unique_ptr<Texture>> art;
std::unordered_map<std::string, glm::vec4> crops;
std::unordered_map<std::string, std::unique_ptr<SoundEffect>> sounds;
std::unique_ptr<MusicSound> wind;
// Flash clips (SwfClip.h) play the original timelines; the game drives their
// named children the way frame_7/DoAction.as does.
std::unique_ptr<swf::Clip> fieldClip;
std::unique_ptr<swf::Clip> splashClip; // Root frames 10-209: the MaxGames intro
std::unique_ptr<swf::Clip> helpClip, skipClip;
std::unique_ptr<swf::Clip> introShip; // shipz, with its looping jets
// shipup: lands during the cinematic, then takes off again (dropp).
std::unique_ptr<swf::Clip> dropshipClip;
// The overlays frame_7 attaches: status panel, crosshair (circle), pause
// screen, quality menu, energy warning and the shop's notices.
std::unique_ptr<swf::Clip> statusClip, circleClip, screenClip, qualClip,
    warnClip, sameClip, unmoneyClip;
// The shop (equip, at 385, 50) and its confirm dialog (at 150, 100).
std::unique_ptr<swf::Clip> equipClip, confirmClip;
std::array<std::unique_ptr<swf::Clip>, 6> slotIcons; // Status equipment icons
glm::vec2 dropshipPos{};
float dropshipScale = 2;
bool departing = false;
std::map<int, std::unique_ptr<swf::Clip>> sceneClips; // Root frames, for buttons
float menuClock = 0; // Seconds owed to menu and overlay clips
std::unique_ptr<Font> uiFont, deviceFont, boldDeviceFont;
Font *engineFont = nullptr;
std::array<int, 6> slots{};     // _root.slot: equipment numbers, 0 when empty
std::array<bool, 3> specials{}; // grenade, cannon, rocket
std::array<int, 2> turretLevel{{0, 1}};
glm::vec2 camera{}, aim{}, cameraFocus{};
// Drawing between steps: how far through the current step the frame falls
// (1 outside DrawGame), and what moved since the step began.
float viewBlend = 1;
glm::vec2 lastFocus{NaN, NaN}, lastDropshipPos{NaN, NaN};
float lastDropshipScale = NaN;
float accumulator = 0, energy = 100, maxEnergy = 100, regen = 0, difficulty = 1,
      heroShock = 1;
int cash = 100, level = 0, kills = 0, timer = 1000, weapon = 0, gun = 0,
    selectedTurret = -1, shake = 0, weaponRange = 500,
    warnTime = 0, winCounter = 0, loseCounter = 0;
bool muted = false, firing = false, warnVisible = false;
bool pointerDown = false, pointerPressed = false, pointerReleased = false;
const swf::Clip *armedButton = nullptr;
// Buttons inside clips pressed during the last draw, by character id, with
// the stage point; rollOver and rollOut events likewise.
std::map<int, glm::vec2> clickedButtons, pressedButtons, rollOvers,
    pendingRollOvers;
std::set<int> rollOuts, pendingRollOuts;
struct PointerInput {
  glm::vec2 normalized{};
  bool down = false;
};
PointerInput *testPointer = nullptr;
bool previousTestPress = false;
// The engine returns normalized mouse coordinates, not pixel coordinates.
glm::vec2 MousePixels() {
  return (testPointer ? testPointer->normalized : GetMousePosition()) *
         GetCurrentWindowSize();
}
void SamplePointer() {
  if (pointerReleased)
    armedButton = nullptr;
  if (testPointer) {
    pointerDown = testPointer->down;
    pointerPressed = pointerDown && !previousTestPress;
    pointerReleased = !pointerDown && previousTestPress;
    previousTestPress = pointerDown;
    return;
  }
  // Query every frame, including releases outside a button, so the engine's
  // previous-button snapshot cannot retain an old press indefinitely.
  pointerPressed = IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
  pointerReleased = IsMouseButtonReleased(GLFW_MOUSE_BUTTON_LEFT);
  pointerDown = IsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);
}
bool radarVisible = true, hintsVisible = false, statusVisible = false,
     helpVisible = false, qualVisible = false, unmoneyVisible = false,
     sameVisible = false;
bool spaceBlocked = false; // Space closed the shop and is still held
// _root.buy/_root.sell and the deal awaiting confirmation (confirm.chk).
bool confirmVisible = false;
int confirmChk = 0, buyFlag = 0, sellFlag = 0;
bool highQuality = true;
std::array<bool, 2> repairing{};
float cinematicAge = 0;
int introTick = 0; // shippos frames run so far (z = 20 per frame)
struct LandingPose {
  glm::vec2 ship{1581.35f, 1882.85f}, camera{};
  float scale = 2;
  bool landed = false;
};
LandingPose LandingAfter(int ticks) {
  LandingPose pose;
  for (int tick = 0; tick <= ticks; ++tick) {
    if (pose.ship.x > 900) {
      pose.ship.x -= (pose.ship.x - 900) / 100 + 1;
      pose.camera = pose.ship + glm::vec2(-20 / .75f, 60 / .75f);
    }
    if (pose.ship.x <= 900 && !pose.landed) {
      if (pose.scale > 1)
        pose.scale -= .01f;
      else
        pose.landed = true;
    }
  }
  return pose;
}
LandingPose LandingAt(float seconds) { return LandingAfter(int(seconds * 24)); }
// The pose drawn at `seconds`: between this frame's and the next.
LandingPose LandingBlend(float seconds) {
  int ticks = int(seconds * 24);
  float t = seconds * 24 - ticks;
  auto pose = LandingAfter(ticks), next = LandingAfter(ticks + 1);
  pose.ship = glm::mix(pose.ship, next.ship, t);
  pose.scale += (next.scale - pose.scale) * t;
  return pose;
}
// heroz sets the camera from the hero's position before it moves.
glm::vec2 CameraTarget() {
  return screen == Screen::Landing ? LandingAt(cinematicAge).camera
                                   : cameraFocus;
}
// The field's scroll, ceil(-focus * 3 / 4) as in the source, blended between
// steps so the view glides instead of moving 24 times a second.
glm::vec2 ViewShift() {
  auto shift = [](glm::vec2 focus) { return glm::ceil(-focus * .75f); };
  if (screen == Screen::Landing) {
    int ticks = int(cinematicAge * 24);
    return glm::mix(shift(LandingAfter(ticks).camera),
                    shift(LandingAfter(ticks + 1).camera),
                    cinematicAge * 24 - ticks);
  }
  if (viewBlend >= 1 || std::isnan(lastFocus.x))
    return shift(cameraFocus);
  return glm::mix(shift(lastFocus), shift(cameraFocus), viewBlend);
}
glm::vec2 FocusNow() {
  return viewBlend >= 1 || std::isnan(lastFocus.x)
             ? cameraFocus
             : glm::mix(lastFocus, cameraFocus, viewBlend);
}
// The Hall of Fame (Mochi's leaderboard in the original), kept locally.
struct HighScore {
  int score = 0;
  std::string name;
};
std::vector<HighScore> highScores;
// Frame 8's _root variables. point1-3 and total are fixed when the mission
// ends; yourname is the victory screen's input field, and it and point2 keep
// their values from one mission to the next.
int point1 = 0, point3 = 0, total = 0;
std::optional<int> point2; // undefined until the first victory
std::string yourname = "NONAME";
bool submitted = false; // Submit pressed for this victory
// The engine's text box edits yourname in place of Flash's input field.
std::unique_ptr<TextInputBox> nameBox;
std::string missing;
float UiScale() {
  auto size = GetCurrentWindowSize();
  return std::min(size.x / 550.f, size.y / 400.f);
}
glm::vec2 UiOrigin() {
  return (GetCurrentWindowSize() - glm::vec2(550, 400) * UiScale()) * .5f;
}
glm::vec2 UiPoint(glm::vec2 point) { return UiOrigin() + point * UiScale(); }
glm::vec2 UiMouse() { return (MousePixels() - UiOrigin()) / UiScale(); }
float WorldZoom() { return .75f * UiScale(); }
// Stage position of the field origin. Source: game._x = ceil(-x * 3 / 4) +
// Stage.width / 2 + 5 * cos(shake * pi / 2), y likewise with sin, then clamped.
// At rest the cosine term leaves the field 5px right of centre.
glm::vec2 FieldOrigin() {
  // AVM1 Stage.width/height include the letterboxed viewport in SWF units.
  // Root artwork still uses UiOrigin(), but the source camera centres on
  // these runtime dimensions rather than the movie's fixed 550x400 bounds.
  glm::vec2 stageSize = GetCurrentWindowSize() / UiScale();
  glm::vec2 origin = stageSize * .5f + ViewShift();
  if (screen == Screen::Landing)
    return origin;
  float phase = float(PI / 2) * shake;
  origin += 5.f * glm::vec2(std::cos(phase), std::sin(phase));
  return glm::clamp(origin, glm::vec2(-750, -1580), glm::vec2(25, 500));
}
glm::vec2 WorldPoint(glm::vec2 point) {
  return UiPoint(FieldOrigin() + point * .75f);
}
bool Over(Rect rect) { return CheckPointInRectangle(UiMouse(), rect); }
// Stage point where the current (or last) press began.
glm::vec2 pressPoint{NaN, NaN};
// The menus' buttons all act on(release): pressed and released over them.
bool Click(Rect rect) {
  return pointerReleased && Over(rect) &&
         CheckPointInRectangle(pressPoint, rect);
}
float Length(glm::vec2 v) { return std::sqrt(glm::dot(v, v)); }
void Sound(const std::string &name) {
  auto it = sounds.find(name);
  if (!muted && it != sounds.end())
    it->second->play();
}
// Flash's random(n): 0 to n - 1.
int Random(int n) { return RNG<int>::getRange(0, n - 1); }
void Camera() {
  camera = -UiPoint(FieldOrigin()) / WorldZoom();
}
// Stage space to window pixels, for drawing clips.
swf::Affine StageBase() {
  return swf::Affine::translate(UiOrigin()) *
         swf::Affine::scale(glm::vec2(UiScale()));
}
// The root timeline stopped at a scene's frame (0-based), for its buttons.
swf::Clip *Scene(int frame) {
  if (!swf::library.loaded())
    return nullptr;
  auto &clip = sceneClips[frame];
  if (!clip) {
    clip = std::make_unique<swf::Clip>(0);
    clip->gotoAndStop(frame);
  }
  return clip.get();
}
// The intro's "incoming transmission" (sprite 834, _root.warning on frame 6).
swf::Clip *Warning() {
  auto *scene = Scene(5);
  return scene ? scene->child("warning") : nullptr;
}
// Buttons draw their real over/down states (scene art is pre-rendered).
void SceneButtons(int frame) {
  if (auto *scene = Scene(frame))
    scene->drawButtons(StageBase());
}
// Draws a root frame live from the SWF, so its vectors and text are as sharp
// as Flash's. Flash draws the menus' static text a pixel right of and above
// its exported geometry (measured against the running Flash screens).
bool DrawScene(int frame) {
  auto *scene = Scene(frame);
  auto *root = swf::library.timeline(0);
  if (!scene || !root)
    return false;
  if (scene->depthOffsets.empty())
    for (const auto &p : root->frames[frame])
      if (auto *ch = swf::library.character(p.id);
          ch && ch->kind == swf::Kind::Text)
        scene->depthOffsets[p.depth] = {1, -1};
  scene->draw(StageBase());
  return true;
}
// Field (world) space to window pixels, for drawing clips.
swf::Affine WorldBase() {
  return swf::Affine::translate(UiPoint(FieldOrigin())) *
         swf::Affine::scale(glm::vec2(WorldZoom()));
}
// The hero's bounds in field space, as hitTest(game.hero0) sees them.
glm::vec4 HeroBounds() {
  if (player.clip)
    return swf::TransformBounds(swf::Affine::translate(player.pos),
                                player.clip->bounds());
  // Hero sprite 372 bounds.
  return {player.pos.x - 23.67f, player.pos.x + 20.5f, player.pos.y - 81.74f,
          player.pos.y + 34.28f};
}
std::shared_ptr<swf::Clip> MakeClip(const std::string &name) {
  int id = swf::library.exportId(name);
  return id < 0 ? nullptr : std::make_shared<swf::Clip>(id);
}
// attachMovie into the field, with the clip's _rotation/_xscale/onEnterFrame.
Effect *AddEffect(const std::string &name, glm::vec2 pos, float scale = 1,
                  float rotation = 0, Effect::Fade fade = Effect::Fade::None,
                  bool ground = false) {
  Effect e;
  e.clip = MakeClip(name);
  if (!e.clip)
    return nullptr;
  e.pos = pos;
  e.scale = scale;
  e.rotation = rotation;
  e.fade = fade;
  e.ground = ground;
  effects.push_back(std::move(e));
  return &effects.back();
}
void Play(swf::Clip *clip, const char *path) {
  if (clip)
    if (auto *c = clip->child(path))
      c->play();
}
const char *BuildingClipName(int i) {
  return i == 0 ? "CC"
                : (i < 3 ? "barack"
                         : (i < 5 ? "turret"
                                  : (i < 7 ? "planturret"
                                           : (i < 9 ? "nest" : "mainest"))));
}
float BuildingScale(int i) {
  return i == 3 || i == 4 ? 1.5f : (i == 5 || i == 6 ? 3.f : 1.f);
}
// status.att: the rifle's attack, which every hero shot also uses.
float HeroAttack() {
  const float attack[] = {20, 40, 80, 200};
  return attack[gun];
}
// The shop's look update (DefineButton2_119), from the equipment slots.
void ApplyHeroLook() {
  if (!player.clip)
    return;
  auto set = [&](const char *path, int frame) {
    if (auto *c = player.clip->child(path))
      c->gotoAndStop(frame);
  };
  for (const char *path :
       {"clip.head", "clip.eff.shield", "clip.back", "clip.eff.arm1",
        "clip.eff.arm2", "clip.eff.shoulder1", "clip.eff.shoulder2"})
    set(path, 0);
  for (int chk : slots) {
    if (!chk)
      continue;
    int tier = (chk - 1) % 3 + 1;
    switch ((chk - 1) / 3) {
    case 0: // Armor
      set("clip.head", tier);
      break;
    case 2: // Shield
      set("clip.eff.shield", tier);
      break;
    case 3: // Suit
      set("clip.back", tier);
      set("clip.eff.arm1", tier);
      set("clip.eff.arm2", tier);
      break;
    case 4: // Plating
      set("clip.eff.shoulder1", tier);
      set("clip.eff.shoulder2", tier);
      break;
    }
  }
  set("clip.cell", gun);
  set("clip.eff.cal", weapon);
}
void LoadArt(const std::string &key, const std::string &folder, int frame = 1) {
  auto p = std::filesystem::path("resources/game/sprites") / folder /
           (std::to_string(frame) + ".png");
  if (!std::filesystem::exists(p)) {
    missing += p.string() + "\n";
    return;
  }
  art[key] = std::make_unique<Texture>(p.string(), true, false, true);
}
void LoadScene(const std::string &key, int frame) {
  auto p = std::filesystem::path("resources/game/frames") /
           (std::to_string(frame) + ".png");
  if (!std::filesystem::exists(p)) {
    missing += p.string() + "\n";
    return;
  }
  art[key] = std::make_unique<Texture>(p.string(), true, false, true);
}
void Sprite(const std::string &key, glm::vec2 pos, glm::vec2 size,
            float angle = 0) {
  auto it = art.find(key);
  if (it == art.end())
    return;
  // The rotated texture overload already rotates about the rectangle centre.
  auto corner = pos - size * .5f;
  auto crop = crops.find(key);
  DrawRectangle(Rect(corner, size), angle, *it->second,
                crop == crops.end() ? nullptr : &crop->second);
}
void RegisteredSprite(const std::string &key, glm::vec2 registration,
                      float factor = 1, float angle = 0) {
  // Bounds recovered from DefineSprite/PlaceObject matrices, in SWF pixels.
  static const std::unordered_map<std::string, glm::vec2> origins = {
      {"field", {-84.1f, -728.3193f}},
      {"CC", {-100, -90}},
      {"barack", {-106, -96}},
      {"hero", {-23.669575f, -81.73652f}},
      {"enemy", {-22.70243f, -20.0024f}},
      {"fly", {-63.75375f, -37.84982f}},
      {"shooter", {-26.68596f, -26.98608f}},
      {"turret", {-69.1f, -35.85f}},
      {"planturret", {-33.3f, -31.3f}},
      {"nest", {-72.4f, -56.4f}},
      {"mainest", {-127.77305f, -127.78571f}},
      {"dropship", {-48.9f, -52.69053f}},
      {"dropshipBody", {-48.9f, -45.3f}},
      {"turretBase", {-35, -35}},
      {"turretRing", {-19.9f, -20.05f}},
      {"turretCross", {-27.3f, -27.25f}},
      {"cannon1", {-69.05f, -27.2f}},
      {"cannon2", {-69.05f, -27.2f}},
      {"cannon3", {-69.05f, -27.2f}},
      {"cannon4", {-69.05f, -27.2f}}};
  auto it = art.find(key);
  if (it == art.end())
    return;
  glm::vec2 top = origins.at(key),
            size = glm::vec2(it->second->width, it->second->height) * .5f;
  if (key == "turretRing")
    size = {40, 40};
  if (key == "turretCross")
    size = {54.65f, 54.55f};
  if (key == "dropshipBody")
    size = {97.75f, 88.25f};
  auto crop = crops.find(key);
  if (crop != crops.end()) {
    top += glm::vec2(crop->second.x,
                     it->second->height - crop->second.y - crop->second.w) *
           .5f;
    size = glm::vec2(crop->second.z, crop->second.w) * .5f;
  }
  auto centre = (top + size * .5f) * factor;
  float c = std::cos(angle), s = std::sin(angle);
  centre = {c * centre.x - s * centre.y, s * centre.x + c * centre.y};
  Sprite(key, WorldPoint(registration + centre), size * factor * WorldZoom(),
         angle);
}
void UiArt(const std::string &key, Rect rect) {
  Sprite(key, UiPoint(rect.position + rect.size * .5f), rect.size * UiScale());
}
void UiText(const std::string &text, float x, float y, float size = 10,
            glm::vec4 color = WHITE,
            TextAlignmentX align = TextAlignmentX::LEFT) {
  DrawText(text, UiPoint({x, y}), size * UiScale(), color, align);
}
void UiRect(Rect rect, glm::vec4 color) {
  DrawRectangle(Rect(UiPoint(rect.position), rect.size * UiScale()), color);
}
void Hover(Rect rect) {
  if (Over(rect)) {
    // Flash's over-state has a soft white rounded bevel, rather than a yellow
    // box.
    const glm::vec2 p = rect.position + glm::vec2(5, 2);
    const glm::vec2 size = rect.size - glm::vec2(10, 4);
    constexpr float radius = 4;
    std::vector<glm::vec2> outline;
    const glm::vec2 centres[] = {
        p + glm::vec2(size.x - radius, radius), p + size - glm::vec2(radius),
        p + glm::vec2(radius, size.y - radius), p + glm::vec2(radius)};
    for (int corner = 0; corner < 4; ++corner)
      for (int step = 0; step <= 8; ++step) {
        float angle = float(PI) * (-.5f + corner * .5f + step / 16.f);
        outline.push_back(
            UiPoint(centres[corner] +
                    radius * glm::vec2(std::cos(angle), std::sin(angle))));
      }
    UiRect(Rect(p, size), glm::vec4(1, 1, 1, .13f));
    for (int layer = 0; layer < 3; ++layer) {
      const float widths[] = {8, 5, 2};
      const float alphas[] = {.04f, .12f, .8f};
      for (size_t i = 0; i < outline.size(); ++i) {
        auto a = outline[i], b = outline[(i + 1) % outline.size()];
        DrawLine(a.x, a.y, b.x, b.y, glm::vec4(1, 1, 1, alphas[layer]),
                 widths[layer] * UiScale());
      }
    }
  }
}
bool IsBuilding(const Actor &a) {
  return &a >= buildings.data() && &a < buildings.data() + buildings.size();
}
int BuildingIndex(const Actor &a) { return int(&a - buildings.data()); }
// Records where everything stands as a step begins (or the clock restarts),
// so frames drawn before the next step can blend towards the result.
void SnapshotMotion() {
  auto actor = [](Actor &a) {
    a.lastPos = a.pos;
    a.lastRotation = a.rotation;
    a.lastCannon = a.cannon;
  };
  actor(player);
  for (auto &a : units)
    actor(a);
  for (auto &b : buildings)
    actor(b);
  for (auto &b : bullets)
    b.lastPos = b.pos;
  for (auto &e : effects) {
    e.lastPos = e.pos;
    e.lastRotation = e.rotation;
    e.lastScale = e.scale;
    e.lastAlpha = e.alpha;
  }
  lastFocus = cameraFocus;
  lastDropshipPos = dropshipPos;
  lastDropshipScale = dropshipScale;
}
void Reset() {
  player = {};
  player.pos = cameraFocus = {881.35f, 1962.85f};
  player.hp = player.maxHp = 500;
  player.armor = 1;
  player.radius = 18;
  player.friendly = true;
  cash = 100;
  level = kills = shake = 0;
  timer = 1000;
  energy = maxEnergy = 100;
  regen = 0;
  heroShock = 1;
  weapon = gun = 0;
  weaponRange = 500;
  warnTime = winCounter = loseCounter = 0;
  warnVisible = firing = false;
  accumulator = 0;
  slots.fill(0);
  specials.fill(false);
  turretLevel = {0, 1};
  units.clear();
  units.reserve(32);
  bullets.clear();
  effects.clear();
  const glm::vec2 positions[] = {{770, 2150},         {350, 2123.6f},
                                 {1250, 2123.6f},     {625, 1880},
                                 {1050, 1880},        {739, 36},
                                 {1182, 36},          {576.35f, -324.6f},
                                 {1331.45f, -324.6f}, {962.35f, -453.4f}};
  for (int i = 0; i < 10; ++i) {
    Actor b;
    b.pos = positions[i];
    b.friendly = i < 5;
    b.kind = b.slot = i;
    b.hp = b.maxHp = (i >= 3 && i <= 6) ? 2500.f : 10000.f * difficulty;
    b.armor = (i == 0 || i == 9) ? 100.f : ((i >= 3 && i <= 6) ? 20.f : 40.f);
    b.radius = (i >= 3 && i <= 6) ? 40.f : 90.f;
    b.clip = MakeClip(BuildingClipName(i));
    buildings[i] = b;
  }
  radarVisible = true;
  hintsVisible = statusVisible = helpVisible = qualVisible = unmoneyVisible =
      sameVisible = false;
  repairing.fill(false);
  fieldClip = swf::Clip::fromExport("field");
  helpClip = swf::Clip::fromExport("help");
  skipClip = swf::Clip::fromExport("skiper");
  statusClip = swf::Clip::fromExport("status");
  circleClip = swf::Clip::fromExport("circle");
  screenClip = swf::Clip::fromExport("screen");
  qualClip = swf::Clip::fromExport("qual");
  warnClip = swf::Clip::fromExport("warn");
  sameClip = swf::Clip::fromExport("same");
  unmoneyClip = swf::Clip::fromExport("unmoney");
  equipClip = swf::Clip::fromExport("equip");
  confirmClip = swf::Clip::fromExport("confirm");
  confirmVisible = false;
  confirmChk = buyFlag = sellFlag = 0;
  for (auto &icon : slotIcons)
    icon.reset();
  player.clip = MakeClip("hero");
  ApplyHeroLook();
  screen = Screen::Play;
  Camera();
  SnapshotMotion();
}
// dropp's jets by phase: flying in (long jet1, jet2 off), descending (jet2
// only), taking off again (jet1 only, moved back).
void PoseDropship(int phase) {
  if (!dropshipClip)
    return;
  swf::Clip *jet1 = dropshipClip->child("jet1"),
            *jet2 = dropshipClip->child("jet2");
  if (!jet1 || !jet2)
    return;
  if (phase == 0) {
    jet1->setScale({jet1->scale().x, .8f});
    jet2->gotoAndStop(1);
  } else if (phase == 1) {
    jet1->gotoAndStop(1);
    jet2->play();
  } else {
    jet1->play();
    jet2->gotoAndStop(1);
    jet1->setScale({jet1->scale().x, 1.f});
    jet1->setPosition({80, jet1->position().y});
  }
}
void BeginLanding() {
  Reset();
  cinematicAge = 0;
  dropshipClip = swf::Clip::fromExport("shipup");
  departing = false;
  screen = Screen::Landing;
  Sound("rocket2");
}
// The hero arrives with the help overlay open. A full landing (dropp) lets
// the dropship take off again; Skip and Play (DefineButton2_760) removes it.
void BeginMission(bool skipped) {
  auto pose = LandingAt(cinematicAge);
  dropshipPos = pose.ship;
  dropshipScale = pose.scale;
  departing = !skipped && dropshipClip != nullptr;
  if (skipped)
    dropshipClip.reset();
  else {
    Sound("rocket2");
    Sound("stup");
  }
  Sound("clk");
  screen = Screen::Play;
  helpVisible = hintsVisible = true;
  if (helpClip)
    helpClip->gotoAndStop(0);
  firing = false;
  accumulator = 0;
  SnapshotMotion();
}
struct Box {
  glm::vec2 min, max;
};
bool Overlaps(const Box &a, const Box &b) {
  return a.min.x < b.max.x && b.min.x < a.max.x && a.min.y < b.max.y &&
         b.min.y < a.max.y;
}
Box ToBox(glm::vec4 b) { return {{b.x, b.z}, {b.y, b.w}}; }
// Buildings are placed by their registration point, not their centre. Flash
// uses bounding boxes: hitTest(building) for shots and the "border" child for
// movement, both in SWF pixels before the building's scale.
Box BuildingBox(int i, bool border) {
  // CC, barack, turret, planturret, nest, mainest.
  static const Box clip[] = {{{-100, -90}, {213.8f, 218.25f}},
                             {{-106, -96}, {167.1f, 161.15f}},
                             {{-69.1f, -35.85f}, {36.15f, 35.15f}},
                             {{-33.3f, -31.3f}, {31.05f, 32.6f}},
                             {{-72.4f, -56.4f}, {72.4f, 77.33f}},
                             {{-127.77f, -127.79f}, {125.77f, 125.79f}}};
  static const Box borders[] = {clip[0],
                                clip[1],
                                {{-35.35f, -35.85f}, {36.15f, 35.15f}},
                                clip[3],
                                {{-65.07f, -54.53f}, {66.78f, 77.33f}},
                                clip[5]};
  int type = i == 0 ? 0 : (i < 3 ? 1 : (i < 5 ? 2 : (i < 7 ? 3 : (i < 9 ? 4 : 5))));
  float scale = BuildingScale(i);
  const Box &box = border ? borders[type] : clip[type];
  return {buildings[i].pos + box.min * scale, buildings[i].pos + box.max * scale};
}
// hitTest(clip): the clip's bounding box in field space. For buildings it is
// the live clip's, which grows and shrinks as a turret's cannon turns.
Box ClipBox(const Actor &a) {
  if (IsBuilding(a)) {
    int i = BuildingIndex(a);
    if (!a.clip)
      return BuildingBox(i, false);
    return ToBox(swf::TransformBounds(
        swf::Affine::translate(a.pos) *
            swf::Affine::scale(glm::vec2(BuildingScale(i))),
        a.clip->bounds()));
  }
  if (&a == &player)
    return ToBox(HeroBounds());
  if (a.clip) {
    auto m = swf::Affine::translate(a.pos);
    if (!a.friendly && a.kind == 2)
      m = m * swf::Affine::rotate(glm::radians(a.rotation));
    return ToBox(swf::TransformBounds(m, a.clip->bounds()));
  }
  return {a.pos - glm::vec2(a.radius), a.pos + glm::vec2(a.radius)};
}
// The "border" child that movement tests use. Flyers have none.
std::optional<Box> BorderBox(const Actor &a) {
  if (IsBuilding(a))
    return BuildingBox(BuildingIndex(a), true);
  bool flyer = !a.friendly && a.kind == 2;
  if (!a.clip)
    return flyer ? std::nullopt
                 : std::optional<Box>(Box{a.pos - glm::vec2(20.5f),
                                          a.pos + glm::vec2(20.5f)});
  swf::Clip *border = a.clip->child("border");
  if (!border)
    return std::nullopt;
  return ToBox(swf::TransformBounds(
      swf::Affine::translate(a.pos) * border->matrix, border->bounds()));
}
bool Touch(const std::optional<Box> &a, const std::optional<Box> &b) {
  return a && b && Overlaps(*a, *b);
}
// this.hitTest(target): the shot clip's bounding box, turned by its _rotation,
// where it stands after this frame's move, against the target's. Nothing is
// swept, so fast shots can step over thin targets, as in Flash.
bool Strikes(const Bullet &b, const Actor &a) {
  Box shot{b.pos, b.pos};
  if (b.clip)
    shot = ToBox(swf::TransformBounds(
        swf::Affine::translate(b.pos) *
            swf::Affine::rotate(glm::radians(b.rotation)),
        b.clip->bounds()));
  return Overlaps(shot, ClipBox(a));
}
// game["hero" + i], game["enemy" + i] and game["building" + i], or null once
// the clip is gone (hero 0 is the player).
Actor *Hero(int i) {
  if (i == 0)
    return player.removed ? nullptr : &player;
  for (auto &u : units)
    if (u.friendly && u.slot == i && !u.removed)
      return &u;
  return nullptr;
}
Actor *Enemy(int i) {
  for (auto &u : units)
    if (!u.friendly && u.slot == i && !u.removed)
      return &u;
  return nullptr;
}
Actor *Building(int i) {
  return i >= 0 && i < 10 && !buildings[i].removed ? &buildings[i] : nullptr;
}
// grid[2]: building 3 (the left turret) starts unmarked and is never marked,
// so aliens never pick it as a target.
bool Marked(int i) { return i != 3 && Building(i); }
float Dist(glm::vec2 from, const Actor *to) {
  return to ? Length(to->pos - from) : NaN;
}
void Hurt(Actor &a, float damage) { a.hp -= damage * 10.f / (10.f + a.armor); }
// Flash keeps _rotation within -180..180.
float Normalize(float degrees) { return std::remainder(degrees, 360.f); }
// clip._rotation = atan2(-dy, -dx) * 180 / pi + 90: facing a point.
float AimAt(glm::vec2 from, glm::vec2 to) {
  return glm::degrees(std::atan2(-(to.y - from.y), -(to.x - from.x))) + 90;
}
void SetRotation(Actor &a, float degrees) {
  a.rotation = Normalize(degrees);
  if (a.clip)
    if (auto *body = a.clip->child("clip"))
      body->setRotation(a.rotation);
}
void PlayPart(Actor &a, const char *path) { Play(a.clip.get(), path); }
glm::vec2 FieldPoint(const char *name, glm::vec2 fallback) {
  if (fieldClip)
    if (auto *c = fieldClip->child(name))
      return c->position();
  return fallback;
}
// hitTest(game.<pad>) between the hero's clip and a field pad.
bool Touching(const char *name) {
  if (!fieldClip || player.removed)
    return false;
  swf::Clip *pad = fieldClip->child(name);
  if (!pad)
    return false;
  return Overlaps(ToBox(HeroBounds()),
                  ToBox(swf::TransformBounds(pad->matrix, pad->bounds())));
}
void SpawnMarine(glm::vec2 at) {
  for (int i = 1; i <= 9; ++i)
    if (!Hero(i)) {
      Actor u;
      u.pos = at;
      u.friendly = true;
      u.slot = i;
      u.hp = u.maxHp = 500;
      u.armor = 1;
      u.clip = MakeClip("hero");
      units.push_back(std::move(u));
      return;
    }
}
// creep, creepshoot and creepfly: the first free enemy slot, lined up by slot
// number at porte1 (creeps, from either nest) or porte3.
void SpawnAlien(int kind) {
  for (int i = 0; i <= 9; ++i)
    if (!Enemy(i)) {
      Actor u;
      u.kind = kind;
      u.slot = i;
      glm::vec2 gate = kind == 0 ? FieldPoint("porte1", {572.5f, -200})
                                 : FieldPoint("porte3", {958.9f, -262.7f});
      u.pos = gate + glm::vec2(i * 20.f - 100, 0);
      u.armor = 4 * difficulty * (kind == 0 ? 1.f : (kind == 1 ? 1.5f : 2.f));
      u.hp = u.maxHp =
          (kind == 0 ? 1000.f : (kind == 1 ? 2000.f : 2500.f)) * difficulty;
      u.radius = kind == 2 ? 28.f : 20.f;
      u.clip = MakeClip(kind == 0 ? "enemy" : (kind == 1 ? "shooter" : "fly"));
      units.push_back(std::move(u));
      return;
    }
}
void Shoot(const char *clip, glm::vec2 at, float v, float rotation,
           Flight flight, Owner h, int mode = 0, int frame = -1) {
  Bullet b;
  b.pos = b.origin = at;
  b.v = v;
  b.rotation = rotation;
  b.flight = flight;
  b.h = h;
  b.mode = mode;
  b.clip = MakeClip(clip);
  if (b.clip && frame >= 0)
    b.clip->gotoAndStop(frame);
  bullets.push_back(std::move(b));
}
// Shots leave 10px from the shooter, turned by its rotation plus a skew.
glm::vec2 Muzzle(const Actor &a, float skew) {
  float r = glm::radians(a.rotation + skew);
  return a.pos + 10.f * glm::vec2(-std::sin(r), std::cos(r));
}
// Turret shots leave `reach` px out along the cannon (offset by `skew`) and
// fly along it: _rotation = cannon - 180.
void TurretShot(const Actor &a, float skew, float reach, const char *clip,
                float v, Flight flight, Owner h) {
  float r = glm::radians(a.cannon + skew);
  Shoot(clip, a.pos + reach * glm::vec2(std::sin(r), -std::cos(r)), v,
        a.cannon - 180, flight, h);
}
// clip.eff._y += a, with a alternating each frame: the firing shake.
void Jitter(Actor &a) {
  if (!a.clip)
    return;
  if (auto *eff = a.clip->child("clip.eff")) {
    a.jitter = -a.jitter;
    eff->setPosition(eff->position() + glm::vec2(0, a.jitter));
  }
}
// turret(): the cannon turns four degrees a frame toward its target.
void TurnCannon(Actor &a, glm::vec2 target) {
  float rot = glm::degrees(
      std::atan2(-(target.y - a.pos.y), -(target.x - a.pos.x)));
  float &c = a.cannon;
  if (std::abs(c - rot) < 180) {
    if (c < rot)
      c += 4;
    if (c > rot)
      c -= 4;
  } else {
    if (c < rot)
      c -= 4;
    if (c > rot)
      c += 4;
  }
  c = Normalize(c);
}
// The cannon parts each turret type plays when it fires (index: tier; 0 is
// the enemy plant turret).
void CannonRecoil(Actor &a, int tier) {
  static const std::vector<const char *> parts[] = {
      {"cannon.reload", "cannon.explode", "cannon.gun"},
      {"cannon.reload", "cannon.rail", "cannon.gun"},
      {"cannon.reload", "cannon.rail1", "cannon.rail2", "cannon.gun1",
       "cannon.gun2"},
      {"cannon.reload", "cannon.explode", "cannon.gun"},
      {"cannon.reload", "cannon.explode1", "cannon.explode2", "cannon.gun1",
       "cannon.gun2"}};
  for (const char *p : parts[std::clamp(tier, 0, 4)])
    Play(a.clip.get(), p);
}
// A rotated scorch mark that fades over 2000 frames (hole with hol).
void Scorch(glm::vec2 at) {
  AddEffect("hole", at, 1, float(Random(180)), Effect::Fade::Hole, true);
}
void Ouch() {
  const char *names[] = {"ouch", "ouch2", "ouch3"};
  Sound(names[Random(3)]);
}
// Impact clips of the hero's weapon, used for every rifle-speed hit.
const char *WeaponImpact(bool building) {
  const char *units[] = {"ball2", "green", "red", "purple"};
  const char *walls[] = {"ball4", "green", "red", "purple"};
  return building ? walls[gun] : units[gun];
}
// The eight walking directions (px, py) and their clip._rotation.
void Face(Actor &a, int px, int py) {
  static const float table[3][3] = {
      {135, 90, 45}, {180, 0, 0}, {-135, -90, -45}};
  if (px || py)
    SetRotation(a, table[px + 1][py + 1]);
}
// Walks toward the target one step per axis, outside a 2px dead zone.
void Approach(Actor &a, const Actor &t, float speed) {
  int px = 0, py = 0;
  if (t.pos.y - a.pos.y > 2) {
    a.pos.y += speed;
    py = 1;
  }
  if (t.pos.y - a.pos.y < -2) {
    a.pos.y -= speed;
    py = -1;
  }
  if (t.pos.x - a.pos.x > 2) {
    a.pos.x += speed;
    px = 1;
  }
  if (t.pos.x - a.pos.x < -2) {
    a.pos.x -= speed;
    px = -1;
  }
  Face(a, px, py);
}
enum class Check { Buildings, Units, Target };
// Steps one axis, notes what the border would touch in the original's order
// (the last match becomes gethit), then steps back. Units are aliens, or
// heroes for marines; touching another unit sets unitHit.
void Probe(Actor &a, const Actor *target, glm::vec2 step,
           std::initializer_list<Check> order, bool heroes, int &hit,
           int &unitHit, const Actor *&gethit, bool attack = false) {
  a.pos += step;
  auto border = BorderBox(a);
  for (Check check : order)
    switch (check) {
    case Check::Buildings:
      for (int i = 0; i < 10; ++i)
        if (Actor *b = Building(i); b && Touch(border, BorderBox(*b))) {
          hit = 1;
          gethit = b;
        }
      break;
    case Check::Units:
      for (int i = 0; i <= (heroes ? 10 : 9); ++i)
        if (Actor *u = heroes ? Hero(i) : Enemy(i);
            u && u != &a && Touch(border, BorderBox(*u))) {
          gethit = u;
          unitHit = 1;
        }
      break;
    case Check::Target:
      if (target && border && Overlaps(*border, ClipBox(*target))) {
        hit = 1;
        gethit = target;
        if (attack) { // enemiez bites what it walks into
          for (const char *p : {"clip.attack1", "clip.attack2", "clip.head"})
            PlayPart(a, p);
          if (a.clip)
            if (auto *leg = a.clip->child("clip.leg"))
              leg->stop();
        }
      }
      break;
    }
  a.pos -= step;
}
// Blocked: step around whatever was hit, toward the target's side of it.
void Sidestep(Actor &a, const Actor *t, const Actor *gethit, int hitx,
              int hity, float speed) {
  if (!t || !gethit)
    return;
  if (hitx == 0 && hity == 1) {
    if (t->pos.x - gethit->pos.x + (a.pos.x - gethit->pos.x) > 0) {
      a.pos.x += speed;
      SetRotation(a, -90);
    } else {
      a.pos.x -= speed;
      SetRotation(a, 90);
    }
  }
  if (hitx == 1) {
    if (t->pos.y - gethit->pos.y + (a.pos.y - gethit->pos.y) > 0) {
      a.pos.y += speed;
      SetRotation(a, 0);
    } else {
      a.pos.y -= speed;
      SetRotation(a, 180);
    }
  }
}
// shockx/shocky: knockback that bleeds off one unit a frame. Units pushed
// into a building are thrown back out of it.
void Knockback(Actor &a, float scale, bool bounce) {
  for (int axis = 0; axis < 2; ++axis) {
    float &s = a.shock[axis];
    if (std::abs(s) <= 1)
      continue;
    if (bounce)
      for (int i = 0; i < 10; ++i)
        if (Actor *b = Building(i); b && Touch(BorderBox(a), BorderBox(*b)))
          a.pos[axis] -= 2 * s;
    s += s > 0 ? -1 : 1;
    a.pos[axis] += s * scale;
  }
}
void Confine(Actor &a) {
  a.pos = glm::clamp(a.pos, glm::vec2(110, -525), glm::vec2(1570, 2480));
}
// enemiez, shooter and flyer: the nearest hero unless that is over 500 away,
// then the nearest marked friendly building (building 0 by default).
Actor *AlienTarget(const Actor &a) {
  int saven = 0;
  for (int n = 1; n <= 9; ++n)
    if (Hero(n) && AtMost(Dist(a.pos, Hero(n)) - Dist(a.pos, Hero(saven)), 0))
      saven = n;
  Actor *t = Hero(saven);
  if (Dist(a.pos, t) > 500 || !t) {
    saven = 0;
    for (int n = 1; n <= 4; ++n)
      if (Marked(n) &&
          AtMost(Dist(a.pos, Building(n)) - Dist(a.pos, Building(saven)), 0))
        saven = n;
    t = Building(saven);
  }
  return t;
}
// herodummy: the nearest alien in slots 0-8, unless over 500 away, then the
// nearest of buildings 5-9.
Actor *MarineTarget(const Actor &a) {
  int saven = 0;
  for (int n = 1; n <= 8; ++n)
    if (Enemy(n) && AtMost(Dist(a.pos, Enemy(n)) - Dist(a.pos, Enemy(saven)), 0))
      saven = n;
  Actor *t = Enemy(saven);
  if (Dist(a.pos, t) > 500 || !t) {
    saven = 5;
    for (int n = 6; n <= 9; ++n)
      if (AtMost(Dist(a.pos, Building(n)) - Dist(a.pos, Building(saven)), 0))
        saven = n;
    t = Building(saven);
  }
  return t;
}
// A dead creep, shooter or flyer frees its slot and drops money.
void AlienDies(Actor &a, int bounty) {
  a.removed = true;
  if (Effect *money = AddEffect("money", a.pos, 1, 0, Effect::Fade::Bling))
    money->clip->variables["cash"] = std::to_string(bounty);
  cash += bounty;
  ++kills;
  Sound("edeads");
  Sound(Random(2) == 0 ? "deadcreep" : "deadcreep2");
}
// enemiez: melee aliens.
void Creep(Actor &a) {
  constexpr float speed = 3.5f;
  Confine(a);
  Knockback(a, .5f, true);
  Actor *t = AlienTarget(a);
  if (t && Overlaps(ClipBox(a), ClipBox(*t)) && ++a.load > 5) {
    a.load = 0;
    Hurt(*t, 10);
  }
  PlayPart(a, "clip.leg");
  int hitx = 0, hity = 0;
  const Actor *gethit = nullptr;
  if (t) {
    auto order = {Check::Buildings, Check::Units, Check::Target};
    if (float dy = t->pos.y - a.pos.y; dy != 0)
      Probe(a, t, {0, dy > 0 ? speed : -speed}, order, false, hity, hity,
            gethit, true);
    if (float dx = t->pos.x - a.pos.x; dx != 0)
      Probe(a, t, {dx > 0 ? speed : -speed, 0}, order, false, hitx, hitx,
            gethit, true);
    if (!hitx && !hity)
      Approach(a, *t, speed);
  }
  if (gethit != t)
    Sidestep(a, t, gethit, hitx, hity, speed);
  if (a.hp <= 0) {
    AlienDies(a, 35 + Random(25));
    AddEffect("edead", a.pos, 1, float(Random(360)), Effect::Fade::Hole, true);
  }
}
// shooter: ranged aliens that stop 300 away.
void AlienShooter(Actor &a) {
  constexpr float speed = 3.5f;
  Confine(a);
  Knockback(a, .5f, true);
  Actor *t = AlienTarget(a);
  float r = Dist(a.pos, t);
  PlayPart(a, "clip.leg");
  int hitx = 0, hity = 0;
  const Actor *gethit = nullptr;
  if (t) {
    auto order = {Check::Units, Check::Target, Check::Buildings};
    if (float dy = t->pos.y - a.pos.y; dy != 0)
      Probe(a, t, {0, dy > 0 ? speed : -speed}, order, false, hity, hity,
            gethit);
    // Walking left, the original flags other aliens as a vertical block.
    if (float dx = t->pos.x - a.pos.x; dx != 0)
      Probe(a, t, {dx > 0 ? speed : -speed, 0}, order, false, hitx,
            dx > 0 ? hitx : hity, gethit);
  }
  bool clear = !hitx && !hity;
  if (clear && r > 300) {
    Approach(a, *t, speed);
    for (const char *p : {"clip.attack1", "clip.attack2", "clip.tail"})
      PlayPart(a, p);
  }
  if ((clear && AtMost(r, 300)) || (t && gethit == t)) {
    for (const char *p : {"clip.head", "clip.attack1", "clip.attack2", "clip.tail"})
      PlayPart(a, p);
    if (++a.load > 15) {
      a.load = 0;
      for (const char *p : {"clip.head", "clip.attack1", "clip.attack2", "clip.blas"})
        PlayPart(a, p);
      Sound("atcrot");
      Shoot("bull5", Muzzle(a, 0), 10, a.rotation + 90, Flight::Shotgnd,
            Owner::None);
    }
    if (t)
      SetRotation(a, AimAt(a.pos, t->pos));
  }
  if (gethit != t) {
    for (const char *p : {"clip.attack1", "clip.attack2", "clip.tail"})
      PlayPart(a, p);
    Sidestep(a, t, gethit, hitx, hity, speed);
  }
  if (a.hp <= 0) {
    AlienDies(a, 55 + Random(25));
    AddEffect("edead", a.pos, 1, float(Random(360)), Effect::Fade::Hole, true);
  }
}
// flyer: hovers 400 away and shoots; ignores collisions.
void Flyer(Actor &a) {
  Confine(a);
  Knockback(a, 1, false);
  if (Actor *t = AlienTarget(a)) {
    a.rotation = Normalize(AimAt(a.pos, t->pos));
    if (Dist(a.pos, t) > 400) {
      for (const char *p : {"head", "attack"})
        if (a.clip)
          if (auto *c = a.clip->child(p))
            c->gotoAndStop(0);
      // The original hands _rotation, in degrees, to Math.cos and Math.sin,
      // which take radians, so flyers wander rather than fly straight.
      a.pos.x -= 4 * std::cos(a.rotation);
      a.pos.y -= 4 * std::sin(a.rotation);
    } else if (++a.load > 20) {
      a.load = 0;
      for (const char *p : {"head", "attack", "blas"})
        PlayPart(a, p);
      Sound("fire");
      Shoot("bull3", Muzzle(a, 5), 10, a.rotation + 90, Flight::Shotfly,
            Owner::None);
    }
  }
  if (a.hp <= 0) {
    AlienDies(a, 85 + Random(25));
    AddEffect("ball", a.pos);
  }
}
// herodummy: the barracks' marines.
void Marine(Actor &a) {
  constexpr float speed = 3;
  Knockback(a, 1, true);
  Actor *t = MarineTarget(a);
  float r = Dist(a.pos, t);
  int hitx = 0, hity = 0;
  const Actor *gethit = nullptr;
  if (t) {
    auto order = {Check::Target, Check::Buildings, Check::Units};
    if (float dy = t->pos.y - a.pos.y; dy != 0)
      Probe(a, t, {0, dy > 0 ? speed : -speed}, order, true, hity, hity,
            gethit);
    if (float dx = t->pos.x - a.pos.x; dx != 0)
      Probe(a, t, {dx > 0 ? speed : -speed, 0}, order, true, hitx, hitx,
            gethit);
  }
  bool clear = !hitx && !hity;
  if (clear && r > 500) {
    Approach(a, *t, speed);
    PlayPart(a, "clip.leg");
  }
  if ((clear && AtMost(r, 500)) || (t && gethit == t)) {
    if (a.clip)
      if (auto *leg = a.clip->child("clip.leg"))
        leg->gotoAndStop(0);
    if (!a.voiced) {
      const char *voices[] = {"man1", "mann1", "mann2"};
      Sound(voices[Random(3)]);
      a.voiced = true;
    }
    ++a.load;
    Jitter(a);
    if (a.load > 5) {
      Sound("macgun");
      a.load = 0;
      PlayPart(a, "clip.gun");
      Shoot("bull", Muzzle(a, 5), 50, a.rotation + 90, Flight::Shot,
            Owner::Dummy);
    }
    if (t)
      SetRotation(a, AimAt(a.pos, t->pos));
  }
  if (gethit != t) {
    PlayPart(a, "clip.leg");
    Sidestep(a, t, gethit, hitx, hity, speed);
  }
  if (a.hp <= 0) {
    a.removed = true;
    AddEffect("herodead", a.pos, 1, float(Random(360)), Effect::Fade::Hole,
              true);
    const char *cries[] = {"herodead2", "herodead3", "herodead4"};
    Sound(cries[Random(3)]);
  }
  Confine(a);
}
void ManVoice() {
  const char *voices[] = {"man2", "man3", "man4", "man5"};
  Sound(voices[Random(4)]);
}
// barak: production and destruction of the non-turret buildings.
void Barak(int i) {
  auto &b = buildings[i];
  if (i == 1 && timer == 1200) {
    ManVoice();
    SpawnMarine(FieldPoint("port1", {383.25f, 1982.8f}));
  }
  if (i == 2 && timer == 1150) {
    ManVoice();
    SpawnMarine(FieldPoint("port2", {1281.35f, 1982.85f}));
  }
  if (i == 9 && timer == 1200)
    for (int threshold : {6, 12, 20, 30})
      if (level > threshold)
        SpawnAlien(2);
  if ((i == 7 || i == 8) && timer == 1200) {
    SpawnAlien(0);
    SpawnAlien(0);
    if (level > (i == 7 ? 2 : 5))
      SpawnAlien(1);
  }
  if (b.hp > 0)
    return;
  b.hp = 0;
  b.removed = true;
  if (i <= 2) {
    AddEffect("hole", b.pos, 3, 0, Effect::Fade::Hole, true);
    AddEffect("exp", b.pos);
    Sound("exps");
  } else {
    AddEffect("plantdead", b.pos, 1, 0, Effect::Fade::Hole, true);
    AddEffect("ball", b.pos, 3);
    Sound("deadcreep2");
  }
  if (i == 9) {
    winCounter = 50;
    Sound("win");
  }
  if (i == 0) {
    loseCounter = 50;
    Sound("defeat");
  }
}
// turret(): construction, targeting, firing and destruction of the four
// turrets (3-4 ours, 5-6 the aliens' plant turrets).
void Turret(int i) {
  auto &a = buildings[i];
  if (a.building) {
    // Frame 2 shows the load counter; the turret is idle and cannot be
    // destroyed until it finishes, about 1000 frames later.
    if (a.clip && a.clip->frame != 1)
      a.clip->gotoAndStop(1);
    a.progress += .1f;
    int loading = int(std::ceil(a.progress));
    if (a.clip)
      if (auto *load = a.clip->child("load")) {
        load->variables["loading"] = std::to_string(loading);
        load->gotoAndStop(std::max(0, int(std::floor(a.progress * .1f)) - 1));
      }
    if (loading > 100) {
      a.hp = 2500;
      Sound("up");
      a.progress = 0;
      if (a.clip)
        a.clip->gotoAndStop(0);
      a.building = false;
    }
    return;
  }
  bool ours = i <= 4;
  int saven = 0;
  for (int n = 0; n <= 9; ++n) {
    Actor *c = ours ? Enemy(n) : Hero(n);
    if (c && AtMost(Dist(a.pos, c) - Dist(a.pos, ours ? Enemy(saven) : Hero(saven)), 0))
      saven = n;
  }
  Actor *t = ours ? Enemy(saven) : Hero(saven);
  if (Dist(a.pos, t) < 700) {
    if (!ours) {
      if (++a.load >= 20) {
        a.load = 0;
        CannonRecoil(a, 0);
        Sound("shote");
        TurretShot(a, -90, 50, "bullz", 40, Flight::Shotur, Owner::None);
      }
    } else if (int typ = turretLevel[i - 3]; typ > 0 && ++a.load >= (typ >= 3 ? 100 : 5)) {
      // typ 0, the unbuilt left turret, only tracks.
      a.load = 0;
      CannonRecoil(a, typ);
      switch (typ) {
      case 1:
        Sound("macgun2");
        TurretShot(a, -90, 100, "bull", 50, Flight::Shot, Owner::Dummy);
        break;
      case 2:
        for (float skew : {-75.f, -105.f}) {
          Sound("macgun");
          TurretShot(a, skew, 100, "bull", 50, Flight::Shot, Owner::Dummy);
        }
        break;
      case 3:
        Sound("sho");
        TurretShot(a, -90, 50, "bull4", 40, Flight::Shot, Owner::None);
        break;
      default:
        for (float skew : {-100.f, -80.f}) {
          Sound("sho");
          TurretShot(a, skew, 50, "bull4", 40, Flight::Shot, Owner::None);
        }
      }
    }
    TurnCannon(a, t->pos);
  }
  if (a.hp <= 0) {
    a.removed = true;
    if (ours) {
      AddEffect("hole", a.pos, 5, 0, Effect::Fade::Hole, true);
      AddEffect("exp", a.pos);
    } else {
      AddEffect("plantdead", a.pos, 1, 0, Effect::Fade::Hole, true);
      AddEffect("ball", a.pos, 3);
      Sound("deadcreep2");
    }
    Sound("exps");
  }
}
// Opening a shop pauses the game and shows the status panel: equip frame 1
// (equipment) at the armory, frame 4 (turrets) at a turret pad.
void CancelDeal() {
  confirmVisible = false;
  confirmChk = buyFlag = sellFlag = 0;
}
void OpenShop(int turret) {
  CancelDeal();
  unmoneyVisible = sameVisible = false;
  selectedTurret = turret;
  if (equipClip)
    equipClip->gotoAndStop(turret >= 0 ? 3 : 0);
  screen = Screen::Shop;
  statusVisible = true;
  firing = false;
  Sound("move");
}
// The shop's close button (DefineButton2_551) or P.
void CloseShop() {
  CancelDeal();
  unmoneyVisible = sameVisible = false;
  armedButton = nullptr;
  screen = Screen::Play;
  statusVisible = false;
  spaceBlocked = IsKeyDown(GLFW_KEY_SPACE);
  firing = false;
  accumulator = 0;
  SnapshotMotion();
  Sound("clk");
}
// The hero's death (end of heroz): a corpse, and defeat 50 frames later.
void HeroDeath() {
  if (player.removed || player.hp > 0)
    return;
  player.hp = 0;
  player.removed = true;
  AddEffect("herodead", player.pos, 1, float(Random(360)), Effect::Fade::Hole,
            true);
  const char *cries[] = {"herodead2", "herodead3", "herodead4"};
  Sound(cries[Random(3)]);
  Sound("herodead2");
  loseCounter = 50;
  Sound("defeat");
}
// heroz: the wave clock, turret repair, pads, weapons, camera and movement.
void Heroz() {
  Actor &h = player;
  if (level == 0 && timer < 900)
    timer = 1000;
  for (int i = 0; i < 2; ++i)
    if (Actor *t = Building(3 + i)) {
      if (t->hp == 0 || cash <= 0 || t->hp >= 2500)
        repairing[i] = false;
      if (repairing[i] && t->hp < 2500) {
        t->hp += 5;
        --cash;
      }
    }
  warnVisible = warnTime > 0;
  if (warnTime > 0)
    --warnTime;
  ++timer;
  if (timer == 1125 || timer == 1140)
    Sound("alarm");
  if (timer == 1201) {
    timer = 0;
    cash += 50 * level;
    ++level;
  }
  if (energy < maxEnergy)
    energy += regen;
  // Field pads play while the hero's clip overlaps them.
  bool space = IsKeyDown(GLFW_KEY_SPACE) && !spaceBlocked;
  if (fieldClip) {
    // Flash runs frame scripts after onEnterFrame, so a pad whose stop() runs
    // this frame overrides the hero's play() and holds its first frame for one
    // more tick (a 21-tick pulse, as measured in Flash Player).
    const char *names[] = {"upgrade", "mech", "arrow", "tur1", "tur2"};
    std::map<std::string, bool> wasPlaying;
    for (const char *name : names)
      if (auto *clip = fieldClip->child(name))
        wasPlaying[name] = clip->playing;
    std::map<std::string, bool> touching;
    for (const char *name : {"upgrade", "mech", "tur1", "tur2"})
      touching[name] = Touching(name);
    fieldClip->tick();
    auto play = [&](const char *name) {
      swf::Clip *clip = fieldClip->child(name);
      if (clip && (clip->playing || !wasPlaying[name]))
        clip->play();
    };
    if (touching["upgrade"]) {
      play("upgrade");
      if (space)
        OpenShop(-1);
    }
    if (touching["mech"]) {
      play("mech");
      play("arrow");
      if (h.hp < h.maxHp - 1)
        ++h.hp;
      if (energy < maxEnergy)
        ++energy;
    }
    if (touching["tur1"]) {
      play("tur1");
      if (space && Building(3) && buildings[3].hp > 0)
        OpenShop(3);
    }
    if (touching["tur2"] && Building(4) && buildings[4].hp > 0) {
      play("tur2");
      if (space)
        OpenShop(4);
    }
  }
  if (firing) {
    int mode = weapon + 1;
    if (mode == 1) {
      ++h.load;
      Jitter(h);
      if (h.load >= 5) {
        const char *sounds[] = {"macgun", "laser1", "laser2", "laser3"};
        Sound(sounds[gun]);
        h.load = 0;
        PlayPart(h, gun ? "clip.gun2" : "clip.gun");
        Shoot("bull", Muzzle(h, 10), 50, h.rotation + 90, Flight::Shot,
              Owner::Hero, mode, gun);
      }
    } else if (++h.load >= (mode == 2 ? 20 : 30)) {
      h.load = 0;
      if (energy < 25.f * mode) {
        Sound("err");
        warnTime = 40;
      } else {
        const char *sounds[] = {"gnd", "sho", "launc"},
                   *clips[] = {"grenade", "bull4", "rocket"};
        const float speeds[] = {10, 30, 20};
        PlayPart(h, "clip.gun2");
        if (mode == 4)
          PlayPart(h, "clip.brus");
        Sound(sounds[mode - 2]);
        energy -= 25.f * mode;
        Shoot(clips[mode - 2], Muzzle(h, 10), speeds[mode - 2],
              h.rotation + 90, Flight::Shot, Owner::Hero, mode, gun);
      }
    }
  }
  aim = MousePixels() / WorldZoom() + camera;
  SetRotation(h, AimAt(h.pos, aim));
  if (shake > 0)
    --shake;
  cameraFocus = h.pos;
  Confine(h);
  if (h.clip)
    if (auto *leg = h.clip->child("clip.leg"))
      leg->stop();
  Knockback(h, heroShock, true);
  auto blocked = [&] {
    auto border = BorderBox(h);
    for (int i = 0; i <= 10; ++i) {
      Actor *b = Building(i), *e = Enemy(i), *m = Hero(i + 1);
      if ((b && Touch(border, BorderBox(*b))) ||
          (e && Touch(border, BorderBox(*e))) ||
          (m && Touch(border, BorderBox(*m))))
        return true;
    }
    return false;
  };
  auto walk = [&](bool held, glm::vec2 step) {
    if (!held)
      return;
    h.pos += step;
    PlayPart(h, "clip.leg");
    if (blocked())
      h.pos -= step;
  };
  walk(IsKeyDown(GLFW_KEY_D) || IsKeyDown(GLFW_KEY_RIGHT), {4, 0});
  walk(IsKeyDown(GLFW_KEY_A) || IsKeyDown(GLFW_KEY_LEFT), {-4, 0});
  walk(IsKeyDown(GLFW_KEY_W) || IsKeyDown(GLFW_KEY_UP), {0, -4});
  walk(IsKeyDown(GLFW_KEY_S) || IsKeyDown(GLFW_KEY_DOWN), {0, 4});
  HeroDeath();
}
void MoveBullet(Bullet &b) {
  float r = glm::radians(b.rotation);
  b.pos += b.v * glm::vec2(std::cos(r), std::sin(r));
  if (b.clip)
    b.clip->tick();
}
float Travelled(const Bullet &b) { return Length(b.origin - b.pos); }
glm::vec2 Heading(const Bullet &b) {
  float r = glm::radians(b.rotation);
  return {std::cos(r), std::sin(r)};
}
// shot: the hero's, marines' and our turrets' projectiles.
void Shot(Bullet &b) {
  if (b.mode == 4) // Rockets trail smoke
    AddEffect("smoke", b.pos, 1, float(Random(180)), Effect::Fade::Smoke, true);
  MoveBullet(b);
  float d = Travelled(b);
  if (d > 500 && b.v == 50) {
    b.gone = true;
    return;
  }
  // Explosives burst at the selected weapon's range (weaponrange) or 700.
  bool burst = d > weaponRange && b.v != 50 && b.h == Owner::Hero;
  if (burst || d > 700) {
    Scorch(b.pos);
    AddEffect("exp", b.pos, burst ? .33f : .25f);
    Sound("exps");
    b.gone = true;
    return;
  }
  // Terminator shells (h undefined) pass through buildings.
  for (int i = 0; i < 10 && b.h != Owner::None; ++i) {
    Actor *t = Building(i);
    if (!t || !Strikes(b, *t))
      continue;
    if (i > 4)
      Hurt(*t, b.h == Owner::Hero ? HeroAttack() : 20.f);
    // Explosives also deal the rocket's 500 to any building, ours included.
    if (b.h == Owner::Hero && b.mode >= 2) {
      AddEffect("exp", b.pos, .5f);
      Sound("exps");
      Hurt(*t, 500);
    } else
      AddEffect(b.h == Owner::Dummy ? "ball4" : WeaponImpact(true), b.pos);
    b.gone = true;
    return;
  }
  for (int i = 0; i <= 10; ++i) {
    Actor *e = Enemy(i);
    if (!e || !Strikes(b, *e))
      continue;
    if (b.v == 50)
      AddEffect(WeaponImpact(false), b.pos);
    else {
      // Explosions throw back and damage every alien within 75.
      Scorch(b.pos);
      for (int j = 0; j <= 10; ++j)
        if (Actor *s = Enemy(j); s && Length(s->pos - b.pos) < 75) {
          s->shock = b.v * 2 * Heading(b);
          bool hero = b.h == Owner::Hero;
          Hurt(*s, hero && b.mode == 2 ? 100.f
                                       : (hero && b.mode == 3 ? 200.f : 500.f));
        }
      AddEffect("exp", b.pos, .5f);
      Sound("exps");
    }
    if (b.h == Owner::Hero)
      Hurt(*e, HeroAttack());
    if (b.h == Owner::Dummy)
      Hurt(*e, 20);
    if (Random(2) == 0)
      Sound("hurt");
    b.gone = true;
    return;
  }
}
// shotur: the plant turrets' shells; they only hit heroes.
void Shotur(Bullet &b) {
  MoveBullet(b);
  if (Travelled(b) > 700) {
    Scorch(b.pos);
    AddEffect("ballu", b.pos);
    Sound("exps");
    b.gone = true;
    return;
  }
  for (int i = 0; i <= 10; ++i) {
    Actor *t = Hero(i);
    if (!t || !Strikes(b, *t))
      continue;
    t->shock = b.v * .5f * Heading(b);
    if (i == 0)
      shake = 8;
    Ouch();
    Scorch(b.pos);
    AddEffect("ballu", b.pos);
    Hurt(*t, 200);
    Sound("exps");
    b.gone = true;
    return;
  }
}
// shotfly: flyer spit.
void Shotfly(Bullet &b) {
  MoveBullet(b);
  for (int i = 0; i < 9; ++i) {
    Actor *t = Building(i);
    if (!t || !Strikes(b, *t))
      continue;
    if (i < 5)
      Hurt(*t, 100);
    Sound("fire2");
    AddEffect("ball3", b.pos);
    b.gone = true;
    return;
  }
  for (int i = 0; i < 9; ++i) {
    Actor *t = Hero(i);
    if (!t || !Strikes(b, *t))
      continue;
    Hurt(*t, 100);
    t->shock = b.v * 1.5f * Heading(b);
    if (i == 0)
      shake = 8;
    Ouch();
    Scorch(b.pos);
    Sound("fire2");
    AddEffect("ball3", b.pos);
    b.gone = true;
    return;
  }
  if (Travelled(b) > 400) {
    Sound("fire2");
    Scorch(b.pos);
    AddEffect("ball3", b.pos);
    b.gone = true;
  }
}
// shotgnd: shooter acid.
void Shotgnd(Bullet &b) {
  MoveBullet(b);
  for (int i = 0; i < 9; ++i) {
    Actor *t = Building(i);
    if (!t || !Strikes(b, *t))
      continue;
    if (i < 5)
      Hurt(*t, 30);
    AddEffect("ball", b.pos);
    b.gone = true;
    return;
  }
  for (int i = 0; i <= 10; ++i) {
    Actor *t = Hero(i);
    if (!t || !Strikes(b, *t))
      continue;
    Hurt(*t, 30);
    Ouch();
    Sound("crot");
    AddEffect("ball", b.pos);
    b.gone = true;
    return;
  }
  if (Travelled(b) > 350) {
    Sound("crot");
    AddEffect("ball", b.pos);
    b.gone = true;
  }
}
// Per-frame clip state for buildings: health bars, turret cannons. Clips
// advance only when `tick` is set (simulation steps).
void PoseBuildings(bool tick = true) {
  for (int i = 0; i < 10; ++i) {
    auto &b = buildings[i];
    if (!b.clip || b.removed)
      continue;
    bool turret = i >= 3 && i <= 6;
    if (!b.building) {
      if (auto *bar = b.clip->child("bar"))
        bar->setWidth(b.hp / b.maxHp * (turret ? 50.f : 100.f));
      if (turret)
        if (auto *cannon = b.clip->child("cannon")) {
          if (i <= 4) {
            // cannon.gotoAndStop(typ), with frame 5 empty for an unbuilt turret.
            int tier = turretLevel[i - 3], frame = tier ? tier - 1 : 4;
            if (cannon->frame != frame)
              cannon->gotoAndStop(frame);
          }
          cannon->setRotation(b.cannon);
        }
    }
    if (tick)
      b.clip->tick();
  }
}
void Step() {
  SnapshotMotion();
  if (!player.removed)
    Heroz();
  for (int i = 0; i < 10; ++i)
    if (!buildings[i].removed) {
      if (i >= 3 && i <= 6)
        Turret(i);
      else
        Barak(i);
    }
  // Scripts set a cannon's _rotation and a bar's _width at once, so later
  // hitTests this frame see them (and not the pose last drawn between steps).
  PoseBuildings(false);
  // Units neither spawn units nor die mid-loop, so the references hold.
  for (size_t i = 0; i < units.size(); ++i) {
    Actor &a = units[i];
    if (a.removed)
      continue;
    if (a.friendly)
      Marine(a);
    else if (a.kind == 0)
      Creep(a);
    else if (a.kind == 1)
      AlienShooter(a);
    else
      Flyer(a);
  }
  for (auto &b : bullets) {
    switch (b.flight) {
    case Flight::Shot:
      Shot(b);
      break;
    case Flight::Shotur:
      Shotur(b);
      break;
    case Flight::Shotfly:
      Shotfly(b);
      break;
    case Flight::Shotgnd:
      Shotgnd(b);
      break;
    }
  }
  std::erase_if(bullets, [](const Bullet &b) { return b.gone; });
  std::erase_if(units, [](const Actor &a) { return a.removed; });
  if (player.clip && !player.removed)
    player.clip->tick();
  for (auto &a : units)
    if (a.clip)
      a.clip->tick();
  PoseBuildings();
  if (departing && dropshipClip) {
    // dropp after touchdown: grow back to 200%, then head west and leave.
    if (dropshipScale < 2)
      dropshipScale += .01f;
    if (dropshipScale > 1.95f) {
      PoseDropship(2);
      dropshipPos.x -= std::abs(dropshipPos.x - 900) / 50 + 1;
    }
    dropshipClip->tick();
    if (dropshipPos.x < 0) {
      departing = false;
      dropshipClip.reset();
    }
  }
  // onEnterFrame fades: hol (scorch marks, corpses), bling (money), smok.
  for (auto &e : effects) {
    e.clip->tick();
    switch (e.fade) {
    case Effect::Fade::Hole:
      e.alpha -= .05f;
      break;
    case Effect::Fade::Bling:
      e.alpha -= 2;
      e.pos.y -= 2;
      break;
    case Effect::Fade::Smoke:
      e.alpha -= 2;
      e.pos.y -= 1;
      e.scale += .01f;
      e.rotation += 25;
      break;
    case Effect::Fade::None:
      break;
    }
  }
  std::erase_if(effects, [](const Effect &e) {
    return !e.clip || e.clip->removed || e.alpha <= 0;
  });
  // Keep long-lived scorch marks and corpses bounded.
  size_t ground = std::count_if(effects.begin(), effects.end(),
                                [](const Effect &e) { return e.ground; });
  for (auto it = effects.begin(); ground > 150 && it != effects.end();)
    if (it->ground) {
      it = effects.erase(it);
      --ground;
    } else
      ++it;
}
// Frame 8's script: point2 = point2 < 40 ? 40 - score2 : 0, then total =
// point1 + point2 * 1000 + point3 * 100. On the first victory point2 is still
// undefined, and AVM1's undefined < 40 is false, so that bonus is 0; later
// victories (point2 now set) earn 40 - waves.
void EndMission(bool won) {
  screen = won ? Screen::Won : Screen::Lost;
  if (nameBox) {
    nameBox->hasFocus = false;
    nameBox->text = yourname;
    nameBox->clearSelection();
  }
  if (!won)
    return;
  point1 = cash;
  point2 = point2 && *point2 < 40 ? 40 - level : 0;
  point3 = kills;
  total = point1 + *point2 * 1000 + point3 * 100;
  submitted = false;
}
void SaveScores() {
  std::ofstream file("super-marine-scores.txt");
  for (const auto &entry : highScores)
    file << entry.score << ' ' << entry.name << '\n';
}
// Submit (DefineButton2_846) posts total and yourname to the leaderboard.
void SubmitScore() {
  if (submitted)
    return;
  submitted = true;
  highScores.push_back({total, yourname});
  std::stable_sort(highScores.begin(), highScores.end(),
                   [](const HighScore &a, const HighScore &b) {
                     return a.score > b.score;
                   });
  if (highScores.size() > 10)
    highScores.resize(10);
  SaveScores();
}
// checker: the mission ends 49 frames after the hive or the hero/command
// centre falls. It keeps counting while the game is paused.
void Checker() {
  if (winCounter > 0 && --winCounter == 1) {
    Sound("win");
    EndMission(true);
  }
  if (loseCounter > 0 && --loseCounter == 1 && screen != Screen::Won) {
    Sound("defeat");
    EndMission(false);
  }
}
void ApplyItem(const Item &item, float sign) {
  player.armor += sign * item.armor;
  player.maxHp += sign * item.hp;
  maxEnergy += sign * item.energy;
  regen += sign * item.regen;
}
void Unmoney() {
  unmoneyVisible = true;
  Sound("err");
}
void Same(const std::string &con, const std::string &txt) {
  sameVisible = true;
  Sound("err");
  if (sameClip) {
    sameClip->variables["con"] = con;
    sameClip->variables["txt"] = txt;
  }
}
bool Owned(int chk) {
  return std::find(slots.begin(), slots.end(), chk) != slots.end();
}
void SellItem(int chk) {
  const auto &item = items[chk - 1];
  for (auto &s : slots)
    if (s == chk)
      s = 0;
  cash += item.price / 2;
  ApplyItem(item, -1);
  if (item.sellShock > 0)
    heroShock = item.sellShock;
  ApplyHeroLook();
}
std::optional<Deal> DealFor(int chk) {
  if (chk >= 1 && chk <= 18)
    return Deal{items[chk - 1].name, items[chk - 1].info, items[chk - 1].price};
  if (chk >= 25 && chk <= 28)
    return turretDeals[chk - 25];
  if (chk >= 29 && chk <= 34)
    return weaponDeals[chk - 29];
  return std::nullopt;
}
// The shop grids (DefineButton2_547, 562, 570) are one button each; the item
// is picked from the pointer's position in the equip clip.
int GridItem(int button, glm::vec2 local) {
  if (button == 570)
    return int(std::ceil(local.x / 80)) + 2 * int(std::ceil((local.y - 100) / 80)) + 24;
  return int(std::ceil(local.x / 50)) + 3 * int(std::ceil((local.y - 70) / 50)) +
         (button == 562 ? 28 : 0);
}
void SetConfirm(const std::string &con, const char *txt) {
  confirmVisible = true;
  if (!confirmClip)
    return;
  confirmClip->variables["con"] = con;
  if (txt)
    confirmClip->variables["txt"] = txt;
}
// A grid release: too little cash shows unmoney, otherwise the buy is offered
// for confirmation (unless a sale is pending or unmoney is still up).
void OfferDeal(int chk) {
  auto deal = DealFor(chk);
  if (!deal || confirmVisible || unmoneyVisible || sameVisible)
    return;
  if (cash < deal->price) {
    Unmoney();
    return;
  }
  if (sellFlag != 1 && !unmoneyVisible) {
    buyFlag = 1;
    std::string txt = deal ? std::string(deal->name) + "?" : "";
    SetConfirm("confirm to buy:", deal ? txt.c_str() : nullptr);
    confirmChk = chk;
  }
}
// Clicking an equipment icon on the status panel (DefineButton2_626-677).
void OfferSale(int chk) {
  if (buyFlag == 1 || confirmVisible || chk < 1 || chk > 18 || !Owned(chk))
    return;
  Sound("clk");
  sellFlag = 1;
  SetConfirm("Confirm to sell(50%):",
             chk == 7 ? "Thick Steel Round Shield" : items[chk - 1].name);
  confirmChk = chk;
}
// The confirm dialog's Yes (DefineButton2_119).
void ConfirmDeal() {
  if (!confirmVisible)
    return;
  int chk = confirmChk;
  if (sellFlag == 1 && chk >= 1 && chk <= 18 && Owned(chk))
    SellItem(chk);
  if (buyFlag == 1) {
    auto deal = DealFor(chk);
    if (!deal || cash < deal->price) {
      CancelDeal();
      Unmoney();
      return;
    }
    bool ok = true;
    for (int s : slots)
      if (s && (s + 2) / 3 == (chk + 2) / 3) {
        Same("Same Type Item !!!", "You must sell the existing item");
        ok = false;
        break;
      }
    if ((chk >= 29 && chk <= 31 && chk - 27 <= gun + 1) ||
        (chk >= 32 && chk <= 34 && specials[chk - 32]) ||
        (chk >= 25 && chk <= 28 && selectedTurret >= 0 &&
         chk - 24 <= turretLevel[selectedTurret - 3])) {
      Same("Already Upgraded!", "");
      ok = false;
    }
    if (ok) {
      Sound("clink");
      if (auto deal = DealFor(chk))
        cash -= deal->price;
      if (chk >= 1 && chk <= 18) {
        for (auto &s : slots)
          if (!s) {
            s = chk;
            break;
          }
        const auto &item = items[chk - 1];
        ApplyItem(item, 1);
        if (item.shock > 0)
          heroShock = item.shock;
      } else if (chk >= 25 && chk <= 28 && selectedTurret >= 0) {
        Sound("stup");
        turretLevel[selectedTurret - 3] = chk - 24;
        buildings[selectedTurret].building = true;
        buildings[selectedTurret].progress = 0;
      } else if (chk >= 29 && chk <= 31)
        gun = chk - 28;
      else if (chk >= 32 && chk <= 34)
        specials[chk - 32] = true;
    }
  }
  CancelDeal();
  ApplyHeroLook();
}
void DrawUnit(const Actor &a) {
  if (a.removed)
    return;
  bool flyer = !a.friendly && a.kind == 2;
  if (!a.clip) {
    RegisteredSprite(a.friendly ? "hero"
                                : (a.kind == 0 ? "enemy" : (a.kind == 1 ? "shooter" : "fly")),
                     a.pos, 1, glm::radians(a.rotation + 180));
    return;
  }
  // Units turn their inner clip; flyers have none and turn as a whole.
  swf::Affine base = WorldBase() * swf::Affine::translate(a.pos);
  if (flyer)
    base = base * swf::Affine::rotate(glm::radians(a.rotation));
  a.clip->draw(base);
}
std::string FlashNumber(float value) {
  std::ostringstream text;
  text << value;
  return text.str();
}
// The status clip (at 2, 110) with its fields, HP figure and the equipment
// icons the shop attaches to it.
void DrawStatus() {
  if (!statusClip)
    return;
  auto &fields = statusClip->variables;
  fields["rn"] = "4";
  fields["att"] = FlashNumber(HeroAttack());
  fields["deff"] = FlashNumber(player.armor);
  fields["str"] = FlashNumber(player.maxHp);
  fields["energy"] = FlashNumber(maxEnergy);
  fields["gen"] = FlashNumber(regen);
  fields["reduce"] =
      "-" + std::to_string(100 - int(std::ceil(1000 / (10 + player.armor)))) + "%";
  if (auto *figure = statusClip->child("HPvis")) {
    int frame = player.removed
                    ? 9
                    : std::clamp(10 - int(std::floor(player.hp / player.maxHp * 10)),
                                 0, figure->frameCount() - 1);
    if (figure->frame != frame)
      figure->gotoAndStop(frame);
  }
  auto base = StageBase() * swf::Affine::translate({2, 110});
  statusClip->draw(base);
  for (int k = 0; k < 6; ++k) {
    if (!slots[k]) {
      slotIcons[k].reset();
      continue;
    }
    std::string name = std::to_string(slots[k]);
    if (!slotIcons[k] || slotIcons[k]->id != swf::library.exportId(name))
      slotIcons[k] = swf::Clip::fromExport(name);
    int empty = k + 1;
    if (slotIcons[k])
      slotIcons[k]->draw(base * swf::Affine::translate(
                                    {78.f - 48 * (empty % 2),
                                     48 * std::ceil(empty / 2.f) - 15}));
  }
}
// The quality menu's low/medium/high buttons; only high smooths bitmaps.
// The exports are 2x or more, so smoothing needs their mip levels.
void SetQuality(bool high) {
  highQuality = high;
  for (auto &entry : art)
    entry.second->updateSamplerDetails(highQuality ? GL_FilterModes::MIPMAP
                                                   : GL_FilterModes::NEAREST,
                                       entry.second->wrapMode);
}

} // namespace

void DrawGame();
// Clips rasterize and upload their art the first time they are drawn, which
// stalled the first landing frame for about two seconds (and, with the frame
// time clamp, cost the cinematic that time). Flash has it all decoded before
// the menus, so draw the intro, landing and arrival once at start-up, unseen.
void WarmUp() {
  auto draw = [](Screen shown) {
    screen = shown;
    BeginDrawing();
    DrawGame();
    ClearBackground(BLACK); // Never presented
  };
  for (Screen menu : {Screen::Splash, Screen::Title, Screen::Instructions,
                      Screen::Difficulty})
    draw(menu);
  Reset();
  cinematicAge = 0;
  introShip = swf::Clip::fromExport("shipz");
  draw(Screen::Intro);
  dropshipClip = swf::Clip::fromExport("shipup");
  draw(Screen::Landing);
  helpVisible = hintsVisible = statusVisible = qualVisible = true;
  departing = dropshipClip != nullptr;
  draw(Screen::Play);
  draw(Screen::Pause);
  draw(Screen::Shop);
  introShip.reset();
  dropshipClip.reset();
  departing = false;
}
void InitGame() {
  extras::drawYDown = true;
  // The template's decorative font lacks digits and punctuation used in the
  // HUD.
  const char *fontPath = "C:/Windows/Fonts/segoeui.ttf";
  if (std::filesystem::exists(fontPath)) {
    uiFont = std::make_unique<Font>();
    if (uiFont->loadFromFile(fontPath)) {
      engineFont = extras::defaultFont;
      extras::defaultFont = uiFont.get();
      extras::defaultTextRenderer->setFont(*uiFont);
    }
  }
  // The HUD's dynamic fields name the device font "Sydnie", whose embedded copy
  // has no digits; Flash falls back to Arial with GDI-style hinting.
  const char *devicePath = "C:/Windows/Fonts/arial.ttf";
  if (std::filesystem::exists(devicePath)) {
    deviceFont = std::make_unique<Font>();
    if (deviceFont->loadFromFile(devicePath)) {
      deviceFont->setHinting(FontHinting::FULL);
      deviceFont->setCoverageGamma(1.6f);
    } else
      deviceFont.reset();
  }
  const std::pair<const char *, const char *> assets[] = {
      {"field", "DefineSprite_108_field"},
      {"hero", "DefineSprite_372_hero"},
      {"enemy", "DefineSprite_383_enemy"},
      {"fly", "DefineSprite_393_fly"},
      {"shooter", "DefineSprite_442_shooter"},
      {"turret", "DefineSprite_275_turret"},
      {"planturret", "DefineSprite_410_planturret"},
      {"mainest", "DefineSprite_420_mainest"},
      {"nest", "DefineSprite_428_nest"},
      {"CC", "DefineSprite_707_CC"},
      {"barack", "DefineSprite_718_barack"},
      {"herohp", "DefineSprite_601_herohp"},
      {"heromana", "DefineSprite_603_heromana"},
      {"turretBar", "DefineSprite_235"},
      {"dot", "DefineSprite_513_dot"},
      {"dotb", "DefineSprite_515_dotb"},
      {"dotr", "DefineSprite_517_dotr"},
      {"dote", "DefineSprite_688_dote"},
      {"guard", "DefineSprite_752_guard"},
      {"dropship", "DefineSprite_754_shipup"},
      {"ship", "DefineSprite_112_shipz"},
      {"logo", "DefineSprite_765_logoin"},
      {"skipplay", "DefineSprite_762_skiper"}};
  for (const auto &a : assets)
    LoadArt(a.first, a.second);
  for (int i = 1; i <= 30; ++i)
    LoadArt("exp" + std::to_string(i), "DefineSprite_472_exp", i);
  for (int i = 1; i <= 6; ++i)
    LoadArt("help" + std::to_string(i), "DefineSprite_229_help", i);
  LoadArt("turretBase", "DefineSprite_231");
  LoadArt("titleBack", "DefineSprite_768");
  for (int i = 1; i <= 4; ++i)
    LoadArt("weapview" + std::to_string(i), "DefineSprite_585", i);
  for (int i = 1; i <= 9; ++i)
    LoadArt("slot" + std::to_string(i), "DefineSprite_597", i);
  // HUD bar without placeholder text, meters or slots (tools/export_hud.py).
  if (std::filesystem::exists("resources/game/hud/bar.png"))
    art["hud"] =
        std::make_unique<Texture>("resources/game/hud/bar.png", true, false,
                                  true);
  else
    missing += "resources/game/hud/bar.png\n";
  LoadArt("jets", "DefineSprite_111");
  for (int i = 1; i <= 4; ++i)
    LoadArt("cannon" + std::to_string(i), "DefineSprite_259", i);
  for (int i = 1; i <= 31; ++i)
    LoadArt("briefing" + std::to_string(i), "DefineSprite_834", i);
  for (const auto &item : std::vector<std::pair<const char *, int>>{
           {"turretRing", 232},
           {"turretCross", 236},
           {"dropshipBody", 753},
           {"turretBarFrame", 233},
           {"radarFrame", 503}}) {
    auto path =
        "resources/game/shapes/" + std::to_string(item.second) + ".svg";
    if (std::filesystem::exists(path))
      art[item.first] = std::make_unique<Texture>(path, true, true, true);
    else
      missing += path + "\n";
  }
  for (const auto &scene :
       std::vector<std::pair<const char *, int>>{{"title", 3},
                                                 {"instructions", 4},
                                                 {"difficulty", 5},
                                                 {"planet", 6},
                                                 {"victory", 8},
                                                 {"defeat", 9},
                                                 {"back", 4}})
    LoadScene(scene.first, scene.second);
  if (!swf::library.load("resources/game/swf"))
    missing += "resources/game/swf/timelines.json (tools/export_timelines.py)\n";
  else {
    // Edit-text fields (money values and the like) use the device font.
    swf::drawEditText = [](const swf::EditText &edit, const std::string &text,
                           const swf::Affine &world, const glm::vec4 &bounds,
                           const swf::ColorTransform &tint) {
      Font *font = deviceFont ? deviceFont.get() : uiFont.get();
      if (text.empty() || !font)
        return;
      float scale = std::sqrt(world.a * world.a + world.b * world.b);
      auto align = edit.align == 1   ? TextAlignmentX::RIGHT
                   : edit.align == 2 ? TextAlignmentX::CENTER
                                     : TextAlignmentX::LEFT;
      float x = align == TextAlignmentX::RIGHT    ? bounds.y - 2
                : align == TextAlignmentX::CENTER ? (bounds.x + bounds.y) * .5f
                                                  : bounds.x + 2;
      // Sizes as calibrated for the HUD's device text (9px for a 10px field).
      DrawText(text, world.apply({x, bounds.z + 2}), edit.size * .9f * scale,
               edit.color * tint.mul, *font, align, TextAlignmentY::TOP);
    };
    // Buttons follow the pointer like Flash's; entering one plays "button".
    // Presses are noted for the next update (help pages, notices, quality).
    swf::resolveButton = [](const swf::Clip &button, glm::vec4 area) {
      if (((unmoneyVisible || sameVisible) && button.id != 615) ||
          (confirmVisible && !unmoneyVisible && !sameVisible &&
           button.id != 119 && button.id != 122))
        return swf::ButtonState::Up;
      // The title's Play More Games button never shows its over state in
      // Flash (its link still works; see UpdateGame).
      if (button.id == 779)
        return swf::ButtonState::Up;
      glm::vec2 m = MousePixels();
      bool inside = m.x >= area.x && m.x <= area.y && m.y >= area.z && m.y <= area.w;
      auto state = !inside      ? swf::ButtonState::Up
                   : pointerDown ? swf::ButtonState::Down
                                 : swf::ButtonState::Over;
      if (inside && pointerPressed)
        armedButton = &button;
      if (inside && pointerReleased && armedButton == &button) {
        pressedButtons.clear();
        pressedButtons[button.id] = UiMouse();
        armedButton = nullptr;
      }
      if (state != swf::ButtonState::Up && button.buttonState == swf::ButtonState::Up) {
        Sound("button");
        pendingRollOvers[button.id] = UiMouse();
      }
      if (state == swf::ButtonState::Up && button.buttonState != swf::ButtonState::Up)
        pendingRollOuts.insert(button.id);
      return state;
    };
    // The splash tagline (shape 871) is revealed by a white bar (872) that
    // shrinks horizontally; drawn from the 2x exports, the squashed bar's
    // edges blur and the hidden letters show through.
    // The title's vector art is drawn at screen scale too, as the 2x
    // exports, halved and resampled again inside scaled sprites, are softer
    // than Flash: backdrop 767, marine 769, turrets 771/772, muzzle flashes
    // 774, shield bar 782 and buttons 755/756/758, and the instructions and
    // difficulty backgrounds (791, 809). The SUPER MARINE (781) and MaxGames
    // (789) logos are unsmoothed bitmap fills, drawn pixel for pixel.
    swf::library.vectorShapes = {871, 872, 755, 756, 758, 767, 769, 771,
                                 772, 774, 781, 782, 789, 791, 809};
    splashClip = std::make_unique<swf::Clip>(0);
    splashClip->gotoAndPlay(9);
    screen = Screen::Splash;
  }
  if (!missing.empty()) {
    screen = Screen::Missing;
    std::cerr << missing;
    return;
  }
  // Exported timelines have transparent margins; crop UVs without changing
  // source artwork.
  const std::pair<const char *, glm::vec4> bounds[] = {
      {"hero", {11, 104, 64, 77}},   {"enemy", {12, 9, 67, 67}},
      {"fly", {0, 0, 255, 128}},     {"shooter", {1, 1, 108, 102}},
      {"turret", {68, 1, 140, 140}}, {"back", {850, 716, 236, 64}}};
  for (const auto &b : bounds) {
    auto r = b.second;
    r.y = float(art[b.first]->height) - r.y - r.w;
    crops[b.first] = r;
  }
  for (const auto &[name, id] : soundIds) {
    auto p = "resources/game/sounds/" + std::to_string(id) + "_" +
             std::to_string(id) + ".mp3";
    if (std::filesystem::exists(p))
      sounds[name] = std::make_unique<SoundEffect>(p, 4); // OpenAL has ~256 sources
  }
  const char *p = "resources/game/sounds/4_4.mp3";
  if (std::filesystem::exists(p)) {
    wind = std::make_unique<MusicSound>(p, true);
    wind->setVolume(.25f);
    wind->play();
  }
  WarmUp();
  Reset();
  // One "score name" entry per line; older files hold scores alone.
  std::ifstream scoreFile("super-marine-scores.txt");
  for (std::string line; highScores.size() < 10 && std::getline(scoreFile, line);) {
    HighScore entry;
    size_t end = 0;
    try {
      entry.score = std::stoi(line, &end);
    } catch (const std::exception &) {
      continue;
    }
    size_t start = line.find_first_not_of(" \t\r", end);
    size_t last = line.find_last_not_of(" \t\r");
    entry.name = start == std::string::npos ? "NONAME"
                                            : line.substr(start, last - start + 1);
    highScores.push_back(entry);
  }
  screen = splashClip ? Screen::Splash : Screen::Title;
}
// yourname: input field 857 on frame 8, at (311.95, 278) with bounds -2..143
// by -2..15.35, 12 px white device text, centred. The engine's text box edits
// it; it is laid out in window pixels, so it follows the stage every frame.
const Rect nameField(309.95f, 276, 145, 17.35f);
void PlaceNameBox() {
  if (!nameBox) {
    nameBox = std::make_unique<TextInputBox>();
    nameBox->renderText.setFont(deviceFont ? *deviceFont : *extras::defaultFont);
    nameBox->color = glm::vec4(0); // The scene draws the field's backdrop
    nameBox->textColor = nameBox->cursorColor = WHITE;
    // Flash highlights a selection in black, keeping the text white.
    nameBox->selectionColor = BLACK;
    nameBox->invertSelection = true;
    nameBox->selectedTextColor = WHITE;
    nameBox->offsetFromLeft = 0;
    nameBox->text = yourname;
  }
  float scale = UiScale();
  nameBox->rect = Rect(UiPoint(nameField.position), nameField.size * scale);
  // drawEditText's calibration: device text at 90% of the field's size.
  nameBox->renderText.setCharacterSize(12 * .9f * scale);
  float width = nameBox->renderText.GetCursorX(nameBox->text,
                                               int(nameBox->text.size()));
  nameBox->textOffsetX = (nameBox->rect.size.x - width) * .5f;
  // The box centres a line at its top plus one line height (the default
  // font's "Ag" plus 5 px); drawEditText puts the text's top 2 px into the
  // field. Shift the box's lines (text, caret, selection) to match.
  auto line = MeasureText("Ag", nameBox->renderText.characterSize);
  float lineHeight = line.maxAscender - line.maxDescender + 5;
  Text sample(*nameBox->renderText.font, "Ag", nameBox->renderText.characterSize);
  float centre = UiPoint({0, 278}).y + sample.MeasureText().totalHeight * .5f;
  nameBox->textOffsetY = centre - (nameBox->rect.position.y + lineHeight);
}
// Flash input-field editing through the engine's text box. The name stays on
// one line within the field.
void EditName() {
  PlaceNameBox();
  std::string before = nameBox->text;
  int caret = nameBox->cursorPosition;
  nameBox->Update(MousePixels());
  float width = nameBox->renderText.GetCursorX(nameBox->text,
                                               int(nameBox->text.size()));
  if (width > nameBox->rect.size.x - 4 * UiScale()) {
    nameBox->text = before; // Full
    nameBox->cursorPosition = std::min(caret, int(before.size()));
    nameBox->clearSelection();
  }
  yourname = nameBox->text;
}
void UpdateGame(float dt) {
  SamplePointer();
  if (pointerPressed)
    pressPoint = UiMouse();
  clickedButtons.swap(pressedButtons);
  pressedButtons.clear();
  rollOvers.swap(pendingRollOvers);
  pendingRollOvers.clear();
  rollOuts.swap(pendingRollOuts);
  pendingRollOuts.clear();
  // The first frame's time is start-up, not movie time: Flash starts the
  // splash on its first frame once loaded.
  static bool started = false;
  if (!std::exchange(started, true))
    dt = 0;
  dt = std::clamp(dt, 0.f, .25f);
  if (IsKeyPressed(GLFW_KEY_M)) {
    muted = !muted;
    if (wind) {
      if (muted)
        wind->pause();
      else
        wind->play();
    }
  }
  if (screen == Screen::Missing)
    return;
  auto pressed = [](int key) { return IsKeyPressed(key); };
  auto clicked = [](int button) { return clickedButtons.count(button) > 0; };
  bool gameplay = screen == Screen::Play || screen == Screen::Pause ||
                  screen == Screen::Shop;
  // Menu and overlay clips run at the movie's frame rate.
  menuClock += dt;
  while (menuClock >= Tick) {
    menuClock -= Tick;
    if (splashClip)
      splashClip->tick();
    for (auto &entry : sceneClips)
      entry.second->tick();
    if (gameplay) {
      Checker();
      // Flash pauses physics handlers, not independent effect timelines.
      // Their scripted fades remain in Step(), where the source checks p.
      if (screen == Screen::Pause || screen == Screen::Shop) {
        for (auto &effect : effects)
          effect.clip->tick();
        std::erase_if(effects, [](const Effect &effect) {
          return effect.clip->removed;
        });
      }
      for (auto *clip : {helpClip.get(), statusClip.get(), circleClip.get(),
                         screenClip.get(), qualClip.get(), warnClip.get(),
                         sameClip.get(), unmoneyClip.get()})
        if (clip)
          clip->tick();
    }
    if (screen == Screen::Landing) {
      if (skipClip)
        skipClip->tick();
      if (dropshipClip) {
        PoseDropship(LandingAt(cinematicAge).ship.x > 900 ? 0 : 1);
        dropshipClip->tick();
      }
    }
    if (screen == Screen::Intro) {
      if (!introShip)
        introShip = swf::Clip::fromExport("shipz");
      if (introShip)
        introShip->tick();
    }
  }
  if (screen == Screen::Splash) {
    // Frame 209 hands over to the title (maxgam = 2, back through frame 2).
    if (!splashClip || splashClip->frame >= 208 || pressed(GLFW_KEY_ENTER) ||
        pressed(GLFW_KEY_ESCAPE) || pressed(GLFW_KEY_SPACE)) {
      splashClip.reset();
      screen = Screen::Title;
    }
    return;
  }
  if (screen == Screen::Title) {
    if (Click(Rect(398, 123, 117, 32)) || pressed(GLFW_KEY_ENTER)) {
      screen = Screen::Difficulty;
      Sound("clk");
    } else if (Click(Rect(398, 165, 117, 32))) {
      screen = Screen::Instructions;
      Sound("clk");
    } else if (Click(Rect(398, 248, 117, 32))) {
      screen = Screen::Scores;
      Sound("clk");
    } else if (Click(Rect(398, 205, 117, 32)) || Click(Rect(382, 15, 160, 45)))
      ShellExecuteW(nullptr, L"open", L"https://www.maxgames.com/", nullptr,
                    nullptr, SW_SHOWNORMAL);
    return;
  }
  if (screen == Screen::Instructions || screen == Screen::Scores) {
    if (pressed(GLFW_KEY_ESCAPE) || Click(Rect(425, 358, 118, 32))) {
      screen = Screen::Title;
      Sound("clk");
    }
    return;
  }
  if (screen == Screen::Difficulty) {
    if (pressed(GLFW_KEY_ESCAPE)) {
      screen = Screen::Title;
      return;
    }
    for (int i = 0; i < 3; ++i)
      if (Click(Rect(234, 137.5f + 40 * i, 117, 32)) ||
          pressed(GLFW_KEY_1 + i) || (i == 1 && pressed(GLFW_KEY_ENTER))) {
        difficulty = i == 0 ? .5f : (i == 1 ? .8f : 1.2f);
        cinematicAge = 0;
        introTick = 0;
        if (auto *warning = Warning())
          warning->gotoAndStop(0);
        screen = Screen::Intro;
        Sound("clk");
        break;
      }
    return;
  }
  if (screen == Screen::Intro) {
    cinematicAge += dt;
    // shippos: the transmission opens at z 40 and closes at z 4000, and the
    // ships' engines sound at z 40 and z 100.
    while (introTick < int(cinematicAge * 24)) {
      ++introTick;
      auto *warning = Warning();
      if (introTick == 2 || introTick == 5)
        Sound("shipp");
      if (warning && introTick == 2)
        warning->gotoAndPlay(1);
      if (warning && introTick == 200)
        warning->gotoAndPlay(20);
    }
    if (cinematicAge >= 400.f / 24 || Click(Rect(425, 358, 118, 32)) ||
        pressed(GLFW_KEY_SPACE) || pressed(GLFW_KEY_ENTER) ||
        pressed(GLFW_KEY_ESCAPE))
      BeginLanding();
    return;
  }
  if (screen == Screen::Landing) {
    cinematicAge += dt;
    if (LandingAt(cinematicAge).landed)
      BeginMission(false);
    else if (Click(Rect(410, 320, 117, 32)) || pressed(GLFW_KEY_SPACE) ||
             pressed(GLFW_KEY_ENTER) || pressed(GLFW_KEY_ESCAPE)) {
      spaceBlocked = IsKeyDown(GLFW_KEY_SPACE);
      BeginMission(true);
    }
    return;
  }
  // Frames 8 and 9: Submit (846), Hall of Fame (848), Main Menu (839) and
  // the MaxGames logo (859). Enter and Esc remain shortcuts while the name
  // field does not have the keyboard.
  if (screen == Screen::Won || screen == Screen::Lost) {
    // Enter in the box ends editing; it does not also start a mission.
    bool typing = nameBox && nameBox->hasFocus;
    if (screen == Screen::Won) {
      EditName();
      typing = typing || nameBox->hasFocus;
    }
    if (clicked(846)) {
      SubmitScore();
      screen = Screen::Scores;
    } else if (clicked(848))
      screen = Screen::Scores;
    else if (clicked(839) || (!typing && pressed(GLFW_KEY_ESCAPE))) {
      Sound("clk");
      screen = Screen::Title;
    } else if (!typing && pressed(GLFW_KEY_ENTER))
      screen = Screen::Difficulty;
    else if (clicked(859))
      ShellExecuteW(nullptr, L"open", L"https://www.maxgames.com/", nullptr,
                    nullptr, SW_SHOWNORMAL);
    if (screen != Screen::Won && screen != Screen::Lost && nameBox)
      nameBox->hasFocus = false;
    return;
  }
  // Gameplay: Play, Pause (P) and the shop.
  if (!IsKeyDown(GLFW_KEY_SPACE))
    spaceBlocked = false;
  if (unmoneyVisible || sameVisible) {
    if (clicked(615))
      unmoneyVisible = sameVisible = false;
    return;
  }
  if (confirmVisible) {
    if (clicked(122)) {
      Sound("clk");
      CancelDeal();
    } else if (clicked(119))
      ConfirmDeal();
    return;
  }
  if (helpVisible && helpClip) {
    if (clicked(130))
      helpClip->gotoAndStop(helpClip->frame + 1);
    if (clicked(148))
      helpClip->gotoAndStop(helpClip->frame - 1);
  }
  if (qualVisible)
    for (int button : {722, 723, 724})
      if (clicked(button)) {
        Sound("clk");
        qualVisible = false;
        SetQuality(button == 724);
      }
  for (int chk = 1; chk <= 18; ++chk)
    if (clicked(623 + 3 * chk)) // Status panel icons
      OfferSale(chk);
  if (screen == Screen::Shop) {
    const glm::vec2 equipAt(385, 50);
    if (equipClip) {
      // rollOver names the item under the pointer; rollOut clears it.
      for (int id : rollOuts)
        if (id == 547 || id == 562 || id == 570)
          equipClip->variables["nam"] = equipClip->variables["price"] = "";
      for (auto &[id, at] : rollOvers)
        if (id == 547 || id == 562 || id == 570)
          if (auto deal = DealFor(GridItem(id, at - equipAt))) {
            equipClip->variables["nam"] = deal->name;
            equipClip->variables["price"] = deal->info;
          }
      // One button spans the grid. Refresh when moving between its cells.
      if (auto *grid = equipClip->childAt(2);
          grid && grid->buttonState != swf::ButtonState::Up)
        if (auto deal = DealFor(GridItem(grid->id, UiMouse() - equipAt))) {
          equipClip->variables["nam"] = deal->name;
          equipClip->variables["price"] = deal->info;
        }
      if (clicked(558)) { // Weapon tab
        Sound("clk");
        equipClip->gotoAndStop(2);
      }
      if (clicked(566)) // Equipment tab
        equipClip->gotoAndStop(0);
    }
    for (int grid : {547, 562, 570})
      if (auto it = clickedButtons.find(grid); it != clickedButtons.end()) {
        Sound(grid == 547 ? "pick" : "clk");
        OfferDeal(GridItem(grid, it->second - equipAt));
      }
    if (clicked(551) || pressed(GLFW_KEY_P))
      CloseShop();
    return;
  }
  auto toggle = [&](int key, bool &visible) {
    if (pressed(key)) {
      Sound("clk");
      visible = !visible;
    }
  };
  if (pressed(GLFW_KEY_H) || Click(Rect(500, 0, 50, 17))) {
    Sound("clk");
    if (helpClip)
      helpClip->gotoAndStop(0);
    helpVisible = !helpVisible;
  }
  toggle(GLFW_KEY_R, radarVisible);
  toggle(GLFW_KEY_I, hintsVisible);
  toggle(GLFW_KEY_F, statusVisible);
  toggle(GLFW_KEY_Q, qualVisible);
  if (pressed(GLFW_KEY_P)) {
    if (screen == Screen::Pause)
      statusVisible = false;
    screen = screen == Screen::Pause ? Screen::Play : Screen::Pause;
    Sound("clk");
    firing = false;
    accumulator = 0;
    SnapshotMotion();
  }
  if (pressed(GLFW_KEY_E)) {
    Sound("stup");
    for (int i = 0; i < 2; ++i)
      if (Touching(i == 0 ? "tur1" : "tur2"))
        repairing[i] = !repairing[i];
  }
  for (int i = 0; i < 4; ++i)
    if (pressed(GLFW_KEY_1 + i)) {
      if (i == 0 || specials[i - 1]) {
        const int ranges[] = {500, 200, 350, 700};
        Sound("ch");
        weapon = i;
        weaponRange = ranges[i];
        ApplyHeroLook();
      } else
        Sound("err");
    }
  if (screen != Screen::Play)
    return;
  firing = pointerDown;
  accumulator += dt;
  while (accumulator >= Tick && screen == Screen::Play) {
    Step();
    accumulator -= Tick;
  }
  Camera();
}
// While a frame is drawn, puts every moving thing `t` of the way from its
// pose at the start of the step to its current one, and restores the
// simulation's values afterwards. Steps stay at the movie's 24 Hz; only the
// drawing in between is smoothed. Timeline animation is left frame-exact.
class WorldBlend {
public:
  explicit WorldBlend(float t) {
    if (t >= 1)
      return;
    viewBlend = t;
    auto actor = [&](Actor &a) {
      if (a.removed)
        return;
      Set(a.pos, Mix(a.lastPos, a.pos, t));
      Set(a.cannon, MixAngle(a.lastCannon, a.cannon, t));
      float rotation = MixAngle(a.lastRotation, a.rotation, t);
      if (rotation == a.rotation)
        return;
      Set(a.rotation, rotation);
      if (a.clip)
        if (auto *body = a.clip->child("clip")) {
          clips.push_back({body, body->matrix, body->matrixLocked});
          body->setRotation(rotation);
        }
    };
    actor(player);
    for (auto &a : units)
      actor(a);
    for (auto &b : buildings)
      actor(b);
    for (auto &b : bullets)
      Set(b.pos, Mix(b.lastPos, b.pos, t));
    for (auto &e : effects) {
      Set(e.pos, Mix(e.lastPos, e.pos, t));
      Set(e.rotation, MixAngle(e.lastRotation, e.rotation, t));
      Set(e.scale, MixValue(e.lastScale, e.scale, t));
      Set(e.alpha, MixValue(e.lastAlpha, e.alpha, t));
    }
    Set(dropshipPos, Mix(lastDropshipPos, dropshipPos, t));
    Set(dropshipScale, MixValue(lastDropshipScale, dropshipScale, t));
    // heroz turns the hero to the pointer every frame; follow it live.
    if (!player.removed && player.clip)
      if (auto *body = player.clip->child("clip")) {
        glm::vec2 view = -UiPoint(FieldOrigin()) / WorldZoom();
        float facing = Normalize(
            AimAt(player.pos, MousePixels() / WorldZoom() + view));
        Set(player.rotation, facing);
        clips.push_back({body, body->matrix, body->matrixLocked});
        body->setRotation(facing);
      }
  }
  ~WorldBlend() {
    viewBlend = 1;
    for (auto it = clips.rbegin(); it != clips.rend(); ++it) {
      it->clip->matrix = it->matrix;
      it->clip->matrixLocked = it->locked;
    }
    for (auto it = floats.rbegin(); it != floats.rend(); ++it)
      *it->first = it->second;
    for (auto it = points.rbegin(); it != points.rend(); ++it)
      *it->first = it->second;
  }
  WorldBlend(const WorldBlend &) = delete;
  WorldBlend &operator=(const WorldBlend &) = delete;

private:
  static float MixValue(float from, float to, float t) {
    return std::isnan(from) ? to : from + (to - from) * t;
  }
  static float MixAngle(float from, float to, float t) {
    return std::isnan(from) ? to : to - std::remainder(to - from, 360.f) * (1 - t);
  }
  // Fresh objects and jumps longer than any step's travel are not blended.
  static glm::vec2 Mix(glm::vec2 from, glm::vec2 to, float t) {
    return std::isnan(from.x) || Length(to - from) > 120 ? to
                                                         : glm::mix(from, to, t);
  }
  void Set(float &field, float value) {
    floats.push_back({&field, field});
    field = value;
  }
  void Set(glm::vec2 &field, glm::vec2 value) {
    points.push_back({&field, field});
    field = value;
  }
  struct SavedClip {
    swf::Clip *clip;
    swf::Affine matrix;
    bool locked;
  };
  std::vector<std::pair<float *, float>> floats;
  std::vector<std::pair<glm::vec2 *, glm::vec2>> points;
  std::vector<SavedClip> clips;
};
void DrawGame() {
  ClearBackground(BLACK);
  swf::filterScale = UiScale();
  auto w = GetCurrentWindowSize();
  auto origin = UiOrigin();
  float scale = UiScale();
  int fw = 0, fh = 0;
  glfwGetFramebufferSize(extras::ActiveWindow, &fw, &fh);
  float px = fw / w.x, py = fh / w.y;
  // Mouse.hide() while playing; the circle clip is the cursor. The pointer
  // returns in the shop and once the mission is decided.
  bool hideCursor = (screen == Screen::Play || screen == Screen::Pause ||
                     screen == Screen::Landing) &&
                    winCounter == 0 && loseCounter == 0 && !testPointer;
  // BeginDrawing restores the cursor mode every frame; a cached request
  // becomes stale. Reapply against the actual GLFW mode after BeginDrawing.
  int cursorMode = hideCursor ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL;
  if (glfwGetInputMode(extras::ActiveWindow, GLFW_CURSOR) != cursorMode) {
    glfwSetInputMode(extras::ActiveWindow, GLFW_CURSOR,
                     cursorMode);
  }
  glEnable(GL_SCISSOR_TEST);
  glScissor(int(origin.x * px), int((w.y - origin.y - 400 * scale) * py),
            int(550 * scale * px), int(400 * scale * py));
  struct ClipGuard {
    ~ClipGuard() { glDisable(GL_SCISSOR_TEST); }
  } clip;
  if (screen == Screen::Missing) {
    UiText("Game artwork could not be found", 20, 50, 18, RED);
    UiText("Required: resources/game. See PORTING.md.", 20, 80);
    return;
  }
  if (screen == Screen::Splash) {
    if (splashClip)
      splashClip->draw(StageBase());
    return;
  }
  if (screen == Screen::Title) {
    // Flash draws off-stage artwork: the title backdrop (sprite 768, placed at
    // -2,-5) shows 5px of sky above the stage when the window has room.
    glDisable(GL_SCISSOR_TEST);
    UiArt("titleBack", Rect(-2, -5, 554, 405.95f));
    glEnable(GL_SCISSOR_TEST);
    // The scene is drawn live from the SWF, so its vectors are as sharp as
    // Flash's and the button labels do not shift on hover.
    if (auto *scene = Scene(2)) {
      if (scene->depthOffsets.empty()) {
        // Label offsets measured against the running Flash title screen.
        scene->depthOffsets = {{41, {1, -1}}, // PLAY GAME
                               {42, {1, -1}}, // INSTRUCTIONS
                               {46, {1, -1}}, // Play More Games
                               {40, {1, -2}}}; // Hall of Fame
        if (auto *more = scene->childAt(43)) // Top "Play More Games" link
          more->depthOffsets = {{1, {1, -1}}};
      }
      scene->draw(StageBase());
    } else
      UiArt("title", Rect(0, 0, 550, 400));
    return;
  }
  if (screen == Screen::Instructions) {
    if (!DrawScene(3)) {
      UiArt("instructions", Rect(0, 0, 550, 400));
      SceneButtons(3);
    }
    return;
  }
  if (screen == Screen::Difficulty) {
    if (!DrawScene(4)) {
      UiArt("difficulty", Rect(0, 0, 550, 400));
      SceneButtons(4);
    }
    return;
  }
  if (screen == Screen::Scores) {
    UiArt("title", Rect(0, 0, 550, 400));
    UiRect(Rect(80, 35, 390, 305), glm::vec4(0, 0, 0, .94f));
    UiText("Hall of Fame", 275, 70, 24, YELLOW, TextAlignmentX::CENTER);
    UiText("Local mission scores", 275, 95, 10, WHITE, TextAlignmentX::CENTER);
    if (highScores.empty())
      UiText("Complete a mission to record your score.", 275, 155, 12, WHITE,
             TextAlignmentX::CENTER);
    for (size_t i = 0; i < highScores.size(); ++i) {
      float y = 125 + 18.f * i;
      UiText(std::to_string(i + 1) + ".    " + highScores[i].name, 140, y, 12);
      UiText(std::to_string(highScores[i].score), 410, y, 12, WHITE,
             TextAlignmentX::RIGHT);
    }
    // Reuse the original instructions screen's Back button as a texture crop.
    UiArt("back", Rect(425, 358, 118, 32));
    SceneButtons(3); // The instructions screen's Back button
    return;
  }
  if (screen == Screen::Intro) {
    UiArt("planet", Rect(0, 0, 550, 400));
    int tick = int(cinematicAge * 24);
    // Drawn between frames (steps), so the ships glide rather than step.
    float steps = cinematicAge * 24;
    float ratio = 50.f / (50.f + 20 * steps);
    // shippos: two ships at (tick, 200) and (tick + 100, 300), shrinking as
    // they fly off.
    for (glm::vec2 at : {glm::vec2(steps, 200), glm::vec2(100 + steps, 300)}) {
      if (introShip)
        introShip->draw(StageBase() * swf::Affine::translate(at) *
                        swf::Affine::scale(glm::vec2(ratio)));
      else
        UiArt("ship", Rect(at.x + 5.5f * ratio, at.y - 96.6f * ratio,
                           290.5f * ratio, 228 * ratio));
    }
    // The transmission clip plays its own timeline, including its flashing.
    if (auto *warning = Warning())
      warning->draw(StageBase() * warning->matrix);
    else {
      int briefingFrame = 1;
      if (tick >= 2 && tick < 200)
        briefingFrame = std::min(20, tick);
      else if (tick >= 200)
        briefingFrame = std::min(31, 21 + tick - 200);
      UiArt("briefing" + std::to_string(briefingFrame),
            Rect(368.9f, 249.95f, 134, 89));
    }
    SceneButtons(5);
    return;
  }
  if (screen == Screen::Won || screen == Screen::Lost) {
    // Frames 8 and 9 drawn live, so their buttons, animation and fields
    // (point1-3, total and the yourname input) behave as in Flash.
    bool won = screen == Screen::Won;
    if (auto *scene = Scene(won ? 7 : 8); scene && won) {
      scene->variables["point1"] = std::to_string(point1);
      scene->variables["point2"] = std::to_string(point2.value_or(0));
      scene->variables["point3"] = std::to_string(point3);
      scene->variables["total"] = std::to_string(total);
      // The text box draws the name, caret and selection itself.
      scene->variables["yourname"] = "";
    }
    if (!DrawScene(won ? 7 : 8)) {
      UiArt(won ? "victory" : "defeat", Rect(0, 0, 550, 400));
      return;
    }
    if (won) {
      PlaceNameBox();
      nameBox->Draw();
    }
    return;
  }
  // Flash draws off-stage content; the guard clip below frames it.
  glDisable(GL_SCISSOR_TEST);
  bool landing = screen == Screen::Landing;
  WorldBlend blend(screen == Screen::Play ? std::clamp(accumulator / Tick, 0.f, 1.f)
                                          : 1.f);
  if (fieldClip) {
    // game.hint: hidden until the hero lands, then toggled with I.
    if (auto *hint = fieldClip->child("hint")) {
      hint->visible = hintsVisible && !landing;
      hint->textPixelOffset = glm::vec2(1, -1) * scale;
    }
    fieldClip->draw(WorldBase());
  } else
    RegisteredSprite("field", {0, 0});
  // Scorch marks and corpses sit under everything else (depths 500-700).
  auto drawEffects = [&](bool ground) {
    for (auto &e : effects)
      if (e.ground == ground) {
        auto transform = WorldBase() * swf::Affine::translate(e.pos) *
                         swf::Affine::rotate(glm::radians(e.rotation)) *
                         swf::Affine::scale(glm::vec2(e.scale));
        glm::vec4 tint(1, 1, 1, std::clamp(e.alpha / 100, 0.f, 1.f));
        if (auto *frame = swf::library.effectFrame(e.clip->id, e.clip->frame))
          swf::DrawTexture(*frame->texture, transform, frame->origin,
                           frame->size, tint);
        else
          e.clip->draw(transform, {tint});
      }
  };
  drawEffects(true);
  PoseBuildings(false);
  for (int i = 0; i < 10; ++i) {
    const auto &b = buildings[i];
    if (b.removed)
      continue;
    if (b.clip)
      b.clip->draw(WorldBase() * swf::Affine::translate(b.pos) *
                   swf::Affine::scale(glm::vec2(BuildingScale(i))));
    else
      RegisteredSprite(BuildingClipName(i), b.pos, BuildingScale(i));
  }
  for (auto &u : units)
    DrawUnit(u);
  if (!landing)
    DrawUnit(player);
  for (auto &b : bullets) {
    if (b.clip) {
      // Projectile clips point along +x; _rotation is the direction of travel.
      b.clip->draw(WorldBase() * swf::Affine::translate(b.pos) *
                   swf::Affine::rotate(glm::radians(b.rotation)));
      continue;
    }
    auto p = WorldPoint(b.pos), tail = WorldPoint(b.pos - Heading(b) * b.v * .5f);
    DrawLine(tail.x, tail.y, p.x, p.y, b.h != Owner::None ? YELLOW : GREEN,
             (b.v != 50 ? 3.f : 1.f) * scale);
  }
  drawEffects(false);
  if (departing && dropshipClip)
    dropshipClip->draw(WorldBase() * swf::Affine::translate(dropshipPos) *
                       swf::Affine::scale(glm::vec2(dropshipScale)));
  if (radarVisible) {
    // radar clip at (-10, 25): frame 503, then layer1 scrolled to
    // (35 - x / 30, 45 - y / 30) under mask 505, whose shape 504 is convex.
    UiArt("radarFrame", Rect(-5.75f, 27.2f, 72.25f, 88.6f));
    const glm::vec2 mask[] = {{0, 0},         {51.3f, 0},  {69.2f, 17.9f},
                              {69.2f, 64.1f}, {51.3f, 82}, {0, 82}};
    auto corner = [&](int i) {
      return glm::vec3(UiPoint(glm::vec2(-5.6f, 30.2f) + mask[i]), 0);
    };
    glEnable(GL_STENCIL_TEST);
    glClear(GL_STENCIL_BUFFER_BIT);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    for (int i = 1; i < 5; ++i)
      DrawTriangle(corner(0), corner(i), corner(i + 1), WHITE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    // layer1: black shape 506, grid hairlines 508 and field outline 509. Flash
    // draws hairlines one pixel wide at any scale, so snap them to pixels.
    glm::vec2 layer = glm::vec2(25, 70) - FocusNow() / 30.f;
    UiRect(Rect(layer + glm::vec2(-100, -110), {240, 250}), BLACK);
    auto hairline = [&](glm::vec2 a, glm::vec2 b, glm::vec4 color) {
      DrawLine(glm::floor(UiPoint(layer + a)) + .5f,
               glm::floor(UiPoint(layer + b)) + .5f, color);
    };
    const glm::vec4 grid(.2f, .2f, .2f, 1);
    for (int i = 0; i < 16; ++i) {
      hairline({-35.5f + 10 * i, -61.5f}, {-35.5f + 10 * i, 131.5f}, grid);
      hairline({-57, -40.f + 10 * i}, {136, -40.f + 10 * i}, grid);
    }
    const glm::vec4 outline(142 / 255.f, 128 / 255.f, 96 / 255.f, 1);
    const glm::vec2 field[] = {
        {4.45f, -17.05f}, {57.4f, -17.05f}, {57.4f, 89.95f}, {4.45f, 89.95f}};
    for (int i = 0; i < 4; ++i)
      hairline(field[i], field[(i + 1) % 4], outline);
    auto dot = [&](const Actor &a, const char *key) {
      if (!a.removed)
        UiArt(key, Rect(layer + a.pos / 30.f - glm::vec2(2.5f), {5, 5}));
    };
    // Source depths: buildings, then enemies, then heroes (player first).
    if (!landing) {
      for (auto &b : buildings)
        dot(b, "dot");
      for (auto &u : units)
        if (!u.friendly)
          dot(u, "dote");
      dot(player, "dotb");
      for (auto &u : units)
        if (u.friendly)
          dot(u, "dotr");
    }
    glDisable(GL_STENCIL_TEST);
  }
  // Stage overlays by depth, all under the bar (11): pause/shop screen (3),
  // quality menu (4), confirm (5), equip (6), unmoney (7), same (8), warn (9)
  // and status (10).
  // Flash's static glyph placement differs from the exported outlines.
  // Apply the measured nudge to glyphs only, including nested help pages.
  if (screenClip)
    screenClip->textPixelOffset = glm::vec2(1, -1) * scale;
  if (helpClip)
    helpClip->textPixelOffset = glm::vec2(1, -1) * scale;
  if (qualClip)
    qualClip->textPixelOffset = glm::vec2(1, 0) * scale;
  if ((screen == Screen::Pause || screen == Screen::Shop) && screenClip)
    screenClip->draw(StageBase());
  if (qualVisible && qualClip)
    qualClip->draw(StageBase() * swf::Affine::translate({250, 200}));
  if (!landing) {
    if (confirmVisible && confirmClip)
      confirmClip->draw(StageBase() * swf::Affine::translate({150, 100}));
    if (screen == Screen::Shop && equipClip)
      equipClip->draw(StageBase() * swf::Affine::translate({385, 50}));
    if (unmoneyVisible && unmoneyClip)
      unmoneyClip->draw(StageBase() * swf::Affine::translate({150, 100}));
    if (sameVisible && sameClip)
      sameClip->draw(StageBase() * swf::Affine::translate({150, 100}));
    if (warnVisible && warnClip)
      warnClip->draw(StageBase() * swf::Affine::translate({200, 300}));
    if (statusVisible)
      DrawStatus();
  }
  UiArt("hud", Rect(-24.95f, -1.1f, 619, 54.5f));
  swf::DrawStaticTexts(613, StageBase() * swf::Affine::translate({-25, 0}));
  // Live children of the bar clip (placed at x -25), in SWF stage pixels.
  // herohp/heromana: _width = value / max * 130 at 80% height.
  auto meter = [&](const char *key, float x, float fraction) {
    UiArt(key, Rect(x, 14.4f, 130 * std::clamp(fraction, 0.f, 1.f), 8));
  };
  // Until heroz first runs, the meters keep their authored full width while
  // the text fields still read 0/0; the hero's death empties them.
  meter("herohp", 107.8f,
        landing ? 1 : (player.removed ? 0 : player.hp / player.maxHp));
  meter("heromana", 272.4f,
        landing ? 1 : (player.removed ? 0 : energy / maxEnergy));
  UiArt("weapview" + std::to_string(weapon + 1), Rect(432.3f, 18.2f, 89.5f, 30));
  // slotN frames: 1 empty, N + 1 owned, N + 5 selected.
  for (int i = 0; i < 4; ++i) {
    bool owned = i == 0 || specials[i - 1];
    int frame = !owned ? 1 : (weapon == i ? i + 6 : i + 2);
    UiArt("slot" + std::to_string(frame),
          Rect(i < 2 ? 520.45f : 533.2f, i % 2 ? 31.5f : 18.5f, 12.05f, 17));
  }
  // Right-aligned device-font fields. Flash's text ends about 4px inside the
  // field (2px gutter plus the glyphs' trailing bearing); the baseline sits
  // about 11.6px below the field top.
  auto field = [&](const std::string &text, float right, float top,
                   glm::vec4 color = WHITE) {
    glm::vec2 at = UiPoint({right - 4, top + 11.6f});
    if (deviceFont)
      DrawText(text, at, 9 * UiScale(), color, *deviceFont,
               TextAlignmentX::RIGHT);
    else
      UiText(text, right - 2, top + 11.6f, 10, color, TextAlignmentX::RIGHT);
  };
  const glm::vec4 gold(1, .8f, 0, 1);
  field(std::to_string(cash), 80.5f, 2.3f, gold);
  field(level > 0 ? "+" + std::to_string(50 * level) : "0", 80.5f, 14.3f,
        gold);
  field(landing ? "0/0" : player.removed ? "0"
                 : std::to_string(int(std::ceil(player.hp))) + "/" +
                       std::to_string(int(player.maxHp)),
        239.5f, 1.4f);
  field(landing ? "0/0" : player.removed ? "0"
                 : std::to_string(int(std::ceil(energy))) + "/" +
                       std::to_string(int(maxEnergy)),
        403.5f, .9f);
  field(std::to_string(level), 473.5f, 2.5f);
  int minutes = std::abs(2 - int(std::ceil(std::fmod(timer / 600.f, 60.f))));
  int seconds = 60 - int(std::ceil(std::fmod(timer / 10.f, 60.f)));
  field(landing ? "0" : std::to_string(minutes) + ":" + std::to_string(seconds),
        503, 2.5f);
  // "guard" (depth 14, at -78,-103): a bevelled surround hiding off-stage art.
  UiArt("guard", Rect(-457.9f, -402.85f, 1531.75f, 1207.75f));
  // heroz hides the logo (gameads) while the shop is open.
  if (screen != Screen::Shop)
    UiArt("logo", Rect(410, 360, 120, 24));
  if (landing) {
    auto pose = LandingBlend(cinematicAge);
    if (dropshipClip)
      dropshipClip->draw(WorldBase() * swf::Affine::translate(pose.ship) *
                         swf::Affine::scale(glm::vec2(pose.scale)));
    else {
      auto jetCentre =
          pose.ship +
          glm::vec2(57 - .2f * 54.65f, -23.4f + .15f * 155.15f) * pose.scale;
      Sprite("jets", WorldPoint(jetCentre),
             glm::vec2(253 * .15f, 117 * .2f) * pose.scale * WorldZoom(),
             float(PI / 2));
      RegisteredSprite("dropshipBody", pose.ship, pose.scale);
    }
    if (skipClip)
      skipClip->draw(StageBase() * swf::Affine::translate({410, 320}));
    else {
      UiArt("skipplay", Rect(410, 320, 117, 32));
      Hover(Rect(410, 320, 117, 32));
    }
    if (circleClip)
      circleClip->draw(StageBase() * swf::Affine::translate(UiMouse()));
    return;
  }
  // circle (depth 12) follows the pointer, except in the shop.
  if (screen != Screen::Shop && circleClip)
    circleClip->draw(StageBase() * swf::Affine::translate(UiMouse()));
  // help (depth 13, at 150, 100): one page per frame, with Next/Prev buttons.
  if (helpVisible && helpClip)
    helpClip->draw(StageBase() * swf::Affine::translate({150, 100}));
}
void ShutdownGame() {
  nameBox.reset(); // Refers to the fonts released below
  // MyEngine retains non-owning audio registries. Remove this game's entries
  // before deleting them so CloseWindow never visits a freed object.
  if (extras::openAL) {
    for (const auto &entry : sounds)
      std::erase(extras::openAL->loadedSounds, entry.second.get());
    if (wind)
      std::erase(extras::openAL->loadedMusicSounds, wind.get());
  }
  if (wind)
    wind->stop();
  wind.reset();
  sounds.clear();
  fieldClip.reset();
  splashClip.reset();
  helpClip.reset();
  skipClip.reset();
  introShip.reset();
  dropshipClip.reset();
  for (auto *clip : {&statusClip, &circleClip, &screenClip, &qualClip,
                     &warnClip, &sameClip, &unmoneyClip, &equipClip,
                     &confirmClip})
    clip->reset();
  for (auto &icon : slotIcons)
    icon.reset();
  sceneClips.clear();
  player = {};
  for (auto &b : buildings)
    b = {};
  swf::drawEditText = nullptr;
  swf::resolveButton = nullptr;
  swf::ReleaseFilterTargets();
  swf::library.clear();
  art.clear();
  crops.clear();
  units.clear();
  bullets.clear();
  effects.clear();
  if (engineFont) {
    extras::defaultFont = engineFont;
    extras::defaultTextRenderer->setFont(*engineFont);
    engineFont = nullptr;
  }
  uiFont.reset();
  deviceFont.reset();
}

std::string ConfigureGamePreview(int index) {
  if (screen == Screen::Missing)
    return "missing";
  testPointer = nullptr;
  Reset();
  switch (index) {
  case 0:
    screen = Screen::Title;
    return "title";
  case 1:
    screen = Screen::Difficulty;
    return "difficulty";
  case 2:
    screen = Screen::Instructions;
    return "instructions";
  case 3:
    screen = Screen::Intro;
    cinematicAge = 3;
    return "intro";
  case 4:
    screen = Screen::Landing;
    cinematicAge = 1.5f;
    return "landing";
  case 5:
    screen = Screen::Play;
    helpVisible = true;
    if (helpClip)
      helpClip->gotoAndStop(0);
    return "help";
  case 6:
    screen = Screen::Play;
    hintsVisible = true;
    return "gameplay";
  case 7:
    screen = Screen::Scores;
    return "scores";
  case 8:
    screen = Screen::Won;
    return "victory";
  case 9:
    screen = Screen::Lost;
    return "defeat";
  default: {
    static PointerInput hoverInput;
    hoverInput.normalized = UiPoint({450, 139}) / GetCurrentWindowSize();
    hoverInput.down = false;
    testPointer = &hoverInput;
    previousTestPress = false;
    screen = Screen::Title;
    return "title-hover";
  }
  }
}

bool RunGameChecks() {
  bool ok = true;
  int count = 0;
  bool wasMuted = muted;
  muted = true;
  auto check = [&](bool condition, const char *message) {
    ++count;
    if (!condition) {
      std::cerr << "FAIL: " << message << '\n';
      ok = false;
    }
  };
  if (screen == Screen::Missing)
    return false;
  // A shop click followed by the confirm dialog's Yes, and a sale.
  auto buy = [](int chk) {
    OfferDeal(chk);
    if (confirmVisible)
      ConfirmDeal();
  };
  auto sell = [](int chk) {
    OfferSale(chk);
    ConfirmDeal();
  };
  Reset();
  check(player.hp == 500 && cash == 100 &&
            buildings[9].hp == 10000 * difficulty && turretLevel[0] == 0 &&
            turretLevel[1] == 1,
        "initial source stats");
  buy(3);
  check(unmoneyVisible && !confirmVisible && cash == 100 && !Owned(3),
        "insufficient funds show unmoney and leave balance unchanged");
  unmoneyVisible = false;
  cash = 10000;
  buy(2);
  check(Owned(2) && cash == 9300 && player.armor == 11 && player.maxHp == 600,
        "equipment purchase");
  buy(1);
  check(sameVisible && !Owned(1) && cash == 9300, "one item of each type");
  sameVisible = false;
  sell(2);
  check(!Owned(2) && cash == 9650 && player.armor == 1 && player.maxHp == 500,
        "sale restores stats and returns half price");
  buy(9);
  check(heroShock == .1f, "Shield III reduces knockback");
  Reset();
  Actor target;
  target.hp = target.maxHp = 100;
  target.armor = 10;
  Hurt(target, 20);
  check(std::abs(target.hp - 90) < .001f, "armor damage reduction");
  for (int i = 0; i < 20; ++i)
    SpawnMarine({800, 1500});
  for (int i = 0; i < 20; ++i)
    SpawnAlien(0);
  int marines = 0, aliens = 0;
  for (auto &u : units)
    (u.friendly ? marines : aliens)++;
  check(marines == 9 && aliens == 10, "nine marine and ten alien slots");
  Reset();
  timer = 1199;
  Step();
  aliens = 0;
  bool atGate = true;
  for (auto &u : units)
    if (!u.friendly) {
      ++aliens;
      atGate = atGate && u.kind == 0 && std::abs(u.pos.x - 572.5f) <= 100;
    }
  check(aliens == 4 && atGate, "first wave: four creeps at porte1");
  Step();
  check(level == 1 && timer == 0, "wave rollover");
  Reset();
  player.pos = {739, 300};
  int shells = 0;
  for (int i = 0; i < 19; ++i)
    Step();
  for (auto &b : bullets)
    shells += b.flight == Flight::Shotur;
  check(shells == 0, "plant turret holds fire for 19 frames");
  Step();
  for (auto &b : bullets)
    shells += b.flight == Flight::Shotur;
  check(shells == 2, "both plant turrets fire on frame 20");
  Reset();
  selectedTurret = 3;
  cash = 10000;
  buy(25);
  check(buildings[3].building && cash == 8000, "turret purchase starts building");
  for (int i = 0; i < 990; ++i)
    Step();
  check(buildings[3].building, "turret still building after 990 frames");
  for (int i = 0; i < 20; ++i)
    Step();
  check(!buildings[3].building && buildings[3].hp > 0,
        "turret finishes building after about 1000 frames");
  Reset();
  Shoot("grenade", {960, 2055}, 10, 90, Flight::Shot, Owner::Hero, 2);
  float centreHp = buildings[0].hp;
  Step();
  check(std::abs(buildings[0].hp - (centreHp - 500 * 10.f / 110)) < .01f,
        "explosives damage friendly buildings");
  Reset();
  buildings[9].hp = 0;
  Step();
  check(winCounter == 50, "hive destruction starts the win countdown");
  for (int i = 0; i < 48; ++i)
    Checker();
  check(screen == Screen::Play, "play continues during the countdown");
  Checker();
  check(screen == Screen::Won, "hive destruction wins mission");
  {
    // Frame 8: point2 is undefined at the first victory, so no level bonus;
    // the next victory compares the stored 0 and earns 40 - waves.
    auto savedPoint2 = point2;
    point2.reset();
    level = 3;
    EndMission(true);
    bool firstNoBonus = point2 == 0 && total == cash + kills * 100;
    EndMission(true);
    check(firstNoBonus && point2 == 37 && total == cash + 37000 + kills * 100,
          "victory bonus follows frame 8's point2 < 40 test");
    point2 = savedPoint2;
    // The name field (the engine's text box) takes typed text.
    std::string savedName = yourname;
    UpdateGame(0);
    nameBox->hasFocus = true;
    nameBox->text = yourname = "";
    nameBox->cursorPosition = 0;
    for (char c : std::string("Joe"))
      extras::codepointsData.push_back(c);
    UpdateGame(0);
    extras::codepointsData.clear();
    check(yourname == "Joe" && screen == Screen::Won,
          "victory name field accepts typed text");
    yourname = savedName;
    nameBox->text = yourname;
    nameBox->hasFocus = false;
  }
  Reset();
  buildings[0].hp = 0;
  Step();
  for (int i = 0; i < 49; ++i)
    Checker();
  check(screen == Screen::Lost, "command centre destruction loses mission");
  Reset();
  player.hp = 0;
  Step();
  check(player.removed && loseCounter == 50, "marine death starts the defeat");
  Reset();
  auto originalPoint = glm::vec2(425, 358);
  auto roundTrip = (UiPoint(originalPoint) - UiOrigin()) / UiScale();
  check(Length(roundTrip - originalPoint) < .001f,
        "scaled menu coordinates round trip");
  auto projected = WorldPoint(player.pos);
  auto stageCentre = GetCurrentWindowSize() / (2.f * UiScale());
  check(Length(projected - UiPoint(stageCentre + glm::vec2(5, 0))) <
            1.5f * UiScale(),
        "source camera uses viewport dimensions with the resting shake offset");
  auto aimAtPlayer = projected / WorldZoom() + camera;
  check(Length(aimAtPlayer - player.pos) < .001f,
        "world projection and mouse aiming remain inverse transforms");
  BeginLanding();
  check(screen == Screen::Landing && cinematicAge == 0,
        "mission begins with landing sequence");
  BeginMission(false);
  check(screen == Screen::Play && helpVisible, "landing opens help overlay");
  // Feed the normalized coordinates supplied by the engine through the actual
  // hit-testing and UpdateGame transitions, without moving the user's cursor.
  PointerInput testInput;
  testPointer = &testInput;
  previousTestPress = false;
  auto movePointer = [&](glm::vec2 stagePoint) {
    auto pixels = UiPoint(stagePoint);
    testInput.normalized = pixels / GetCurrentWindowSize();
    return pixels;
  };
  auto mouseEvent = [&](glm::vec2 stagePoint, bool down) {
    movePointer(stagePoint);
    testInput.down = down;
    UpdateGame(0);
  };
  auto clickPointer = [&](glm::vec2 stagePoint) {
    mouseEvent(stagePoint, true);
    mouseEvent(stagePoint, false);
  };
  Reset();
  screen = Screen::Title;
  mouseEvent({10, 200}, false);
  movePointer({450, 139});
  check(Over(Rect(398, 123, 117, 32)),
        "normalized cursor hovers over Play Game");
  check(Length(UiMouse() - glm::vec2(450, 139)) < 1,
        "normalized engine cursor converts to stage pixels");
  clickPointer({450, 139});
  check(screen == Screen::Difficulty, "Play Game click opens difficulty");
  clickPointer({290, 193});
  check(screen == Screen::Intro && difficulty == .8f,
        "Normal click starts the intro");
  clickPointer({480, 374});
  check(screen == Screen::Landing, "intro Skip click starts landing");
  clickPointer({465, 336});
  check(screen == Screen::Play && helpVisible && !dropshipClip,
        "Skip and Play opens help and removes the dropship");
  screen = Screen::Title;
  mouseEvent({450, 181}, true);
  check(screen == Screen::Title, "a press alone does not act, as on(release)");
  mouseEvent({450, 181}, false);
  check(screen == Screen::Instructions, "Instructions click opens controls");
  mouseEvent({480, 374}, true);
  mouseEvent({10, 200}, false);
  check(screen == Screen::Instructions,
        "releasing outside the button cancels its click");
  clickPointer({480, 374});
  check(screen == Screen::Title,
        "release outside buttons permits the next click");
  movePointer({10, 200});
  check(!Over(Rect(398, 123, 117, 32)), "cursor outside Play does not hover");
  testPointer = nullptr;
  SamplePointer();
  Reset();
  auto *hintRaster = swf::library.textVector(103, WorldZoom());
  check(hintRaster && std::abs(hintRaster->scale - WorldZoom()) < .0002f,
        "field text raster matches display zoom without secondary scaling");
  bool completeImpacts = true;
  for (const char *name : {"ball4", "ball2", "green", "red", "purple",
                           "ballu", "ball3", "ball"}) {
    auto clip = MakeClip(name);
    completeImpacts &= bool(clip);
    if (clip)
      for (int frame = 0; frame < clip->frameCount(); ++frame) {
        auto *raster = swf::library.effectFrame(clip->id, frame);
        completeImpacts &= raster && raster->texture &&
                           raster->size.x > 0 && raster->size.y > 0;
      }
  }
  check(completeImpacts, "all original intermediate impact frames are available");
  screen = Screen::Pause;
  AddEffect("ball4", player.pos);
  for (int tick = 0; tick < 12; ++tick)
    UpdateGame(Tick);
  check(effects.empty(), "impact timeline finishes while physics is paused");
  Reset();
  // hitTest: a rifle shot's art (27 px long) reaches a turret before its
  // centre does, and only where it stands this frame.
  {
    Bullet probe;
    probe.clip = MakeClip("bull");
    Box box = ClipBox(buildings[5]);
    probe.pos = {box.min.x - 10, (box.min.y + box.max.y) / 2};
    bool reach = Strikes(probe, buildings[5]);
    probe.pos.x = box.min.x - 20;
    check(reach && !Strikes(probe, buildings[5]),
          "shots hit by their art's bounding box, as hitTest does");
  }
  Reset();
  muted = wasMuted;
  std::cout << "Gameplay checks: " << count << ", " << (ok ? "PASS" : "FAIL")
            << std::endl;
  return ok;
}
