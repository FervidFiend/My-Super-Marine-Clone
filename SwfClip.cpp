#include "SwfClip.h"

#include <nanosvg/nanosvg.h>
#include <nanosvg/nanosvgrast.h>
#include <stb_image.h>
#include <nlohmann/json.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace swf {

Library library;
EditTextDrawer drawEditText;
ButtonResolver resolveButton;

Affine Affine::operator*(const Affine &o) const {
  return {a * o.a + c * o.b,       b * o.a + d * o.b,       a * o.c + c * o.d,
          b * o.c + d * o.d,       a * o.tx + c * o.ty + tx, b * o.tx + d * o.ty + ty};
}

glm::vec2 Affine::apply(glm::vec2 p) const {
  return {a * p.x + c * p.y + tx, b * p.x + d * p.y + ty};
}

Affine Affine::translate(glm::vec2 t) { return {1, 0, 0, 1, t.x, t.y}; }
Affine Affine::scale(glm::vec2 s) { return {s.x, 0, 0, s.y, 0, 0}; }
Affine Affine::rotate(float radians) {
  float cs = std::cos(radians), sn = std::sin(radians);
  return {cs, sn, -sn, cs, 0, 0};
}

ColorTransform ColorTransform::operator*(const ColorTransform &child) const {
  return {mul * child.mul, child.add * mul + add};
}

namespace {
Affine ToAffine(const nlohmann::json &m) {
  return {m[0].get<float>(), m[1].get<float>(), m[2].get<float>(),
          m[3].get<float>(), m[4].get<float>(), m[5].get<float>()};
}

glm::vec4 ToBounds(const nlohmann::json &b) {
  return {b[0].get<float>(), b[1].get<float>(), b[2].get<float>(),
          b[3].get<float>()};
}

ColorTransform ToColor(const nlohmann::json &cx) {
  return {{cx[0].get<float>(), cx[1].get<float>(), cx[2].get<float>(),
           cx[3].get<float>()},
          {cx[4].get<float>(), cx[5].get<float>(), cx[6].get<float>(),
           cx[7].get<float>()}};
}

std::vector<Filter> ToFilters(const nlohmann::json &list) {
  std::vector<Filter> filters;
  for (auto &f : list) {
    Filter filter;
    std::string type = f.value("type", "");
    filter.type = type == "glow"     ? Filter::Type::Glow
                  : type == "shadow" ? Filter::Type::Shadow
                                     : Filter::Type::Other;
    if (f.contains("color")) {
      auto &c = f["color"];
      filter.color = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>(),
                      c[3].get<float>()};
    }
    filter.blurX = f.value("blurX", 0.0f);
    filter.blurY = f.value("blurY", 0.0f);
    filter.strength = f.value("strength", 1.0f);
    filter.angle = f.value("angle", 0.0f);
    filter.distance = f.value("distance", 0.0f);
    int flags = f.value("flags", 0);
    filter.inner = flags & 0x80;
    filter.knockout = flags & 0x40;
    filters.push_back(filter);
  }
  return filters;
}

Kind ToKind(const std::string &type) {
  if (type == "shape") return Kind::Shape;
  if (type == "morph") return Kind::Morph;
  if (type == "text") return Kind::Text;
  if (type == "edit") return Kind::Edit;
  if (type == "button") return Kind::Button;
  if (type == "sprite") return Kind::Sprite;
  return Kind::Unknown;
}

// Axis-aligned bounds of a clip-space rectangle under a transform.
glm::vec4 WorldBounds(const Affine &world, glm::vec4 bounds) {
  glm::vec2 corners[] = {world.apply({bounds.x, bounds.z}),
                         world.apply({bounds.y, bounds.z}),
                         world.apply({bounds.x, bounds.w}),
                         world.apply({bounds.y, bounds.w})};
  glm::vec4 out(corners[0].x, corners[0].x, corners[0].y, corners[0].y);
  for (auto &p : corners) {
    out.x = std::min(out.x, p.x);
    out.y = std::max(out.y, p.x);
    out.z = std::min(out.z, p.y);
    out.w = std::max(out.w, p.y);
  }
  return out;
}

// Flash's _xscale/_yscale/_rotation view of a matrix.
struct Decomposed {
  float scaleX, scaleY, skewX, skewY;
};

Decomposed Decompose(const Affine &m) {
  return {std::sqrt(m.a * m.a + m.b * m.b), std::sqrt(m.c * m.c + m.d * m.d),
          std::atan2(-m.c, m.d), std::atan2(m.b, m.a)};
}

void Compose(Affine &m, const Decomposed &p) {
  m.a = p.scaleX * std::cos(p.skewY);
  m.b = p.scaleX * std::sin(p.skewY);
  m.c = -p.scaleY * std::sin(p.skewX);
  m.d = p.scaleY * std::cos(p.skewX);
}
} // namespace

