#include "Globals.h"
#include "AshfallData.h"
#include "Application.h"
#include "RuntimeCore.h"
#include "SceneManager.h"
#include "HotReloadManager.h"
#include "ModuleFileSystem.h"
#include "EditorColors.h"
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace AshfallData {

JNode* JNode::member(const std::string& k){
    for (JNode& c : kids) if (c.key == k) return &c;
    return nullptr;
}
const JNode* JNode::member(const std::string& k) const {
    for (const JNode& c : kids) if (c.key == k) return &c;
    return nullptr;
}

// ================================================================ parse

namespace {

struct Parser {
    const std::string& s;
    size_t i = 0;
    std::string err;

    explicit Parser(const std::string& text) : s(text){}

    int line() const { return 1 + (int)std::count(s.begin(), s.begin() + (std::min)(i, s.size()), '\n'); }
    bool fail(const char* what){ if (err.empty()) err = std::string(what) + " at line " + std::to_string(line()); return false; }
    void ws(){ while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i; }

    static void appendUtf8(std::string& out, unsigned cp){
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800){ out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    }

    bool str(std::string& out){
        if (i >= s.size() || s[i] != '"') return fail("expected a string");
        ++i;
        out.clear();
        while (i < s.size() && s[i] != '"'){
            char c = s[i++];
            if (c != '\\'){ out += c; continue; }
            if (i >= s.size()) return fail("bad escape");
            const char e = s[i++];
            switch (e){
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u':{
                    if (i + 4 > s.size()) return fail("bad \\u escape");
                    appendUtf8(out, (unsigned)std::stoul(s.substr(i, 4), nullptr, 16));
                    i += 4;
                    break;
                }
                default: return fail("bad escape");
            }
        }
        if (i >= s.size()) return fail("unterminated string");
        ++i;
        return true;
    }

    bool value(JNode& n){
        ws();
        if (i >= s.size()) return fail("unexpected end");
        n.begin = i;
        const char c = s[i];
        if (c == '{'){
            n.kind = JNode::Kind::Object;
            ++i; ws();
            if (i < s.size() && s[i] == '}'){ ++i; n.end = i; return true; }
            for (;;){
                ws();
                JNode kid;
                if (!str(kid.key)) return false;
                ws();
                if (i >= s.size() || s[i] != ':') return fail("expected ':'");
                ++i;
                if (!value(kid)) return false;
                n.kids.push_back(std::move(kid));
                ws();
                if (i < s.size() && s[i] == ','){ ++i; continue; }
                if (i < s.size() && s[i] == '}'){ ++i; break; }
                return fail("expected ',' or '}'");
            }
        }
        else if (c == '['){
            n.kind = JNode::Kind::Array;
            ++i; ws();
            if (i < s.size() && s[i] == ']'){ ++i; n.end = i; return true; }
            for (;;){
                JNode kid;
                if (!value(kid)) return false;
                n.kids.push_back(std::move(kid));
                ws();
                if (i < s.size() && s[i] == ','){ ++i; continue; }
                if (i < s.size() && s[i] == ']'){ ++i; break; }
                return fail("expected ',' or ']'");
            }
        }
        else if (c == '"'){ n.kind = JNode::Kind::String; if (!str(n.str)) return false; }
        else if (s.compare(i, 4, "true") == 0){ n.kind = JNode::Kind::Bool; n.b = true; i += 4; }
        else if (s.compare(i, 5, "false") == 0){ n.kind = JNode::Kind::Bool; n.b = false; i += 5; }
        else if (s.compare(i, 4, "null") == 0){ n.kind = JNode::Kind::Null; i += 4; }
        else if (c == '-' || (c >= '0' && c <= '9')){
            n.kind = JNode::Kind::Number;
            const size_t start = i;
            if (s[i] == '-') ++i;
            bool frac = false;
            while (i < s.size() && (isdigit((unsigned char)s[i]) || s[i] == '.' || s[i] == 'e' || s[i] == 'E' ||
                                    ((s[i] == '+' || s[i] == '-') && (s[i - 1] == 'e' || s[i - 1] == 'E')))){
                if (s[i] == '.' || s[i] == 'e' || s[i] == 'E') frac = true;
                ++i;
            }
            n.num = std::strtod(s.c_str() + start, nullptr);
            n.isInt = !frac;
        }
        else return fail("unexpected character");
        n.end = i;
        return true;
    }
};

std::string escape(const std::string& in){
    std::string out = "\"";
    for (unsigned char c : in){
        switch (c){
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20){ char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); out += b; }
                else out += (char)c;
        }
    }
    return out + "\"";
}

