#pragma once
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

/// Ashfall's JSON data files in the editor (Window > Ashfall Data, and the Inspector's config view of a script's
/// .json AssetPath field). Editor only.
///
/// - Documents are parsed by a small reader that remembers where every value sits in the file, so Save only
///   rewrites what changed: an edited number replaces just that number, an array with an element added or removed
///   is re-written in place, key order and the rest of the hand formatting stay as they were.
/// - Schemas (optional) label and constrain fields: a sidecar <name>.schema.json, a folder-wide _schema.json, or a
///   "_meta" block in the file itself (docs/ASHFALL_DATA.md). Fields without one still show, with default widgets.
/// - While playing, every edit is sent to the scripts (HotReloadManager::notifyDataChanged); nothing is written to
///   disk until Save. After Play the edits stay pending with Keep changes (save) / Discard.
namespace AshfallData {

struct JNode {
    enum class Kind { Null, Bool, Number, String, Array, Object };
    Kind kind = Kind::Null;
    std::string key;                 // member name (inside an object)
    bool b = false;
    double num = 0.0;
    bool isInt = false;              // the number was written without '.' / exponent
    std::string str;
    std::vector<JNode> kids;         // array elements / object members, in file order
    size_t begin = std::string::npos, end = std::string::npos;   // the value's span in the file text
    bool valueDirty = false;         // scalar changed
    bool structDirty = false;        // container gained / lost / reordered children: re-written as a whole

    bool isContainer() const { return kind == Kind::Array || kind == Kind::Object; }
    JNode* member(const std::string& k);
    const JNode* member(const std::string& k) const;
};

/// Text <-> tree. Parse returns false (and an error with the line) on bad JSON.
bool Parse(const std::string& text, JNode& out, std::string& error);
std::string Write(const JNode& node, int indent = 0);              // fresh formatting (compact where short)
std::string Rebuild(const std::string& original, const JNode& root); // original text with only the edits applied

struct FieldMeta {
    std::string label, tooltip, unit;
    float min = 0.f, max = 0.f, step = 0.f;   // min < max: a slider
    std::vector<std::string> options;         // string or number choices
    int numberKind = 0;                       // 0 = as written, 1 = int, 2 = float
    bool readOnly = false;
};

struct Doc {
    std::string path;          // absolute
    std::string assetPath;     // "Assets/Enemies/goblin.json"
    std::string text;          // what is on disk (last load / save)
    JNode root;
    std::string error;         // parse error: shown instead of the tree
    bool dirty = false;
    bool editedInPlay = false; // changed while playing, not saved yet
    bool readOnly = false;     // VFX recipes: generated files
    std::string readOnlyNote;
    std::map<std::string, FieldMeta> schema;   // "health.max", "attacks[].damage"
    std::string schemaSource;
    long long diskTime = 0;    // file time at the last load / save: a clean doc re-reads a newer file

    std::string currentText() const { return error.empty() ? Rebuild(text, root) : text; }
};

// Open (or the already open) document. `assetPath` relative to the project ("Assets/...") or absolute.
Doc* Open(const std::string& assetPath);
const std::vector<std::unique_ptr<Doc>>& OpenDocs();
bool Save(Doc& doc, std::string& message);
void Revert(Doc& doc);
// Copy of the current text (edits included) as <folder>/<newStem>.json; opens and returns it.
Doc* Duplicate(Doc& doc, const std::string& newStem, std::string& message);

/// "goblin.json" + filter "Assets/Enemies|Assets/EnemyConfigs;.json" -> "Assets/Enemies/goblin.json" (the first that
/// exists); a value with a '/' is taken relative to the project, or to Assets/. "" when nothing matches.
std::string ResolveAsset(const std::string& value, const char* filter);

/// The tree editor. overrides (optional): a JSON object of per-instance overrides - edits go there instead of into
/// the file, overridden values draw bold with a reset button. Returns true when the doc or *overrides changed.
bool DrawTree(Doc& doc, std::string* overrides, bool readOnly = false);
/// Save / Revert (+ Keep changes / Discard after Play) row for one document.
void DrawToolbar(Doc& doc, bool showOpenButton);

/// Ask the Ashfall Data window to show this file (Inspector's "Open in Ashfall Data").
void RequestFocus(const std::string& assetPath);
std::string TakeFocusRequest();

bool IsPlaying();
/// Sends the document's current text to the scripts (while playing).
void NotifyLive(Doc& doc);

} // namespace AshfallData