bool Library::load(const std::string &dir) {
  clear();
  directory = dir;
  std::ifstream file(std::filesystem::path(dir) / "timelines.json");
  if (!file)
    return false;
  nlohmann::json doc;
  try {
    file >> doc;
  } catch (const std::exception &e) {
    std::cerr << "swf: " << e.what() << '\n';
    return false;
  }
  frameRate = doc.value("frameRate", 24.0f);
  zoom = doc.value("zoom", 2.0f);
  for (auto &[key, value] : doc["characters"].items()) {
    Character ch;
    ch.kind = ToKind(value.value("type", ""));
    if (value.contains("bounds"))
      ch.bounds = ToBounds(value["bounds"]);
    if (value.contains("endBounds"))
      ch.endBounds = ToBounds(value["endBounds"]);
    ch.zoom = value.value("zoom", zoom);
    if (ch.kind == Kind::Button)
      for (auto &r : value["records"]) {
        ButtonRecord record;
        record.states = r["states"];
        record.id = r["id"];
        record.depth = r["depth"];
        record.matrix = ToAffine(r["m"]);
        record.color = ToColor(r["cx"]);
        if (r.contains("filters"))
          record.filters = ToFilters(r["filters"]);
        ch.records.push_back(record);
      }
    if (ch.kind == Kind::Edit) {
      ch.edit.text = value.value("text", "");
      ch.edit.variable = value.value("variable", "");
      ch.edit.size = value.value("size", 12.0f);
      if (value.contains("color")) {
        auto &c = value["color"];
        ch.edit.color = {c[0].get<float>(), c[1].get<float>(),
                         c[2].get<float>(), c[3].get<float>()};
      }
      ch.edit.align = value.value("align", 0);
      ch.edit.html = value.value("html", false);
      ch.edit.outlines = value.value("outlines", false);
      ch.edit.multiline = value.value("multiline", false);
      ch.edit.border = value.value("border", false);
    }
    characters[std::stoi(key)] = std::move(ch);
  }
  for (auto &[key, value] : doc["sprites"].items()) {
    Timeline timeline;
    for (auto &frame : value["frames"]) {
      std::vector<Placement> placements;
      for (auto &item : frame) {
        Placement p;
        p.depth = item["depth"];
        p.id = item["id"];
        p.serial = item.value("serial", 0);
        p.clipDepth = item.value("clip", 0);
        p.ratio = item.value("ratio", 0.0f);
        p.name = item.value("name", "");
        p.matrix = ToAffine(item["m"]);
        if (item.contains("cx"))
          p.color = ToColor(item["cx"]);
        if (item.contains("filters"))
          p.filters = ToFilters(item["filters"]);
        placements.push_back(std::move(p));
      }
      timeline.frames.push_back(std::move(placements));
    }
    for (auto &[label, frame] : value["labels"].items())
      timeline.labels[label] = frame;
    for (auto &[frame, ops] : value["actions"].items())
      for (auto &op : ops) {
        Action action;
        std::string name = op[0];
        if (name == "stop") action.op = Action::Stop;
        else if (name == "play") action.op = Action::Play;
        else if (name == "remove") action.op = Action::Remove;
        else {
          action.op = name == "gotoPlay" ? Action::GotoPlay : Action::GotoStop;
          if (op[1].is_string()) {
            auto found = timeline.labels.find(op[1].get<std::string>());
            if (found == timeline.labels.end())
              continue;
            action.frame = found->second;
          } else
            action.frame = op[1];
        }
        timeline.actions[std::stoi(frame)].push_back(action);
      }
    timelines[std::stoi(key)] = std::move(timeline);
  }
  for (auto &[name, id] : doc["exports"].items())
    exports[name] = id;
  std::ifstream impactFile(std::filesystem::path(dir) / "impact-frames.json");
  if (impactFile) {
    nlohmann::json impacts;
    impactFile >> impacts;
    for (auto &[id, value] : impacts.items()) {
      EffectSequence sequence;
      sequence.origin = {value["origin"][0], value["origin"][1]};
      sequence.zoom = value.value("zoom", 2.f);
      sequence.files = value["frames"].get<std::vector<std::string>>();
      effectSequences[std::stoi(id)] = std::move(sequence);
    }
  }
  return true;
}

const Library::EffectFrame *Library::effectFrame(int sprite, int frame) {
  auto sequence = effectSequences.find(sprite);
  if (sequence == effectSequences.end() || frame < 0 ||
      frame >= static_cast<int>(sequence->second.files.size()))
    return nullptr;
  auto key = std::make_pair(sprite, frame);
  auto found = effectFrames.find(key);
  if (found != effectFrames.end())
    return &found->second;
  EffectFrame raster;
  raster.origin = sequence->second.origin;
  auto path = std::filesystem::path(directory) / sequence->second.files[frame];
  raster.texture = std::make_unique<Texture>(path.string(), true, false, true);
  raster.size = glm::vec2(raster.texture->width, raster.texture->height) /
                sequence->second.zoom;
  return &effectFrames.emplace(key, std::move(raster)).first->second;
}

void Library::clear() {
  timelines.clear();
  characters.clear();
  exports.clear();
  shapes.clear();
  morphStarts.clear();
  morphEnds.clear();
  texts.clear();
  vectors.clear();
  bitmaps.clear();
  hairlines.clear();
  effectFrames.clear();
  effectSequences.clear();
}

const Timeline *Library::timeline(int id) const {
  auto found = timelines.find(id);
  return found == timelines.end() ? nullptr : &found->second;
}

const Character *Library::character(int id) const {
  auto found = characters.find(id);
  return found == characters.end() ? nullptr : &found->second;
}

int Library::exportId(const std::string &name) const {
  auto found = exports.find(name);
  return found == exports.end() ? -1 : found->second;
}

// The exporter anti-aliases each fill on its own, so where two fills of a
// shape meet, a hairline of partial alpha lets whatever lies underneath show
// through (the dropship's jets through its hull). Flash rasterizes a shape's
// fills together, without seams. A translucent pixel with opaque pixels on
// opposite sides, at most two pixels apart, is such a seam: make it opaque,
// blending in its neighbours' colour. Outer edges have no opaque side across.
static bool CloseSeams(std::vector<unsigned char> &rgba, int w, int h) {
  const std::vector<unsigned char> source = rgba;
  auto at = [&](int x, int y) { return &source[(static_cast<size_t>(y) * w + x) * 4]; };
  const int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
  bool changed = false;
  for (int y = 2; y < h - 2; ++y)
    for (int x = 2; x < w - 2; ++x) {
      const unsigned char *p = at(x, y);
      if (p[3] >= 240)
        continue;
      for (auto [dx, dy] : directions) {
        const unsigned char *before = nullptr, *after = nullptr;
        for (int back = 1; back <= 2 && !before; ++back)
          for (int ahead = 1; ahead + back <= 3 && !before; ++ahead) {
            const unsigned char *b = at(x - dx * back, y - dy * back),
                                *a = at(x + dx * ahead, y + dy * ahead);
            if (b[3] >= 250 && a[3] >= 250) {
              before = b;
              after = a;
            }
          }
        if (!before)
          continue;
        unsigned char *d = &rgba[(static_cast<size_t>(y) * w + x) * 4];
        float own = p[3] / 255.0f;
        for (int c = 0; c < 3; ++c)
          d[c] = static_cast<unsigned char>(std::lround(
              p[c] * own + (before[c] + after[c]) * 0.5f * (1.0f - own)));
        d[3] = 255;
        changed = true;
        break;
      }
    }
  return changed;
}