std::string number(const JNode& n){
    char b[64];
    if (n.isInt && std::fabs(n.num) < 1e15 && n.num == std::floor(n.num)){
        snprintf(b, sizeof(b), "%lld", (long long)n.num);
        return b;
    }
    snprintf(b, sizeof(b), "%.6g", n.num);
    std::string s = b;
    if (s.find_first_of(".eEn") == std::string::npos) s += ".0";
    return s;
}

std::string scalar(const JNode& n){
    switch (n.kind){
        case JNode::Kind::Bool: return n.b ? "true" : "false";
        case JNode::Kind::Number: return number(n);
        case JNode::Kind::String: return escape(n.str);
        default: return "null";
    }
}

constexpr size_t kCompactWidth = 100;

// One line ("{ \"a\": 1, \"b\": [1, 2] }") when everything below is short; false = too long or too deep.
bool compact(const JNode& n, std::string& out, int depth = 0){
    if (!n.isContainer()){ out = scalar(n); return true; }
    if (depth > 2) return false;
    const bool obj = n.kind == JNode::Kind::Object;
    if (n.kids.empty()){ out = obj ? "{}" : "[]"; return true; }
    std::string s = obj ? "{ " : "[";
    for (size_t k = 0; k < n.kids.size(); ++k){
        std::string kid;
        if (!compact(n.kids[k], kid, depth + 1)) return false;
        if (k) s += ", ";
        if (obj) s += escape(n.kids[k].key) + ": ";
        s += kid;
        if (s.size() > kCompactWidth) return false;
    }
    s += obj ? " }" : "]";
    out = s;
    return true;
}

} // namespace

bool Parse(const std::string& text, JNode& out, std::string& error){
    Parser p(text);
    out = JNode();
    if (!p.value(out)){ error = p.err; return false; }
    p.ws();
    if (p.i != text.size()){ error = "extra text after the JSON at line " + std::to_string(p.line()); return false; }
    error.clear();
    return true;
}

std::string Write(const JNode& n, int indent){
    std::string one;
    if (compact(n, one)) return one;
    const bool obj = n.kind == JNode::Kind::Object;
    std::string s = obj ? "{\n" : "[\n";
    const std::string pad(indent + 2, ' ');
    for (size_t k = 0; k < n.kids.size(); ++k){
        s += pad;
        if (obj) s += escape(n.kids[k].key) + ": ";
        s += Write(n.kids[k], indent + 2);
        if (k + 1 < n.kids.size()) s += ",";
        s += "\n";
    }
    s += std::string(indent, ' ') + (obj ? "}" : "]");
    return s;
}

