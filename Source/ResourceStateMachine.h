#pragma once
#include "ResourceCommon.h"
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

// Name + cached hash. Always serialise/deserialise from str; hash is recomputed on load.
struct HashString {
    std::string str;
    uint32_t hash = 0;

    HashString() = default;
    explicit HashString(const std::string& s) : str(s), hash(compute(s)) {}
    explicit HashString(std::string&& s) : str(std::move(s)), hash(compute(str)) {}

    HashString& operator=(const std::string& s) { str = s; hash = compute(str); return *this; }
    HashString& operator=(std::string&& s) { str = std::move(s); hash = compute(str); return *this; }

    bool operator==(const HashString& o) const { return hash == o.hash && str == o.str; }
    bool operator!=(const HashString& o) const { return !(*this == o); }
    bool empty() const { return str.empty(); }

    static uint32_t compute(const std::string& s){
        return static_cast<uint32_t>(std::hash<std::string>{}(s));
    }
};

struct SMClip {
    HashString name;
    UID animationUID = 0;
    bool loop = true;
};

struct SMState {
    HashString name;
    HashString clipName;
};

struct SMTransition {
    HashString source;
    HashString target;
    HashString trigger;
    uint32_t interpolationMs = 200;
};

class ResourceStateMachine : public ResourceBase {
public:
    explicit ResourceStateMachine(UID uid);

    bool LoadInMemory() override;
    void UnloadFromMemory() override;

    bool Save(const std::string& path) const;
    bool Load(const std::string& path);

    // Return nullptr / -1 if not found.
    const SMClip* FindClip(const HashString& name) const;
    const SMState* FindState(const HashString& name) const;
    int FindStateIndex(const HashString& name) const;

    void DrawInspector();

    std::vector<SMClip> clips;
    std::vector<SMState> states;
    std::vector<SMTransition> transitions;
    HashString defaultState;
};