Texture *Library::cached(std::map<int, std::unique_ptr<Texture>> &cache, int id,
                         const std::string &path, bool svg) {
  auto found = cache.find(id);
  if (found != cache.end())
    return found->second.get();
  std::unique_ptr<Texture> texture;
  if (std::filesystem::exists(path))
    // Exported at 2x or more and drawn smaller, so mip levels keep thin
    // strokes and text from breaking up.
    texture = svg ? std::make_unique<Texture>(path, true, false, true, 1.0f)
                  : std::make_unique<Texture>(path, true, false, true);
  // Mip levels of odd-sized exports straddle source texels, so art drawn at
  // half size bleeds a row past its edge (the title buttons' bottom rim).
  // Pad the image's right and bottom to a multiple of 4 with transparent
  // copies of the edge colours; drawn sizes follow the padded texture, so
  // the scale is unchanged.
  bool pixels = texture && !svg && texture->width > 0 &&
                texture->textureData.size() ==
                    static_cast<size_t>(texture->width) * texture->height * 4;
  bool sealed =
      pixels && CloseSeams(texture->textureData, texture->width, texture->height);
  if (pixels && (sealed || texture->width % 4 || texture->height % 4)) {
    int w = texture->width, h = texture->height;
    int pw = (w + 3) / 4 * 4, ph = (h + 3) / 4 * 4, extra = ph - h;
    std::vector<unsigned char> padded(static_cast<size_t>(pw) * ph * 4);
    // Rows are stored bottom first, so the new bottom rows come first.
    for (int y = 0; y < ph; ++y)
      for (int x = 0; x < pw; ++x) {
        int sy = std::max(y - extra, 0), sx = std::min(x, w - 1);
        const unsigned char *s =
            &texture->textureData[(static_cast<size_t>(sy) * w + sx) * 4];
        unsigned char *d = &padded[(static_cast<size_t>(y) * pw + x) * 4];
        std::copy(s, s + 4, d);
        if (y < extra || x >= w)
          d[3] = 0;
      }
    texture = std::make_unique<Texture>(pw, ph, std::move(padded));
    texture->updateSamplerDetails(GL_FilterModes::LINEAR,
                                  GL_WrapModes::CLAMP_TO_EDGE);
    texture->generateAlphaAwareMipmaps(false);
  }
  return (cache[id] = std::move(texture)).get();
}

Texture *Library::shape(int id) {
  return cached(shapes, id, directory + "/shapes/" + std::to_string(id) + ".png",
                false);
}

Texture *Library::morph(int id, bool end) {
  return cached(end ? morphEnds : morphStarts, id,
                directory + "/morphs/" + std::to_string(id) +
                    (end ? ".end.png" : ".start.png"),
                false);
}

Texture *Library::text(int id) {
  return cached(texts, id, directory + "/texts/" + std::to_string(id) + ".svg",
                true);
}

const Library::Vector *Library::textVector(int id, float pixelScale) {
  return vector(id, directory + "/texts/" + std::to_string(id) + ".svg",
                pixelScale, true);
}

const Library::Vector *Library::shapeVector(int id, float pixelScale,
                                            glm::vec2 phase) {
  if (!vectorShapes.count(id) || bitmapFill(id) || bitmaps[id].hasImage)
    return nullptr;
  return vector(id, directory + "/../shapes/" + std::to_string(id) + ".svg",
                pixelScale, false, phase);
}

static std::vector<unsigned char> DecodeBase64(const std::string &text) {
  std::vector<unsigned char> out;
  unsigned value = 0;
  int bits = 0;
  for (char ch : text) {
    int v = ch >= 'A' && ch <= 'Z'   ? ch - 'A'
            : ch >= 'a' && ch <= 'z' ? ch - 'a' + 26
            : ch >= '0' && ch <= '9' ? ch - '0' + 52
            : ch == '+'              ? 62
            : ch == '/'              ? 63
                                     : -1;
    if (v < 0)
      continue;
    value = value << 6 | v;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<unsigned char>(value >> bits & 0xff));
    }
  }
  return out;
}

const Library::Bitmap *Library::bitmapFill(int id) {
  if (!vectorShapes.count(id))
    return nullptr;
  auto found = bitmaps.find(id);
  if (found != bitmaps.end())
    return found->second.texture ? &found->second : nullptr;
  Bitmap &b = bitmaps[id];
  std::ifstream file(directory + "/../shapes/" + std::to_string(id) + ".svg");
  std::stringstream buffer;
  buffer << file.rdbuf();
  const std::string svg = buffer.str();
  auto count = [&](const char *s) {
    size_t n = 0;
    for (size_t at = svg.find(s); at != std::string::npos; at = svg.find(s, at + 1))
      ++n;
    return n;
  };
  b.hasImage = count("<image") > 0;
  if (count("<pattern") != 1 || count("<path") != 1 || count("<image") != 1)
    return nullptr;
  float m[6];
  size_t at = svg.find("patternTransform=\"matrix(");
  if (at == std::string::npos ||
      std::sscanf(svg.c_str() + at + 25, "%f, %f, %f, %f, %f, %f", &m[0], &m[1],
                  &m[2], &m[3], &m[4], &m[5]) != 6)
    return nullptr;
  size_t data = svg.find("base64,");
  if (data == std::string::npos)
    return nullptr;
  data += 7;
  auto bytes = DecodeBase64(svg.substr(data, svg.find('"', data) - data));
  int w = 0, h = 0, channels = 0;
  // Bottom row first, as the file loader's flipped textures are (the flag
  // is global, so set it either way).
  stbi_set_flip_vertically_on_load(true);
  unsigned char *image = stbi_load_from_memory(
      bytes.data(), static_cast<int>(bytes.size()), &w, &h, &channels, 4);
  if (!image)
    return nullptr;
  std::vector<unsigned char> pixels(image, image + static_cast<size_t>(w) * h * 4);
  stbi_image_free(image);
  b.matrix = {m[0], m[1], m[2], m[3], m[4], m[5]};
  b.size = {static_cast<float>(w), static_cast<float>(h)};
  // Only a fill that exactly covers the shape's own bounds is drawn this way.
  glm::vec4 area = WorldBounds(b.matrix, {0, b.size.x, 0, b.size.y});
  auto *ch = character(id);
  if (!ch || glm::any(glm::greaterThan(glm::abs(area - ch->bounds), glm::vec4(0.1f))))
    return nullptr;
  b.texture = std::make_unique<Texture>(w, h, std::move(pixels));
  b.texture->updateSamplerDetails(svg.find("pixelated") != std::string::npos
                                      ? GL_FilterModes::NEAREST
                                      : GL_FilterModes::LINEAR,
                                  GL_WrapModes::CLAMP_TO_EDGE);
  return &b;
}

