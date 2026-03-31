#pragma once
#include "ResourceCommon.h"
#include <string>
#include <vector>
#include <unordered_map>

struct SMClip {
    std::string name;     
    UID         animUID = 0;
    bool        loop = true;
};

struct SMState {
    std::string name;   
    std::string clipName; 

    float nodeX = 0.0f;
    float nodeY = 0.0f;
};

struct SMTransition {
    std::string source;          
    std::string target;           
    std::string triggerName;      
    float       interpolationMs = 200.0f;
};

class StateMachineResource {
public:
    std::string               defaultState; 
    std::vector<SMClip>       clips;
    std::vector<SMState>      states;
    std::vector<SMTransition> transitions;

    SMClip* findClip(const std::string& name);
    const SMClip* findClip(const std::string& name) const;
    SMState* findState(const std::string& name);
    const SMState* findState(const std::string& name) const;

    const SMTransition* findTransition(const std::string& fromState, const std::string& trigger) const;

    bool saveToFile(const std::string& filePath) const;
    bool loadFromFile(const std::string& filePath);
    std::string toJson()  const;
    bool        fromJson(const std::string& json);
};
