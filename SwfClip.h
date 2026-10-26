#pragma once

// A small Flash timeline player for the exported SWF (tools/export_timelines.py).
// Clips keep Flash's rules: nested clips run independently at the movie's
// frame rate, frame scripts stop/play/goto, instances persist while their
// placement does, and scripts may take over a child's transform.

#include "MyEngine.h"

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <string>
#include <vector>

namespace swf {

// Flash matrix: x' = a x + c y + tx, y' = b x + d y + ty.
struct Affine {
  float a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;

  Affine operator*(const Affine &o) const;
  glm::vec2 apply(glm::vec2 p) const;
  static Affine translate(glm::vec2 t);
  static Affine scale(glm::vec2 s);
  static Affine rotate(float radians);
};

struct ColorTransform {
  glm::vec4 mul{1.0f};
  glm::vec4 add{0.0f};
  ColorTransform operator*(const ColorTransform &child) const;
};

// Glow and drop-shadow filters; other kinds are kept but drawn unfiltered.
struct Filter {
  enum class Type { Glow, Shadow, Other } type = Type::Other;
  glm::vec4 color{1.0f};
  float blurX = 0, blurY = 0, strength = 1, angle = 0, distance = 0;
  bool inner = false, knockout = false;
};

struct Placement {
  int depth = 0, id = 0, serial = 0, clipDepth = 0;
  Affine matrix;
  ColorTransform color;
  float ratio = 0;
  std::string name;
  std::vector<Filter> filters;
};

struct Action {
  enum Op { Stop, Play, GotoPlay, GotoStop, Remove } op = Stop;
  int frame = 0;
};

struct Timeline {
  std::vector<std::vector<Placement>> frames;
  std::map<int, std::vector<Action>> actions;
  std::map<std::string, int> labels;
};

enum class Kind { Unknown, Shape, Morph, Text, Edit, Button, Sprite };

struct ButtonRecord {
  int states = 0, id = 0, depth = 0;
  Affine matrix;
  ColorTransform color;
  std::vector<Filter> filters;
};

struct EditText {
  std::string text, variable;
  float size = 12;
  glm::vec4 color{0, 0, 0, 1};
  int align = 0; // 0 left, 1 right, 2 centre, 3 justify
  bool html = false, outlines = false, multiline = false;
  bool border = false; // A white backdrop with a black outline
};

struct Character {
  Kind kind = Kind::Unknown;
  glm::vec4 bounds{0}, endBounds{0}; // xmin, xmax, ymin, ymax
  float zoom = 2; // Artwork pixels per SWF pixel
  std::vector<ButtonRecord> records;
  EditText edit;
};

class Library {
public:
  bool load(const std::string &directory);
  bool loaded() const { return !timelines.empty(); }
  const Timeline *timeline(int id) const;
  const Character *character(int id) const;
  int exportId(const std::string &name) const;
  Texture *shape(int id);
  Texture *morph(int id, bool end);
  // Full original timeline frames preserve morph geometry between endpoints.
  struct EffectFrame {
    std::unique_ptr<Texture> texture;
    glm::vec2 origin{}, size{};
  };
  const EffectFrame *effectFrame(int sprite, int frame);
  Texture *text(int id);
  // Vector art rasterized for `pixelScale` screen pixels per SWF pixel, as
  // Flash draws it: static text and shapes with sub-pixel strokes, which
  // resampling a fixed-size export leaves faint and soft.
  struct Vector {
    std::unique_ptr<Texture> texture;
    float scale = 1; // texture pixels per SWF pixel
  };
  const Vector *textVector(int id, float pixelScale);
  const Vector *hairlineVector(int id, float pixelScale);
  // Shapes always drawn from their SVGs at screen scale, as Flash draws
  // vectors: for art squashed on one axis (wipes), whose resampled export
  // blurs at the edges.
  std::set<int> vectorShapes;
  // `phase` offsets the art within its raster, in raster pixels, so it can
  // be drawn at an exact sub-pixel position.
  // Null for bitmap-filled shapes, which nanosvg would draw empty.
  const Vector *shapeVector(int id, float pixelScale, glm::vec2 phase);
  // One of vectorShapes that is a single bitmap fill over its own rectangle:
  // the embedded bitmap, its fill matrix and size in bitmap pixels. Bitmaps
  // Flash does not smooth sample the nearest pixel, as Flash does.
  struct Bitmap {
    std::unique_ptr<Texture> texture;
    Affine matrix;
    glm::vec2 size{0.0f};
    bool hasImage = false; // Any bitmap at all: nanosvg cannot draw these
  };
  const Bitmap *bitmapFill(int id);
  void clear();

  float frameRate = 24;
  float zoom = 2; // Artwork pixels per SWF pixel

private:
  Texture *cached(std::map<int, std::unique_ptr<Texture>> &cache, int id,
                  const std::string &path, bool svg);
  const Vector *vector(int id, const std::string &path, float pixelScale,
                       bool flashType, glm::vec2 phase = glm::vec2(0.0f));