const Library::Vector *Library::hairlineVector(int id, float pixelScale) {
  // FFDec marks strokes thinner than its export zoom can show.
  std::string path = directory + "/../shapes/" + std::to_string(id) + ".svg";
  auto found = hairlines.find(id);
  if (found == hairlines.end()) {
    std::ifstream file(path);
    std::stringstream svg;
    svg << file.rdbuf();
    found = hairlines
                .emplace(id, svg.str().find("has-small-stroke") !=
                                 std::string::npos)
                .first;
  }
  return found->second ? vector(id, path, pixelScale, false) : nullptr;
}

// Rasterizes an SVG whose canvas covers the character's bounds. Scales are
// Shapes use 1/16-octave scale steps; text uses fine fixed-point scale keys
// so common display scales such as 0.75 map raster pixels directly to screen.
// Every static text in the SWF asks for FlashType (CSMTextSettings), whose
// stroke modulation fills partly covered pixels in, so small text reads
// heavier and crisper than plain coverage; `flashType` stretches coverage
// to match it.
const Library::Vector *Library::vector(int id, const std::string &path,
                                       float pixelScale, bool flashType,
                                       glm::vec2 phase) {
  auto *ch = character(id);
  if (!ch || !std::isfinite(pixelScale) || pixelScale <= 0)
    return nullptr;
  // Text must rasterize at its actual display scale. Octave quantization
  // turned the field's 0.75 zoom into ~0.738 and then blurred it by resampling.
  int step = flashType ? std::max(1, static_cast<int>(std::lround(pixelScale * 4096)))
                      : static_cast<int>(std::lround(std::log2(pixelScale) * 16));
  glm::ivec2 eighths = glm::round(glm::clamp(phase, 0.0f, 1.0f) * 8.0f);
  phase = glm::vec2(eighths) / 8.0f;
  auto key = std::make_tuple(id, step, eighths.x, eighths.y);
  auto found = vectors.find(key);
  if (found != vectors.end())
    return found->second.texture ? &found->second : nullptr;
  Vector &v = vectors[key];
  NSVGimage *image = nsvgParseFromFile(path.c_str(), "px", 96.0f);
  float width = ch->bounds.y - ch->bounds.x;
  if (!image || image->width <= 0 || image->height <= 0 || width <= 0) {
    if (image)
      nsvgDelete(image);
    return nullptr;
  }
  float displayScale = flashType ? step / 4096.f : std::exp2(step / 16.0f);
  float raster = displayScale * width / image->width;
  raster = std::min(raster, 4096 / std::max(image->width, image->height));
  // Flash never draws a stroke thinner than one pixel.
  for (NSVGshape *s = image->shapes; s; s = s->next)
    if (s->stroke.type != NSVG_PAINT_NONE)
      s->strokeWidth = std::max(s->strokeWidth, 1 / raster);
  // A phase shifts the art right/down, so the raster grows by a pixel.
  int w = std::max(1, static_cast<int>(std::ceil(image->width * raster +
                                                 (eighths.x ? 1 : 0))));
  int h = std::max(1, static_cast<int>(std::ceil(image->height * raster +
                                                 (eighths.y ? 1 : 0))));
  std::vector<unsigned char> pixels(static_cast<size_t>(w) * h * 4, 0);
  if (NSVGrasterizer *rasterizer = nsvgCreateRasterizer()) {
    nsvgRasterize(rasterizer, image, phase.x, phase.y, raster, pixels.data(),
                  w, h, w * 4);
    nsvgDeleteRasterizer(rasterizer);
  }
  v.scale = raster * image->width / width;
  nsvgDelete(image);
  // Calibrated against Flash Player 32. Stage-size text needs a gentler boost;
  // minified field glyphs need stronger modulation to retain thin strokes.
  constexpr int flashTypeBlack = 13;
  const int flashTypeWhite = displayScale < .9f ? 153 : 190;
  if (flashType)
    for (size_t i = 3; i < pixels.size(); i += 4)
      pixels[i] = static_cast<unsigned char>(
          std::clamp((pixels[i] - flashTypeBlack) * 255 /
                         (flashTypeWhite - flashTypeBlack),
                     0, 255));
  // Bottom row first, as the file loader's flipped textures are.
  for (int y = 0; y < h / 2; ++y)
    std::swap_ranges(pixels.begin() + static_cast<size_t>(y) * w * 4,
                     pixels.begin() + static_cast<size_t>(y + 1) * w * 4,
                     pixels.begin() + static_cast<size_t>(h - 1 - y) * w * 4);
  v.texture = std::make_unique<Texture>(w, h, std::move(pixels));
  v.texture->updateSamplerDetails(GL_FilterModes::LINEAR,
                                  GL_WrapModes::CLAMP_TO_EDGE);
  return &v;
}

namespace {
// Flash colour transforms: rgb * mul + add and alpha * mulA + addA, on
// straight alpha. The default 2D shader only multiplies.
const char *ColorVertexSource = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 2) in vec2 aTexCoords;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec2 TexCoords;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoords = aTexCoords;
}
)glsl";

// color, mixColor and useTexture are the uniforms DrawTexturedQuad sets.
const char *ColorFragmentSource = R"glsl(
#version 330 core
in vec2 TexCoords;
out vec4 FragColor;
uniform sampler2D textureSampler;
uniform vec4 color;
uniform vec4 addColor;
uniform bool mixColor;
uniform bool useTexture;
void main() {
    vec4 tex = useTexture ? texture(textureSampler, TexCoords) : vec4(1.0);
    vec4 mul = mixColor ? color : vec4(1.0);
    FragColor = clamp(tex * mul + addColor, 0.0, 1.0);
    if (tex.a <= 0.0)
        FragColor.a = 0.0; // Flash never fills fully transparent pixels
}
)glsl";

std::unique_ptr<Shader> colorShader;
} // namespace

void DrawTexture(Texture &texture, const Affine &world, glm::vec2 origin,
                 glm::vec2 size, const ColorTransform &color) {
  if (color.add == glm::vec4(0.0f)) {
    DrawTexture(texture, world, origin, size, color.mul);
    return;
  }
  if (color.mul.a + color.add.a <= 0.002f)
    return;
  if (!colorShader) {
    colorShader = std::make_unique<Shader>("swfColorShader");
    colorShader->loadFromFile(ColorVertexSource, ColorFragmentSource);
  }
  BeginShaderMode(*colorShader);
  SetUniforms2D();
  colorShader->setUniform("addColor", color.add);
  DrawTexture(texture, world, origin, size, color.mul);
  EndShaderMode();
}

