#pragma once
#include <cstdlib>
#include <string>

// Minimal writer/reader for the flat {"key":value,...} strings that Component::onSave/onLoad exchange.
// Strings are escaped, so UI text may contain quotes, backslashes and newlines.
namespace UIJson {

inline void key(std::string& o, const char* k){
    if (!o.empty()) o += ',';
    o += '"'; o += k; o += "\":";
}

inline void putBool(std::string& o, const char* k, bool v){ key(o, k); o += v ? "true" : "false"; }
inline void putInt(std::string& o, const char* k, int v){ key(o, k); o += std::to_string(v); }
inline void putFloat(std::string& o, const char* k, float v){ key(o, k); o += std::to_string(v); }

inline void putString(std::string& o, const char* k, const std::string& v){
    key(o, k);
    o += '"';
    for (char c : v){
        switch (c){
        case '"':  o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n";  break;
        case '\r': o += "\\r";  break;
        case '\t': o += "\\t";  break;
        default:   o += c;      break;
        }
    }
    o += '"';
}

inline void putFloats(std::string& o, const char* k, const float* v, int count){
    key(o, k);
    o += '[';
    for (int i = 0; i < count; ++i){
        if (i) o += ',';
        o += std::to_string(v[i]);
    }
    o += ']';
}

class Reader {
public:
    explicit Reader(const std::string& json) : m_json(json){}

    bool getBool(const char* k, bool& out) const {
        size_t p;
        if (!find(k, p)) return false;
        out = m_json.compare(p, 4, "true") == 0;
        return true;
    }

    bool getInt(const char* k, int& out) const {
        size_t p;
        if (!find(k, p)) return false;
        out = std::atoi(m_json.c_str() + p);
        return true;
    }

    bool getFloat(const char* k, float& out) const {
        size_t p;
        if (!find(k, p)) return false;
        out = static_cast<float>(std::atof(m_json.c_str() + p));
        return true;
    }

    bool getString(const char* k, std::string& out) const {
        size_t p;
        if (!find(k, p) || m_json[p] != '"') return false;
        out.clear();
        for (++p; p < m_json.size() && m_json[p] != '"'; ++p){
            char c = m_json[p];
            if (c == '\\' && p + 1 < m_json.size()){
                switch (m_json[++p]){
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                default:  c = m_json[p]; break;
                }
            }
            out += c;
        }
        return true;
    }

    // Reads up to `count` floats from a [a,b,c] array; missing trailing values are left untouched.
    bool getFloats(const char* k, float* out, int count) const {
        size_t p;
        if (!find(k, p) || m_json[p] != '[') return false;
        ++p;
        for (int i = 0; i < count && p < m_json.size() && m_json[p] != ']'; ++i){
            char* end = nullptr;
            out[i] = std::strtof(m_json.c_str() + p, &end);
            p = static_cast<size_t>(end - m_json.c_str());
            if (p < m_json.size() && m_json[p] == ',') ++p;
        }
        return true;
    }

private:
    bool find(const char* k, size_t& pos) const {
        const std::string needle = std::string("\"") + k + "\":";
        pos = m_json.find(needle);
        if (pos == std::string::npos) return false;
        pos += needle.size();
        return pos < m_json.size();
    }

    const std::string& m_json;
};

} // namespace UIJson