namespace {
int indentAt(const std::string& text, size_t pos){
    size_t lineStart = text.rfind('\n', pos == 0 ? 0 : pos - 1);
    lineStart = lineStart == std::string::npos ? 0 : lineStart + 1;
    int n = 0;
    while (lineStart + n < text.size() && text[lineStart + n] == ' ') ++n;
    return n;
}

bool anyDirty(const JNode& n){
    if (n.valueDirty || n.structDirty || n.begin == std::string::npos) return true;
    for (const JNode& k : n.kids) if (anyDirty(k)) return true;
    return false;
}

// One line, however long (a new element next to one-line siblings).
std::string inlineText(const JNode& n){
    if (!n.isContainer()) return scalar(n);
    const bool obj = n.kind == JNode::Kind::Object;
    if (n.kids.empty()) return obj ? "{}" : "[]";
    std::string s = obj ? "{ " : "[";
    for (size_t k = 0; k < n.kids.size(); ++k){
        if (k) s += ", ";
        if (obj) s += escape(n.kids[k].key) + ": ";
        s += inlineText(n.kids[k]);
    }
    return s + (obj ? " }" : "]");
}

// The node's text with the edits applied: untouched parts are copied from the original, a changed scalar is
// re-written, a container that gained / lost children is laid out again around its untouched children's text.
std::string rebuilt(const std::string& orig, const JNode& n, int indent){
    if (n.begin == std::string::npos) return Write(n, indent);
    if (!anyDirty(n)) return orig.substr(n.begin, n.end - n.begin);
    if (!n.isContainer()) return scalar(n);
    const bool obj = n.kind == JNode::Kind::Object;
    if (!n.structDirty){
        std::string out;
        size_t pos = n.begin;
        for (const JNode& k : n.kids){
            if (!anyDirty(k)) continue;
            out += orig.substr(pos, k.begin - pos);
            out += rebuilt(orig, k, indentAt(orig, k.begin));
            pos = k.end;
        }
        return out + orig.substr(pos, n.end - pos);
    }
    if (n.kids.empty()) return obj ? "{}" : "[]";
    // Lay it out the way it was: one line if it was one line, else one child per line. New children follow the
    // style of the existing ones (one-line siblings -> one line).
    const bool wasInline = orig.substr(n.begin, n.end - n.begin).find('\n') == std::string::npos;
    bool kidsInline = true;
    for (const JNode& k : n.kids)
        if (k.begin != std::string::npos){ kidsInline = orig.substr(k.begin, k.end - k.begin).find('\n') == std::string::npos; break; }
    auto kidText = [&](const JNode& k, int kidIndent){
        if (k.begin != std::string::npos) return rebuilt(orig, k, kidIndent);
        return kidsInline ? inlineText(k) : Write(k, kidIndent);
    };
    if (wasInline){
        std::string s = obj ? "{ " : "[";
        for (size_t i = 0; i < n.kids.size(); ++i){
            if (i) s += ", ";
            if (obj) s += escape(n.kids[i].key) + ": ";
            s += kidText(n.kids[i], indent);
        }
        return s + (obj ? " }" : "]");
    }
    std::string s = obj ? "{\n" : "[\n";
    const std::string pad(indent + 2, ' ');
    for (size_t i = 0; i < n.kids.size(); ++i){
        s += pad;
        if (obj) s += escape(n.kids[i].key) + ": ";
        s += kidText(n.kids[i], indent + 2);
        if (i + 1 < n.kids.size()) s += ",";
        s += "\n";
    }
    return s + std::string(indent, ' ') + (obj ? "}" : "]");
}
} // namespace

std::string Rebuild(const std::string& original, const JNode& root){
    if (root.begin == std::string::npos) return Write(root) + "\n";
    if (!anyDirty(root)) return original;
    std::string out = original.substr(0, root.begin) + rebuilt(original, root, indentAt(original, root.begin)) + original.substr(root.end);
    if (original.find("\r\n") != std::string::npos){
        // CRLF file: new line breaks match it.
        std::string t;
        t.reserve(out.size() + 64);
        for (size_t i = 0; i < out.size(); ++i){
            if (out[i] == '\n' && (i == 0 || out[i - 1] != '\r')) t += '\r';
            t += out[i];
        }
        out.swap(t);
    }
    return out;
}

// ================================================================ documents