void DrawTexture(Texture &texture, const Affine &world, glm::vec2 origin,
                 glm::vec2 size, glm::vec4 color) {
  if (color.a <= 0.002f)
    return;
  glm::mat4 model(1.0f);
  model[0][0] = world.a;
  model[0][1] = world.b;
  model[1][0] = world.c;
  model[1][1] = world.d;
  model[3][0] = world.tx;
  model[3][1] = world.ty;
  model = glm::scale(glm::translate(model, glm::vec3(origin, 0.0f)),
                     glm::vec3(size, 1.0f));
  DrawTexturedQuad(model, texture, color);
}

Clip::Clip(int id) : id(id) {
  if (auto *ch = library.character(id))
    kind = ch->kind == Kind::Button ? Kind::Button : Kind::Sprite;
  enter(0);
}

std::unique_ptr<Clip> Clip::fromExport(const std::string &name) {
  int id = library.exportId(name);
  return id < 0 ? nullptr : std::make_unique<Clip>(id);
}

int Clip::frameCount() const {
  if (kind == Kind::Button)
    return 1;
  auto *timeline = library.timeline(id);
  return timeline ? std::max<int>(1, static_cast<int>(timeline->frames.size()))
                  : 1;
}

void Clip::gotoAndStop(int target) {
  playing = false;
  enter(std::clamp(target, 0, frameCount() - 1));
}

void Clip::gotoAndPlay(int target) {
  playing = true;
  enter(std::clamp(target, 0, frameCount() - 1));
}

void Clip::gotoAndStop(const std::string &label) {
  if (auto *timeline = library.timeline(id)) {
    auto found = timeline->labels.find(label);
    if (found != timeline->labels.end())
      gotoAndStop(found->second);
  }
}

void Clip::gotoAndPlay(const std::string &label) {
  if (auto *timeline = library.timeline(id)) {
    auto found = timeline->labels.find(label);
    if (found != timeline->labels.end())
      gotoAndPlay(found->second);
  }
}

Clip *Clip::child(const std::string &path) {
  Clip *current = this;
  std::stringstream parts(path);
  std::string part;
  while (current && std::getline(parts, part, '.')) {
    Clip *next = nullptr;
    for (auto &[depth, c] : current->children)
      if (c.clip && c.placement.name == part) {
        next = c.clip.get();
        break;
      }
    current = next;
  }
  return current;
}

Clip *Clip::childAt(int depth) {
  auto found = children.find(depth);
  return found == children.end() ? nullptr : found->second.clip.get();
}

Affine Clip::childWorld(const Affine &base, int depth,
                        const Affine &local) const {
  auto offset = depthOffsets.find(depth);
  return offset == depthOffsets.end()
             ? base * local
             : base * Affine::translate(offset->second) * local;
}

void Clip::setRotation(float degrees) {
  Decomposed p = Decompose(matrix);
  float delta = glm::radians(degrees) - p.skewY;
  p.skewX += delta;
  p.skewY += delta;
  Compose(matrix, p);
  matrixLocked = true;
}

void Clip::setScale(glm::vec2 scale) {
  Decomposed p = Decompose(matrix);
  p.scaleX = scale.x;
  p.scaleY = scale.y;
  Compose(matrix, p);
  matrixLocked = true;
}

void Clip::setPosition(glm::vec2 position) {
  matrix.tx = position.x;
  matrix.ty = position.y;
  matrixLocked = true;
}

glm::vec4 Clip::bounds() const {
  bool any = false;
  glm::vec4 out(0.0f);
  for (auto &[depth, c] : children) {
    if (c.placement.clipDepth > 0 || (c.clip && (c.clip->removed || !c.clip->visible)))
      continue;
    glm::vec4 local;
    if (c.clip) {
      local = c.clip->bounds();
      if (local == glm::vec4(0.0f))
        continue;
    } else if (auto *ch = library.character(c.placement.id))
      local = ch->bounds;
    else
      continue;
    glm::vec4 b = WorldBounds(c.clip ? c.clip->matrix : c.placement.matrix, local);
    out = any ? glm::vec4(std::min(out.x, b.x), std::max(out.y, b.y),
                          std::min(out.z, b.z), std::max(out.w, b.w))
              : b;
    any = true;
  }
  return out;
}

void Clip::setWidth(float width) {
  glm::vec4 local = bounds();
  float unscaled = local.y - local.x;
  if (unscaled <= 0.0f)
    return;
  Decomposed p = Decompose(matrix);
  p.scaleX = std::max(width, 0.0f) / unscaled;
  Compose(matrix, p);
  matrixLocked = true;
}

glm::vec4 TransformBounds(const Affine &m, glm::vec4 bounds) {
  return WorldBounds(m, bounds);
}

void Clip::enter(int target, int depth) {
  frame = target;
  sync();
  runActions(depth);
}

void Clip::sync() {
  std::vector<Placement> placements;
  if (kind == Kind::Button) {
    auto *ch = library.character(id);
    int mask = buttonState == ButtonState::Up ? 1
               : buttonState == ButtonState::Over ? 2
                                                  : 4;
    if (ch)
      for (size_t i = 0; i < ch->records.size(); ++i) {
        auto &r = ch->records[i];
        if (!(r.states & mask))
          continue;
        Placement p;
        p.depth = r.depth;
        p.id = r.id;
        p.serial = static_cast<int>(i) + 1; // Shared records keep their instance
        p.matrix = r.matrix;
        p.color = r.color;
        p.filters = r.filters;
        placements.push_back(p);
      }
  } else if (auto *timeline = library.timeline(id)) {
    if (frame >= 0 && frame < static_cast<int>(timeline->frames.size()))
      placements = timeline->frames[frame];
  }

  std::map<int, Child> next;
  for (auto &p : placements) {
    auto existing = children.find(p.depth);
    if (existing != children.end() &&
        existing->second.placement.serial == p.serial &&
        existing->second.placement.id == p.id) {
      Child c = std::move(existing->second);
      c.placement = p;
      if (c.clip) {
        if (!c.clip->matrixLocked)
          c.clip->matrix = p.matrix;
        c.clip->color = p.color;
      }
      next[p.depth] = std::move(c);
      continue;
    }
    Child c;
    c.placement = p;
    auto *ch = library.character(p.id);
    if (ch && (ch->kind == Kind::Sprite || ch->kind == Kind::Button)) {
      c.clip = std::make_unique<Clip>(p.id);
      c.clip->name = p.name;
      c.clip->matrix = p.matrix;
      c.clip->color = p.color;
    }
    next[p.depth] = std::move(c);
  }
  children = std::move(next);
}