  std::string directory;
  std::map<int, Timeline> timelines;
  std::map<int, Character> characters;
  std::map<std::string, int> exports;
  std::map<int, std::unique_ptr<Texture>> shapes, morphStarts, morphEnds,
      texts;
  // (id, scale step, phase x and y in eighths of a pixel)
  std::map<std::tuple<int, int, int, int>, Vector> vectors;
  std::map<int, bool> hairlines;                 // shape id -> thin strokes
  std::map<int, Bitmap> bitmaps;
  struct EffectSequence {
    glm::vec2 origin{};
    float zoom = 2;
    std::vector<std::string> files;
  };
  std::map<int, EffectSequence> effectSequences;
  std::map<std::pair<int, int>, EffectFrame> effectFrames;
};

extern Library library;

enum class ButtonState { Up, Over, Down };

class Clip {
public:
  explicit Clip(int id);
  static std::unique_ptr<Clip> fromExport(const std::string &name);

  int id = 0;
  int frame = 0;
  bool playing = true;
  bool removed = false;
  bool visible = true;
  // Set by scripts (like _rotation/_x): the timeline then leaves it alone.
  Affine matrix;
  bool matrixLocked = false;
  ColorTransform color;
  std::string name;
  // Values for edit-text fields whose variable names live on this clip.
  std::map<std::string, std::string> variables;
  ButtonState buttonState = ButtonState::Up;
  // Offsets (in this clip's space) for the children at these depths. Flash
  // draws some static text a pixel or two away from its exported geometry.
  std::map<int, glm::vec2> depthOffsets;
  // Window-pixel correction for static glyphs, inherited by nested clips.
  // Artwork, masks, editable fields and button hit areas keep their positions.
  glm::vec2 textPixelOffset{0};

  int frameCount() const;
  void gotoAndStop(int frame);
  void gotoAndPlay(int frame);
  void gotoAndStop(const std::string &label);
  void gotoAndPlay(const std::string &label);
  void play() { playing = true; }
  void stop() { playing = false; }
  // Child by instance name; dotted paths walk nested clips ("clip.leg").
  Clip *child(const std::string &path);
  // Sprite or button child placed at `depth`, for unnamed instances.
  Clip *childAt(int depth);
  // Script-style transform edits, decomposed like Flash's _xscale/_rotation.
  void setRotation(float degrees);
  void setScale(glm::vec2 scale);
  void setPosition(glm::vec2 position);
  // Flash _width for unrotated clips: scales x so the current frame spans
  // `width` in the parent's space.
  void setWidth(float width);
  glm::vec2 position() const { return {matrix.tx, matrix.ty}; }
  // Flash _xscale/_yscale as factors.
  glm::vec2 scale() const {
    return {std::sqrt(matrix.a * matrix.a + matrix.b * matrix.b),
            std::sqrt(matrix.c * matrix.c + matrix.d * matrix.d)};
  }
  // Current frame's bounds in this clip's own space (Flash getBounds(this)):
  // xmin, xmax, ymin, ymax. Empty clips return zeros.
  glm::vec4 bounds() const;

  // Advances one movie frame (1 / frameRate seconds), parents before children.
  void tick();
  void draw(const Affine &base, const ColorTransform &tint = {});
  // For scenes whose static art is drawn elsewhere: draws only buttons that
  // are hovered or pressed (or every button, with `allStates`), then the
  // higher-depth content stacked over them.
  void drawButtons(const Affine &base, bool allStates = false);
  // Buttons: asks resolveButton for the state of this button drawn at `world`.
  void updateButton(const Affine &world);

private:
  glm::vec2 inheritedTextPixelOffset{0};
  struct Child {
    Placement placement;
    std::unique_ptr<Clip> clip; // Sprites and buttons only
  };

  Affine childWorld(const Affine &base, int depth, const Affine &local) const;
  void enter(int frame, int depth = 0);
  void sync();
  void runActions(int depth);
  void drawCharacter(Child &child, const Affine &world,
                     const ColorTransform &tint);
  void drawFiltered(Child &child, const Affine &world,
                    const ColorTransform &tint);

  Kind kind = Kind::Sprite;
  bool born = true; // Created this tick: shows its first frame once
  std::map<int, Child> children;
};

// Edit-text fields are drawn by the game (fonts, live values).
using EditTextDrawer = std::function<void(const EditText &, const std::string &,
                                          const Affine &world, const glm::vec4 &bounds,
                                          const ColorTransform &)>;
extern EditTextDrawer drawEditText;

// Decides a button's state from its hit area in draw coordinates.
using ButtonResolver = std::function<ButtonState(const Clip &, glm::vec4 worldBounds)>;
extern ButtonResolver resolveButton;

// Window pixels per stage pixel. Flash sizes filter blurs in stage pixels,
// regardless of the filtered clip's own scale.
extern float filterScale;

// Releases the offscreen targets used for filters (call before the GL
// context goes away).
void ReleaseFilterTargets();

// Draws only the static text placed on `sprite`'s first frame, for art
// exported without its labels (the HUD bar).
void DrawStaticTexts(int sprite, const Affine &base);

// Axis-aligned bounds (xmin, xmax, ymin, ymax) of `bounds` under `m`.
glm::vec4 TransformBounds(const Affine &m, glm::vec4 bounds);

// Draws a texture whose top-left is `origin` and size `size` in clip space.
void DrawTexture(Texture &texture, const Affine &world, glm::vec2 origin,
                 glm::vec2 size, glm::vec4 color);
// As above with a full Flash colour transform (multiply and add).
void DrawTexture(Texture &texture, const Affine &world, glm::vec2 origin,
                 glm::vec2 size, const ColorTransform &color);

} // namespace swf