namespace {

std::vector<std::unique_ptr<Doc>> g_docs;
std::string g_focus;

fs::path ProjectRoot(){
    // GetAssetsPath() is "<exe dir>/Assets/": the project root is its parent.
    fs::path assets = fs::path(app->getFileSystem()->GetAssetsPath());
    if (!assets.has_filename()) assets = assets.parent_path();
    return assets.parent_path();
}

std::string ReadFile(const fs::path& p, bool& ok){
    std::ifstream f(p, std::ios::binary);
    ok = (bool)f;
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string Normalize(const std::string& assetPath){
    fs::path p(assetPath);
    std::error_code ec;
    if (p.is_absolute()){
        const fs::path rel = fs::relative(p, ProjectRoot(), ec);
        if (!ec && !rel.empty() && rel.generic_string().rfind("..", 0) != 0) return rel.generic_string();
        return p.generic_string();
    }
    return p.generic_string();
}

void ReadSchemaInto(const JNode& fields, Doc& d){
    for (const JNode& f : fields.kids){
        if (f.kind != JNode::Kind::Object) continue;
        FieldMeta m;
        if (const JNode* v = f.member("label"); v && v->kind == JNode::Kind::String) m.label = v->str;
        if (const JNode* v = f.member("tooltip"); v && v->kind == JNode::Kind::String) m.tooltip = v->str;
        if (const JNode* v = f.member("unit"); v && v->kind == JNode::Kind::String) m.unit = v->str;
        if (const JNode* v = f.member("min"); v && v->kind == JNode::Kind::Number) m.min = (float)v->num;
        if (const JNode* v = f.member("max"); v && v->kind == JNode::Kind::Number) m.max = (float)v->num;
        if (const JNode* v = f.member("step"); v && v->kind == JNode::Kind::Number) m.step = (float)v->num;
        if (const JNode* v = f.member("readOnly"); v && v->kind == JNode::Kind::Bool) m.readOnly = v->b;
        if (const JNode* v = f.member("type"); v && v->kind == JNode::Kind::String)
            m.numberKind = v->str == "int" ? 1 : v->str == "float" ? 2 : 0;
        if (const JNode* v = f.member("options"); v && v->kind == JNode::Kind::Array)
            for (const JNode& o : v->kids) m.options.push_back(o.kind == JNode::Kind::String ? o.str : number(o));
        d.schema[f.key] = m;
    }
}

void LoadSchema(Doc& d){
    d.schema.clear();
    d.schemaSource.clear();
    const fs::path file(d.path);
    std::string stem = file.filename().string();
    stem = stem.substr(0, stem.find('.'));
    for (const fs::path& p : { file.parent_path() / "_schema.json", file.parent_path() / (stem + ".schema.json") }){
        bool ok = false;
        const std::string text = ReadFile(p, ok);
        JNode root;
        std::string err;
        if (!ok || !Parse(text, root, err)) continue;
        if (const JNode* f = root.member("fields")){
            ReadSchemaInto(*f, d);
            d.schemaSource += (d.schemaSource.empty() ? "" : " + ") + p.filename().string();
        }
    }
    if (const JNode* meta = d.root.member("_meta"))
        if (const JNode* f = meta->member("fields")){
            ReadSchemaInto(*f, d);
            d.schemaSource += std::string(d.schemaSource.empty() ? "" : " + ") + "_meta";
        }
}

long long FileTime(const std::string& path){
    std::error_code ec;
    const auto t = fs::last_write_time(path, ec);
    return ec ? 0 : (long long)t.time_since_epoch().count();
}

void Load(Doc& d){
    bool ok = false;
    d.diskTime = FileTime(d.path);
    d.text = ReadFile(d.path, ok);
    d.dirty = false;
    d.editedInPlay = false;
    if (!ok){ d.error = "can't read " + d.path; d.root = JNode(); return; }
    if (!Parse(d.text, d.root, d.error)) d.root = JNode();
    LoadSchema(d);
}

} // namespace

Doc* Open(const std::string& assetPath){
    if (assetPath.empty() || !app || !app->getFileSystem()) return nullptr;
    const std::string rel = Normalize(assetPath);
    for (auto& d : g_docs)
        if (d->assetPath == rel){
            // Changed on disk (a generator script, a text editor) and nothing unsaved here: take the new file.
            if (!d->dirty && FileTime(d->path) != d->diskTime) Load(*d);
            return d.get();
        }
    fs::path abs = fs::path(rel).is_absolute() ? fs::path(rel) : ProjectRoot() / rel;
    std::error_code ec;
    if (!fs::exists(abs, ec)) return nullptr;
    auto d = std::make_unique<Doc>();
    d->assetPath = rel;
    d->path = abs.string();
    d->readOnly = rel.find("/VFX/recipes/") != std::string::npos;
    if (d->readOnly)
        d->readOnlyNote = "Generated by Assets/VFX/_tools/make_recipes.py - edit its TABLE and run it (R reloads in VFX_Test).";
    Load(*d);
    g_docs.push_back(std::move(d));
    return g_docs.back().get();
}

const std::vector<std::unique_ptr<Doc>>& OpenDocs(){ return g_docs; }

bool Save(Doc& d, std::string& message){
    if (d.readOnly){ message = "read-only"; return false; }
    if (!d.error.empty()){ message = d.error; return false; }
    const std::string text = d.currentText();
    std::ofstream f(d.path, std::ios::binary | std::ios::trunc);
    if (!f){ message = "can't write " + d.path; return false; }
    f << text;
    f.close();
    d.text = text;
    d.diskTime = FileTime(d.path);
    std::string err;
    Parse(d.text, d.root, err);   // fresh spans
    d.dirty = false;
    d.editedInPlay = false;
    LoadSchema(d);
    message = "saved " + d.assetPath;
    return true;
}

void Revert(Doc& d){
    Load(d);
    NotifyLive(d);   // the running game goes back too
}

Doc* Duplicate(Doc& d, const std::string& newStem, std::string& message){
    const fs::path src(d.path);
    std::string name = src.filename().string();
    const std::string ext = name.substr(name.find('.'));   // ".json" / ".encounters.json"
    const fs::path dst = src.parent_path() / (newStem + ext);
    std::error_code ec;
    if (newStem.empty() || fs::exists(dst, ec)){ message = "a file named " + dst.filename().string() + " already exists"; return nullptr; }
    std::ofstream f(dst, std::ios::binary);
    if (!f){ message = "can't write " + dst.string(); return nullptr; }
    f << d.currentText();
    f.close();
    message = "duplicated as " + dst.filename().string();
    return Open(dst.string());
}

std::string ResolveAsset(const std::string& value, const char* filter){
    if (value.empty() || !app || !app->getFileSystem()) return "";
    const fs::path root = ProjectRoot();
    std::error_code ec;
    auto exists = [&](const std::string& rel){ return fs::exists(root / rel, ec); };
    if (value.find('/') != std::string::npos || value.find('\\') != std::string::npos){
        if (exists(value)) return fs::path(value).generic_string();
        if (exists("Assets/" + value)) return "Assets/" + fs::path(value).generic_string();
        return "";
    }
    std::string folders = filter ? filter : "";
    folders = folders.substr(0, folders.find(';'));
    std::stringstream ss(folders);
    std::string folder;
    while (std::getline(ss, folder, '|'))
        if (!folder.empty() && exists(folder + "/" + value)) return folder + "/" + value;
    return exists("Assets/" + value) ? "Assets/" + value : "";
}

void RequestFocus(const std::string& assetPath){ g_focus = assetPath; }
std::string TakeFocusRequest(){ std::string s = g_focus; g_focus.clear(); return s; }

bool IsPlaying(){
    RuntimeCore* rc = app ? app->getRuntimeCore() : nullptr;
    SceneManager* sm = rc ? rc->getSceneManager() : nullptr;
    return sm && sm->getState() != SceneManager::PlayState::Stopped;
}

void NotifyLive(Doc& d){
    if (!IsPlaying() || !d.error.empty()) return;
    RuntimeCore* rc = app ? app->getRuntimeCore() : nullptr;
    if (HotReloadManager* hr = rc ? rc->getHotReloadManager() : nullptr) hr->notifyDataChanged(d.assetPath, d.currentText());
}

// ================================================================ tree editor

namespace {

struct Step { std::string key; int index = -1; };

struct Ctx {
    Doc& doc;
    JNode* ov;              // overrides root (instance mode) or null (file mode)
    bool readOnly;
    bool changed = false;   // file changed
    bool ovChanged = false; // overrides changed
    std::vector<Step> steps;
};

JNode CopyNoSpans(const JNode& n){
    JNode c = n;
    std::vector<JNode*> stack{ &c };
    while (!stack.empty()){
        JNode* x = stack.back(); stack.pop_back();
        x->begin = x->end = std::string::npos;
        x->valueDirty = x->structDirty = false;
        for (JNode& k : x->kids) stack.push_back(&k);
    }
    return c;
}

JNode* OvFind(JNode* ov, const std::vector<Step>& steps){
    JNode* o = ov;
    for (const Step& s : steps){
        if (!o) return nullptr;
        if (s.index >= 0) o = o->kind == JNode::Kind::Array && s.index < (int)o->kids.size() ? &o->kids[s.index] : nullptr;
        else o = o->kind == JNode::Kind::Object ? o->member(s.key) : nullptr;
    }
    return o;
}

// The override node at `steps`, created as needed: objects start empty (members are added as they are edited),
// arrays and scalars are copied whole from the file.
JNode* OvEnsure(JNode& ov, const JNode& file, const std::vector<Step>& steps){
    JNode* o = &ov;
    const JNode* f = &file;
    for (size_t i = 0; i < steps.size(); ++i){
        const Step& s = steps[i];
        const JNode* fc = s.index >= 0 ? (s.index < (int)f->kids.size() ? &f->kids[s.index] : nullptr) : f->member(s.key);
        if (!fc) return nullptr;
        if (s.index >= 0){
            if (o->kind != JNode::Kind::Array || s.index >= (int)o->kids.size()) return nullptr;
            o = &o->kids[s.index];
        } else {
            if (o->kind != JNode::Kind::Object) return nullptr;
            JNode* m = o->member(s.key);
            if (!m){
                JNode n;
                if (fc->kind == JNode::Kind::Object && i + 1 < steps.size()) n.kind = JNode::Kind::Object;
                else n = CopyNoSpans(*fc);
                n.key = s.key;
                o->kids.push_back(std::move(n));
                m = &o->kids.back();
            }
            o = m;
        }
        f = fc;
    }
    return o;
}

// Removes the override at `steps` (an object member) and any objects it leaves empty.
void OvRemove(JNode& ov, const std::vector<Step>& steps){
    if (steps.empty()) return;
    std::vector<JNode*> chain{ &ov };
    for (size_t i = 0; i + 1 < steps.size(); ++i){
        JNode* m = chain.back()->member(steps[i].key);
        if (!m) return;
        chain.push_back(m);
    }
    auto& kids = chain.back()->kids;
    kids.erase(std::remove_if(kids.begin(), kids.end(), [&](const JNode& k){ return k.key == steps.back().key; }), kids.end());
    for (size_t i = chain.size() - 1; i > 0; --i){
        if (!chain[i]->kids.empty()) break;
        const std::string key = chain[i]->key;
        auto& pk = chain[i - 1]->kids;
        pk.erase(std::remove_if(pk.begin(), pk.end(), [&](const JNode& k){ return k.key == key; }), pk.end());
    }
}

bool PathHasIndex(const std::vector<Step>& steps){
    for (const Step& s : steps) if (s.index >= 0) return true;
    return false;
}

std::string SchemaPath(const std::vector<Step>& steps){
    std::string p;
    for (const Step& s : steps){
        if (s.index >= 0) p += "[]";
        else p += (p.empty() ? "" : ".") + s.key;
    }
    return p;
}

bool IsHexColor(const std::string& s){
    if (s.size() != 7 || s[0] != '#') return false;
    for (size_t i = 1; i < 7; ++i) if (!isxdigit((unsigned char)s[i])) return false;
    return true;
}

// Edits `n` (a scalar) with the widget its type / schema calls for. True when it changed.
bool ScalarWidget(JNode& n, const FieldMeta* m, const char* label){
    switch (n.kind){
        case JNode::Kind::Bool: return ImGui::Checkbox(label, &n.b);
        case JNode::Kind::Number:{
            const bool asInt = m && m->numberKind ? m->numberKind == 1 : n.isInt;
            if (m && !m->options.empty()){
                bool changed = false;
                const std::string cur = number(n);
                if (ImGui::BeginCombo(label, cur.c_str())){
                    for (const std::string& o : m->options)
                        if (ImGui::Selectable(o.c_str(), o == cur)){ n.num = std::strtod(o.c_str(), nullptr); changed = true; }
                    ImGui::EndCombo();
                }
                return changed;
            }
            if (asInt){
                int v = (int)std::lround(n.num);
                const bool c = m && m->min < m->max ? ImGui::SliderInt(label, &v, (int)m->min, (int)m->max)
                                                    : ImGui::DragInt(label, &v, m && m->step > 0.f ? m->step : 1.f);
                if (c){ n.num = v; n.isInt = true; }
                return c;
            }
            const float step = m && m->step > 0.f ? m->step : (float)(std::max)(0.01, std::fabs(n.num) * 0.01);
            bool c;
            if (m && m->min < m->max){
                const double lo = m->min, hi = m->max;
                c = ImGui::SliderScalar(label, ImGuiDataType_Double, &n.num, &lo, &hi, "%.3f");
            }
            else c = ImGui::DragScalar(label, ImGuiDataType_Double, &n.num, step, nullptr, nullptr, "%.3f");
            if (c) n.isInt = false;
            return c;
        }
        case JNode::Kind::String:{
            if (m && !m->options.empty()){
                bool changed = false;
                if (ImGui::BeginCombo(label, n.str.c_str())){
                    for (const std::string& o : m->options)
                        if (ImGui::Selectable(o.c_str(), o == n.str)){ n.str = o; changed = true; }
                    ImGui::EndCombo();
                }
                return changed;
            }
            if (IsHexColor(n.str)){
                float c[3];
                const unsigned v = (unsigned)std::stoul(n.str.substr(1), nullptr, 16);
                c[0] = ((v >> 16) & 255) / 255.f; c[1] = ((v >> 8) & 255) / 255.f; c[2] = (v & 255) / 255.f;
                if (ImGui::ColorEdit3(label, c, ImGuiColorEditFlags_DisplayHex)){
                    char b[8];
                    snprintf(b, sizeof(b), "#%02X%02X%02X", (int)std::lround(c[0] * 255), (int)std::lround(c[1] * 255), (int)std::lround(c[2] * 255));
                    n.str = b;
                    return true;
                }
                return false;
            }
            char buf[1024];
            strncpy_s(buf, n.str.c_str(), _TRUNCATE);
            if (ImGui::InputText(label, buf, sizeof(buf))){ n.str = buf; return true; }
            return false;
        }
        default:
            ImGui::TextDisabled("%s: null", label);
            return false;
    }
}

std::string NodeLabel(const JNode& n, const std::vector<Step>& steps, const FieldMeta* m){
    std::string label = m && !m->label.empty() ? m->label : steps.empty() ? std::string("(root)")
                      : steps.back().index >= 0 ? "[" + std::to_string(steps.back().index) + "]" : steps.back().key;
    if (n.kind == JNode::Kind::Object && !steps.empty() && steps.back().index >= 0)
        for (const char* k : { "name", "id", "prefab", "config" })
            if (const JNode* v = n.member(k); v && v->kind == JNode::Kind::String){ label += "  " + v->str; break; }
    if (m && !m->unit.empty()) label += " (" + m->unit + ")";
    return label;
}

void DrawNode(Ctx& c, JNode& file, int depth);

void DrawChildren(Ctx& c, JNode& file, int depth){
    const bool arr = file.kind == JNode::Kind::Array;
    int removeAt = -1;
    for (size_t k = 0; k < file.kids.size(); ++k){
        JNode& kid = file.kids[k];
        if (!arr && !kid.key.empty() && kid.key[0] == '_'){
            // Comments ("_about", "_tune") read as notes; "_meta" is the schema.
            if (kid.key != "_meta" && kid.kind == JNode::Kind::String && depth == 0){
                ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Muted);
                ImGui::TextWrapped("%s", kid.str.c_str());
                ImGui::PopStyleColor();
            }
            continue;
        }
        c.steps.push_back(arr ? Step{ "", (int)k } : Step{ kid.key, -1 });
        ImGui::PushID((int)k);
        DrawNode(c, kid, depth + 1);
        if (arr && !c.ov && !c.readOnly){
            ImGui::SameLine();
            if (ImGui::SmallButton("x")) removeAt = (int)k;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove this element");
        }
        ImGui::PopID();
        c.steps.pop_back();
    }
    if (removeAt >= 0){
        file.kids.erase(file.kids.begin() + removeAt);
        file.structDirty = true;
        c.changed = true;
    }
    if (arr && !c.ov && !c.readOnly){
        if (ImGui::SmallButton("+ add")){
            JNode n = file.kids.empty() ? JNode() : CopyNoSpans(file.kids.back());
            if (file.kids.empty()){ n.kind = JNode::Kind::Number; n.isInt = true; }
            file.kids.push_back(std::move(n));
            file.structDirty = true;
            c.changed = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add an element (a copy of the last one)");
    }
}

void DrawNode(Ctx& c, JNode& file, int depth){
    const std::string spath = SchemaPath(c.steps);
    const auto mit = c.doc.schema.find(spath);
    const FieldMeta* m = mit != c.doc.schema.end() ? &mit->second : nullptr;
    const std::string label = NodeLabel(file, c.steps, m);

    JNode* ovNode = c.ov ? OvFind(c.ov, c.steps) : nullptr;
    const bool overridden = ovNode != nullptr;
    const bool canReset = overridden && !PathHasIndex(c.steps) && (!file.isContainer() || file.kind == JNode::Kind::Array);

    auto boldPush = [&]{
        if (!overridden) return;
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x - 6.f, p.y), ImVec2(p.x - 3.f, p.y + ImGui::GetFrameHeight()),
                                                  ImGui::ColorConvertFloat4ToU32(EditorColors::Override));
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Override);
    };
    auto boldPop = [&]{ if (overridden) ImGui::PopStyleColor(); };
    auto resetButton = [&]{
        if (!canReset) return;
        ImGui::SameLine();
        if (ImGui::SmallButton("reset")){ OvRemove(*c.ov, c.steps); c.ovChanged = true; }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Back to the file's value");
    };

    if (file.isContainer()){
        boldPush();
        const bool open = ImGui::TreeNodeEx("##node", ImGuiTreeNodeFlags_SpanAvailWidth, "%s%s", label.c_str(),
                                            file.kind == JNode::Kind::Array ? ("  [" + std::to_string(file.kids.size()) + "]").c_str() : "");
        boldPop();
        if (m && !m->tooltip.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", m->tooltip.c_str());
        resetButton();
        if (open){
            DrawChildren(c, file, depth);
            ImGui::TreePop();
        }
        return;
    }

    // A scalar: the override's value when there is one.
    JNode shown = ovNode ? *ovNode : file;
    const bool ro = c.readOnly || (m && m->readOnly);
    boldPush();
    if (ro) ImGui::BeginDisabled();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
    const bool edited = ScalarWidget(shown, m, label.c_str());
    if (ro) ImGui::EndDisabled();
    boldPop();
    if (m && !m->tooltip.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", m->tooltip.c_str());
    resetButton();
    if (!edited || ro) return;

    if (c.ov){
        if (JNode* t = OvEnsure(*c.ov, c.doc.root, c.steps)){
            const std::string key = t->key;
            *t = CopyNoSpans(shown);
            t->key = key;
            c.ovChanged = true;
        }
    } else {
        const std::string key = file.key;
        const size_t b = file.begin, e = file.end;
        file = shown;
        file.key = key;
        file.begin = b;
        file.end = e;
        file.valueDirty = true;
        c.changed = true;
    }
}

} // namespace