void Clip::runActions(int depth) {
  if (depth > 8 || kind == Kind::Button)
    return;
  auto *timeline = library.timeline(id);
  if (!timeline)
    return;
  auto found = timeline->actions.find(frame);
  if (found == timeline->actions.end())
    return;
  for (const Action &action : found->second) {
    switch (action.op) {
    case Action::Stop:
      playing = false;
      break;
    case Action::Play:
      playing = true;
      break;
    case Action::Remove:
      removed = true;
      break;
    case Action::GotoPlay:
      playing = true;
      enter(std::clamp(action.frame, 0, frameCount() - 1), depth + 1);
      return;
    case Action::GotoStop:
      playing = false;
      enter(std::clamp(action.frame, 0, frameCount() - 1), depth + 1);
      return;
    }
  }
}

void Clip::tick() {
  if (removed)
    return;
  if (born)
    born = false;
  else if (playing && frameCount() > 1)
    enter(frame + 1 >= frameCount() ? 0 : frame + 1);
  for (auto &[depth, c] : children)
    if (c.clip)
      c.clip->tick();
}

void Clip::updateButton(const Affine &world) {
  if (kind != Kind::Button || !resolveButton)
    return;
  // The hit state's records give the button's active area.
  glm::vec4 area(1e9f, -1e9f, 1e9f, -1e9f);
  if (auto *ch = library.character(id))
    for (auto &r : ch->records)
      if (r.states & 8)
        if (auto *target = library.character(r.id)) {
          glm::vec4 b = WorldBounds(world * r.matrix, target->bounds);
          area = {std::min(area.x, b.x), std::max(area.y, b.y),
                  std::min(area.z, b.z), std::max(area.w, b.w)};
        }
  ButtonState state = resolveButton(*this, area);
  if (state != buttonState) {
    buttonState = state;
    sync();
  }
}

void Clip::drawButtons(const Affine &base, bool allStates) {
  if (!visible || removed)
    return;
  std::vector<std::pair<int, glm::vec4>> active; // depth, area
  for (auto &[depth, c] : children) {
    Affine world =
        childWorld(base, depth, c.clip ? c.clip->matrix : c.placement.matrix);
    bool covered = false;
    for (auto &[below, area] : active) {
      if (depth <= below || c.placement.clipDepth > 0)
        continue;
      glm::vec4 b(0.0f);
      if (c.clip)
        b = WorldBounds(world, c.clip->bounds());
      else if (auto *ch = library.character(c.placement.id))
        b = WorldBounds(world, ch->bounds);
      covered = covered || (b.x < area.y && area.x < b.y && b.z < area.w && area.z < b.w);
    }
    if (c.clip && c.clip->kind == Kind::Button) {
      c.clip->updateButton(world);
      if (c.clip->buttonState == ButtonState::Up && !allStates)
        continue;
      if (c.placement.filters.empty())
        drawCharacter(c, world, c.clip->color);
      else
        drawFiltered(c, world, c.clip->color);
      active.push_back({depth, WorldBounds(world, c.clip->bounds())});
    } else if (covered) {
      ColorTransform color = c.clip ? c.clip->color : c.placement.color;
      if (c.placement.filters.empty())
        drawCharacter(c, world, color);
      else
        drawFiltered(c, world, color);
    }
  }
}

void Clip::draw(const Affine &base, const ColorTransform &tint) {
  if (!visible || removed)
    return;
  updateButton(base);

  // Masks (clip depths) are approximated by the scissor box of their bounds.
  GLint savedBox[4] = {0, 0, 0, 0};
  GLboolean savedScissor = GL_FALSE;
  int maskUntil = -1;
  auto restoreMask = [&]() {
    if (maskUntil < 0)
      return;
    if (savedScissor)
      glScissor(savedBox[0], savedBox[1], savedBox[2], savedBox[3]);
    else
      glDisable(GL_SCISSOR_TEST);
    maskUntil = -1;
  };

  for (auto &[depth, c] : children) {
    if (maskUntil >= 0 && depth > maskUntil)
      restoreMask();
    Affine world =
        childWorld(base, depth, c.clip ? c.clip->matrix : c.placement.matrix);
    ColorTransform color = tint * (c.clip ? c.clip->color : c.placement.color);
    if (c.placement.clipDepth > 0) {
      auto *ch = library.character(c.placement.id);
      if (!ch || ch->kind == Kind::Sprite)
        continue;
      restoreMask();
      glm::vec4 b = WorldBounds(world, ch->bounds);
      int fw = 0, fh = 0, ww = 1, wh = 1;
      glfwGetFramebufferSize(extras::ActiveWindow, &fw, &fh);
      glfwGetWindowSize(extras::ActiveWindow, &ww, &wh);
      float sx = static_cast<float>(fw) / std::max(ww, 1);
      float sy = static_cast<float>(fh) / std::max(wh, 1);
      GLint box[4] = {static_cast<GLint>(std::floor(b.x * sx)),
                      static_cast<GLint>(std::floor((wh - b.w) * sy)),
                      static_cast<GLint>(std::ceil((b.y - b.x) * sx)),
                      static_cast<GLint>(std::ceil((b.w - b.z) * sy))};
      savedScissor = glIsEnabled(GL_SCISSOR_TEST);
      glGetIntegerv(GL_SCISSOR_BOX, savedBox);
      if (savedScissor) {
        int x0 = std::max(box[0], savedBox[0]), y0 = std::max(box[1], savedBox[1]);
        int x1 = std::min(box[0] + box[2], savedBox[0] + savedBox[2]);
        int y1 = std::min(box[1] + box[3], savedBox[1] + savedBox[3]);
        box[0] = x0;
        box[1] = y0;
        box[2] = std::max(0, x1 - x0);
        box[3] = std::max(0, y1 - y0);
      }
      glEnable(GL_SCISSOR_TEST);
      glScissor(box[0], box[1], box[2], box[3]);
      maskUntil = c.placement.clipDepth;
      continue;
    }
    if (!c.placement.filters.empty())
      drawFiltered(c, world, color);
    else
      drawCharacter(c, world, color);
  }
  restoreMask();
}

// Screen pixels per SWF pixel along the larger axis of `world`.
static float PixelScale(const Affine &world) {
  return std::max(std::sqrt(world.a * world.a + world.b * world.b),
                  std::sqrt(world.c * world.c + world.d * world.d));
}

