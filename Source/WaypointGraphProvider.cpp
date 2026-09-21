#include "Globals.h"
#include "WaypointGraphProvider.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "3rdParty/rapidjson/document.h"
#include <queue>
#include <unordered_map>
#include <algorithm>
#include <limits>

using namespace rapidjson;

namespace {
    struct OpenEntry {
        int nodeId;
        float fScore;
        bool operator>(const OpenEntry& o) const { return fScore > o.fScore; }
    };
}

int WaypointGraphProvider::findNearestNode(const Vector3& point) const{
    int best = -1;
    float bestDistSq = std::numeric_limits<float>::max();
    for (const auto& n : nodes){
        float d = (n.position - point).LengthSquared();
        if (d < bestDistSq){ bestDistSq = d; best = n.id; }
    }
    return best;
}

bool WaypointGraphProvider::IsWalkable(const Vector3& /*point*/, const AgentProfile& /*profile*/) const{
    return !nodes.empty();
}

int WaypointGraphProvider::getEdgeCount() const{
    int count = 0;
    for (const auto& n : nodes) count += (int)n.neighborIds.size();
    return count / 2; // graph is authored as bidirectional pairs
}

bool WaypointGraphProvider::FindPath(const Vector3& start, const Vector3& end,
                                     const AgentProfile& /*profile*/, std::vector<Vector3>& outPath){
    if (nodes.empty()) return false;

    int startId = findNearestNode(start);
    int endId = findNearestNode(end);
    if (startId < 0 || endId < 0) return false;

    if (startId == endId){
        outPath.push_back(end);
        return true;
    }

    std::unordered_map<int, const Node*> byId;
    for (const auto& n : nodes) byId[n.id] = &n;

    std::unordered_map<int, float> gScore;
    std::unordered_map<int, int> cameFrom;
    std::unordered_map<int, bool> closed;
    for (const auto& n : nodes) gScore[n.id] = std::numeric_limits<float>::max();
    gScore[startId] = 0.f;

    auto heuristic = [&](int id){ return (byId[id]->position - byId[endId]->position).Length(); };

    std::priority_queue<OpenEntry, std::vector<OpenEntry>, std::greater<OpenEntry>> open;
    open.push({ startId, heuristic(startId) });

    bool found = false;
    while (!open.empty()){
        int current = open.top().nodeId;
        open.pop();
        if (closed[current]) continue;
        closed[current] = true;

        if (current == endId){ found = true; break; }

        const Node* curNode = byId[current];
        for (int neighborId : curNode->neighborIds){
            auto it = byId.find(neighborId);
            if (it == byId.end() || closed[neighborId]) continue;

            float tentativeG = gScore[current] + (it->second->position - curNode->position).Length();
            if (tentativeG < gScore[neighborId]){
                gScore[neighborId] = tentativeG;
                cameFrom[neighborId] = current;
                open.push({ neighborId, tentativeG + heuristic(neighborId) });
            }
        }
    }

    if (!found) return false;

    std::vector<int> nodePath;
    for (int at = endId; ; at = cameFrom[at]){
        nodePath.push_back(at);
        if (at == startId) break;
    }
    std::reverse(nodePath.begin(), nodePath.end());

    for (int id : nodePath) outPath.push_back(byId[id]->position);
    outPath.push_back(end);
    return true;
}

bool WaypointGraphProvider::Load(const std::string& path){
    nodes.clear();

    char* buf = nullptr;
    unsigned size = app->getFileSystem()->Load(path.c_str(), &buf);
    if (!buf || size == 0){
        LOG("WaypointGraphProvider: could not read '%s'", path.c_str());
        return false;
    }

    Document doc;
    doc.Parse(buf, size);
    delete[] buf;

    if (doc.HasParseError() || !doc.HasMember("Nodes") || !doc["Nodes"].IsArray()){
        LOG("WaypointGraphProvider: invalid graph JSON in '%s'", path.c_str());
        return false;
    }

    const Value& arr = doc["Nodes"];
    nodes.reserve(arr.Size());
    for (SizeType i = 0; i < arr.Size(); ++i){
        const Value& v = arr[i];
        if (!v.HasMember("Id") || !v.HasMember("Position") || !v["Position"].IsArray()){
            LOG("WaypointGraphProvider: Nodes[%u] missing Id/Position - skipped", i);
            continue;
        }
        Node n;
        n.id = v["Id"].GetInt();
        const Value& pos = v["Position"];
        n.position = Vector3(pos[0].GetFloat(), pos[1].GetFloat(), pos[2].GetFloat());
        if (v.HasMember("Neighbors") && v["Neighbors"].IsArray()){
            const Value& nb = v["Neighbors"];
            for (SizeType j = 0; j < nb.Size(); ++j)
                n.neighborIds.push_back(nb[j].GetInt());
        }
        nodes.push_back(std::move(n));
    }

    return !nodes.empty();
}

// drawDebug() is in WaypointGraphProviderDebug.cpp - needs debug_draw.hpp.