bool DrawTree(Doc& doc, std::string* overrides, bool readOnly){
    if (!doc.error.empty()){
        ImGui::TextColored(EditorColors::Danger, "%s", doc.error.c_str());
        return false;
    }
    if (doc.root.kind != JNode::Kind::Object && doc.root.kind != JNode::Kind::Array){
        ImGui::TextDisabled("(not an object)");
        return false;
    }

    JNode ov;
    ov.kind = JNode::Kind::Object;
    if (overrides && !overrides->empty()){
        std::string err;
        if (!Parse(*overrides, ov, err) || ov.kind != JNode::Kind::Object){
            ImGui::TextColored(EditorColors::Danger, "overrides are not a JSON object (%s) - Reset all", err.c_str());
            return false;
        }
    }

    Ctx c{ doc, overrides ? &ov : nullptr, readOnly || doc.readOnly };
    ImGui::PushID(doc.assetPath.c_str());
    ImGui::Indent(6.f);
    DrawChildren(c, doc.root, 0);
    ImGui::Unindent(6.f);
    ImGui::PopID();

    if (c.changed){
        doc.dirty = true;
        if (IsPlaying()){ doc.editedInPlay = true; NotifyLive(doc); }
    }
    if (c.ovChanged && overrides){
        // Stored compact (one line) in the script field.
        std::string text;
        if (!ov.kids.empty()){
            std::function<std::string(const JNode&)> one = [&](const JNode& n) -> std::string {
                if (!n.isContainer()) return scalar(n);
                const bool obj = n.kind == JNode::Kind::Object;
                std::string s = obj ? "{" : "[";
                for (size_t k = 0; k < n.kids.size(); ++k){
                    if (k) s += ",";
                    if (obj) s += escape(n.kids[k].key) + ":";
                    s += one(n.kids[k]);
                }
                return s + (obj ? "}" : "]");
            };
            text = one(ov);
        }
        *overrides = text;
    }
    return c.changed || c.ovChanged;
}