// Draws vector art rasterized at this scale. Unrotated art starts on a whole
// pixel so its raster maps 1:1 onto the screen instead of being resampled.
static void DrawVector(const Library::Vector &v, Affine world, glm::vec2 origin,
                       const ColorTransform &color) {
  if (std::abs(world.b) < 1e-4f && std::abs(world.c) < 1e-4f) {
    glm::vec2 at = world.apply(origin);
    world.tx += std::round(at.x) - at.x;
    world.ty += std::round(at.y) - at.y;
  }
  DrawTexture(*v.texture, world, origin,
              {v.texture->width / v.scale, v.texture->height / v.scale},
              color);
}

// Draws one of library.vectorShapes at its exact sub-pixel position: the
// raster carries the fractional offset instead of the art being rounded to
// a whole pixel, so shapes that must line up (a wipe over its text) do.
// Axes that are squashed are resampled anyway and keep their position.
static bool DrawShapeVector(int id, Affine world, glm::vec2 origin,
                            const ColorTransform &color) {
  if (auto *b = library.bitmapFill(id)) {
    DrawTexture(*b->texture, world * b->matrix, {0, 0}, b->size, color);
    return true;
  }
  float scale = PixelScale(world);
  glm::vec2 at = world.apply(origin), phase(0.0f);
  if (std::abs(world.b) < 1e-4f && std::abs(world.c) < 1e-4f) {
    if (std::abs(std::abs(world.a) - scale) < 1e-3f * scale)
      phase.x = at.x - std::floor(at.x);
    if (std::abs(std::abs(world.d) - scale) < 1e-3f * scale)
      phase.y = at.y - std::floor(at.y);
  }
  auto *v = library.shapeVector(id, scale, phase);
  if (!v)
    return false;
  // Unsquashed axes map raster pixels 1:1 onto the screen, so the phase
  // (in screen pixels) moves the art back onto its exact position.
  world.tx -= phase.x;
  world.ty -= phase.y;
  DrawTexture(*v->texture, world, origin,
              {v->texture->width / v->scale, v->texture->height / v->scale},
              color);
  return true;
}

void DrawStaticTexts(int sprite, const Affine &base) {
  auto *timeline = library.timeline(sprite);
  if (!timeline || timeline->frames.empty())
    return;
  for (const Placement &p : timeline->frames[0]) {
    auto *ch = library.character(p.id);
    if (!ch || ch->kind != Kind::Text)
      continue;
    Affine world = base * p.matrix;
    if (auto *v = library.textVector(p.id, PixelScale(world)))
      DrawVector(*v, world, {ch->bounds.x, ch->bounds.z}, p.color);
  }
}

// A transform whose alpha is scaled by `alpha` (morph cross-fades).
static ColorTransform Fade(ColorTransform color, float alpha) {
  color.mul.a *= alpha;
  color.add.a *= alpha;
  return color;
}

void Clip::drawCharacter(Child &c, const Affine &world,
                         const ColorTransform &color) {
  if (c.clip) {
    c.clip->inheritedTextPixelOffset = inheritedTextPixelOffset + textPixelOffset;
    c.clip->draw(world, color);
    return;
  }
  auto *ch = library.character(c.placement.id);
  if (!ch)
    return;
  const float zoom = ch->zoom;
  switch (ch->kind) {
  case Kind::Shape:
    if (DrawShapeVector(c.placement.id, world, {ch->bounds.x, ch->bounds.z},
                        color))
      break;
    if (auto *v = library.hairlineVector(c.placement.id, PixelScale(world)))
      DrawVector(*v, world, {ch->bounds.x, ch->bounds.z}, color);
    else if (Texture *t = library.shape(c.placement.id))
      DrawTexture(*t, world,
                  {std::floor(ch->bounds.x * zoom) / zoom,
                   std::floor(ch->bounds.z * zoom) / zoom},
                  {t->width / zoom, t->height / zoom}, color);
    break;
  case Kind::Morph: {
    // Morphs cross-fade between their start and end artwork. When both cover
    // the same area the start stays opaque under the fading-in end, giving
    // start * (1 - r) + end * r; fading both would let the background show
    // through mid-tween and read as a flicker.
    float r = std::clamp(c.placement.ratio, 0.0f, 1.0f);
    bool sameArea = ch->bounds == ch->endBounds;
    if (r < 1.0f)
      if (Texture *t = library.morph(c.placement.id, false))
        DrawTexture(*t, world,
                    {std::floor(ch->bounds.x * zoom) / zoom,
                     std::floor(ch->bounds.z * zoom) / zoom},
                    {t->width / zoom, t->height / zoom},
                    sameArea ? color : Fade(color, 1 - r));
    if (r > 0.0f)
      if (Texture *t = library.morph(c.placement.id, true))
        DrawTexture(*t, world,
                    {std::floor(ch->endBounds.x * zoom) / zoom,
                     std::floor(ch->endBounds.z * zoom) / zoom},
                    {t->width / zoom, t->height / zoom},
                    Fade(color, r));
    break;
  }
  case Kind::Text: {
    Affine textWorld = world;
    textWorld.tx += textPixelOffset.x + inheritedTextPixelOffset.x;
    textWorld.ty += textPixelOffset.y + inheritedTextPixelOffset.y;
    if (auto *v = library.textVector(c.placement.id, PixelScale(textWorld)))
      DrawVector(*v, textWorld, {ch->bounds.x, ch->bounds.z}, color);
    else if (Texture *t = library.text(c.placement.id))
      DrawTexture(*t, textWorld, {ch->bounds.x, ch->bounds.z},
                  {ch->bounds.y - ch->bounds.x, ch->bounds.w - ch->bounds.z},
                  color);
    break;
  }
  case Kind::Edit:
    if (ch->edit.border) {
      // Flash fills a bordered field white and outlines it with a one-pixel
      // black line along its bounds, snapped to whole pixels.
      glm::vec4 b = WorldBounds(world, ch->bounds);
      glm::vec2 lo = glm::round(glm::vec2(b.x, b.z)),
                hi = glm::round(glm::vec2(b.y, b.w));
      glm::vec4 white = glm::vec4(1.0f) * color.mul + color.add,
                black = glm::vec4(0, 0, 0, 1) * color.mul + color.add;
      DrawRectangle(Rect(lo, hi - lo), white);
      DrawRectangle(Rect(lo, {hi.x - lo.x, 1.0f}), black);
      DrawRectangle(Rect({lo.x, hi.y - 1}, {hi.x - lo.x, 1.0f}), black);
      DrawRectangle(Rect(lo, {1.0f, hi.y - lo.y}), black);
      DrawRectangle(Rect({hi.x - 1, lo.y}, {1.0f, hi.y - lo.y}), black);
    }
    if (drawEditText) {
      auto found = variables.find(ch->edit.variable);
      drawEditText(ch->edit,
                   found != variables.end() ? found->second : ch->edit.text,
                   world, ch->bounds, color);
    }
    break;
  default:
    break;
  }
}


float filterScale = 1;

namespace {
const char *FilterVertexSource = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 2) in vec2 aTexCoords;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec2 TexCoords;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoords = aTexCoords;
}
)glsl";

// One box-blur pass of the source alpha along texelStep (Flash blurs are
// boxes of blurX by blurY pixels). tint = 0 writes the blurred alpha; tint = 1
// writes the filter colour at alpha * strength, ready to composite.
const char *FilterFragmentSource = R"glsl(
#version 330 core
in vec2 TexCoords;
out vec4 FragColor;
uniform sampler2D textureSampler;
uniform vec2 texelStep;
uniform float radius;
uniform int tint;
uniform vec4 color;
uniform float strength;
void main() {
    float sum = 0.0;
    float weight = 0.0;
    for (int i = -96; i <= 96; ++i) {
        float d = abs(float(i));
        if (d > radius + 1.0)
            continue;
        float k = clamp(radius + 0.5 - d, 0.0, 1.0);
        sum += texture(textureSampler, TexCoords + texelStep * float(i)).a * k;
        weight += k;
    }
    float a = weight > 0.0 ? sum / weight : texture(textureSampler, TexCoords).a;
    if (tint == 1)
        FragColor = vec4(color.rgb, clamp(a * strength, 0.0, 1.0) * color.a);
    else
        FragColor = vec4(1.0, 1.0, 1.0, a);
}
)glsl";

struct FilterTargets {
  std::unique_ptr<FrameBuffer> source, blurred;
  std::unique_ptr<Shader> shader;
  int width = 0, height = 0;
} filterTargets;
bool inFilterPass = false;

bool PrepareFilterTargets() {
  int fw = 0, fh = 0;
  glfwGetFramebufferSize(extras::ActiveWindow, &fw, &fh);
  if (fw <= 0 || fh <= 0)
    return false;
  auto &t = filterTargets;
  if (!t.shader) {
    t.shader = std::make_unique<Shader>("swfFilterShader");
    t.shader->loadFromFile(FilterVertexSource, FilterFragmentSource);
  }
  if (!t.source || t.width != fw || t.height != fh) {
    auto make = [&]() {
      return std::make_unique<FrameBuffer>(
          fw, fh, GL_FilterModes::LINEAR, GL_WrapModes::CLAMP_TO_EDGE,
          GL_InternalFormats::RGBA8, GL_ExternalFormats::RGBA,
          GL_DataFormats::UNSIGNED_BYTE, 4);
    };
    t.source = make();
    t.blurred = make();
    t.width = fw;
    t.height = fh;
  }
  return true;
}

void ClearTarget() {
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);
}

// Draws `texture` over the whole window through the filter shader.
void FilterPass(Texture &texture, glm::vec2 step, float radius, bool tint,
                glm::vec4 color, float strength, glm::vec2 offset) {
  Shader &shader = *filterTargets.shader;
  BeginShaderMode(shader);
  SetUniforms2D();
  shader.setUniform("texelStep", step);
  shader.setUniform("radius", radius);
  shader.setUniform("tint", tint ? 1 : 0);
  shader.setUniform("strength", strength);
  glm::vec2 size = GetCurrentWindowSize();
  glm::mat4 model = glm::scale(
      glm::translate(glm::mat4(1.0f), glm::vec3(offset, 0.0f)),
      glm::vec3(size, 1.0f));
  DrawTexturedQuad(model, texture, color);
  EndShaderMode();
}
} // namespace

void ReleaseFilterTargets() {
  filterTargets.source.reset();
  filterTargets.blurred.reset();
  filterTargets.shader.reset();
  filterTargets.width = filterTargets.height = 0;
  colorShader.reset();
}

void Clip::drawFiltered(Child &c, const Affine &world,
                        const ColorTransform &tint) {
  bool usable = false, knockout = false;
  for (const Filter &f : c.placement.filters)
    if (f.type != Filter::Type::Other && !f.inner) {
      usable = true;
      knockout = knockout || f.knockout;
    }
  if (inFilterPass || !usable || !PrepareFilterTargets()) {
    drawCharacter(c, world, tint);
    return;
  }
  auto &t = filterTargets;
  float pixels = static_cast<float>(t.width) /
                 std::max(1.0f, GetCurrentWindowSize().x);

  // The filtered clip alone, keeping straight alpha for the blur.
  t.source->startDrawing();
  ClearTarget();
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                      GL_ONE_MINUS_SRC_ALPHA);
  inFilterPass = true;
  drawCharacter(c, world, tint);
  inFilterPass = false;
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  t.source->stopDrawing();

  for (const Filter &f : c.placement.filters) {
    if (f.type == Filter::Type::Other || f.inner)
      continue;
    float rx = f.blurX * .5f * filterScale * pixels;
    float ry = f.blurY * .5f * filterScale * pixels;
    // Horizontal pass into the scratch target, unblended.
    t.blurred->startDrawing();
    ClearTarget();
    glDisable(GL_BLEND);
    FilterPass(*t.source->texture, {1.0f / t.width, 0.0f}, rx, false,
               glm::vec4(1.0f), 1.0f, {0.0f, 0.0f});
    glEnable(GL_BLEND);
    t.blurred->stopDrawing();
    // Vertical pass, tinted, onto the screen behind the clip.
    glm::vec2 offset(0.0f);
    if (f.type == Filter::Type::Shadow)
      offset = glm::vec2(std::cos(f.angle), std::sin(f.angle)) * f.distance *
               filterScale;
    FilterPass(*t.blurred->texture, {0.0f, 1.0f / t.height}, ry, true,
               f.color * glm::vec4(1, 1, 1, tint.mul.a), f.strength, offset);
  }
  if (!knockout)
    drawCharacter(c, world, tint);
}

} // namespace swf