void DrawToolbar(Doc& doc, bool showOpenButton){
    ImGui::PushID(doc.assetPath.c_str());
    std::string msg;
    if (doc.readOnly){
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Muted);
        ImGui::TextWrapped("Read-only. %s", doc.readOnlyNote.c_str());
        ImGui::PopStyleColor();
    }
    else if (doc.editedInPlay && !IsPlaying()){
        ImGui::TextColored(EditorColors::Warning, "Changed during Play - not saved.");
        ImGui::SameLine();
        if (ImGui::SmallButton("Keep changes")) Save(doc, msg);
        ImGui::SameLine();
        if (ImGui::SmallButton("Discard")) Revert(doc);
    }
    else {
        ImGui::BeginDisabled(!doc.dirty);
        if (ImGui::SmallButton("Save")) Save(doc, msg);
        ImGui::SameLine();
        if (ImGui::SmallButton("Revert")) Revert(doc);
        ImGui::EndDisabled();
        if (doc.dirty){
            ImGui::SameLine();
            ImGui::TextColored(EditorColors::Warning, IsPlaying() ? "* live (not saved)" : "* unsaved");
        }
    }
    if (showOpenButton){
        ImGui::SameLine();
        if (ImGui::SmallButton("Open in Ashfall Data")) RequestFocus(doc.assetPath);
    }
    if (!msg.empty()) PHX_LOG(Editor, Info, "AshfallData: %s", msg.c_str());
    ImGui::PopID();
}

} // namespace AshfallData
