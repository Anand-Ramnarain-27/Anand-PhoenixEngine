#include "Test.h"
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
using namespace rapidjson;

#include "Globals.h"
#include "API/PhoenixAPI.h"

Test::Test() = default;

void Test::Start(GameObject* owner){
    m_owner = owner;
}

// Exercises the Navigation/Perception/Steering/tag API surface as a link test.
void Test::Update(float dt){
    if (!m_owner) return;

    SetTag(m_owner, GetTag(m_owner));

    std::vector<Vec3> path;
    Navigation::FindPath(Position(m_owner), Position(m_owner), path);
    Navigation::IsWalkable(Position(m_owner));

    RaycastResult hit = Perception::Raycast(Position(m_owner), Vec3(0, 0, 1), 5.f);
    Perception::IsLineClear(Position(m_owner), Position(m_owner) + Vec3(0, 0, 1));
    Perception::ValidateChargeLane(Position(m_owner), Vec3(0, 0, 1), 1.f, 5.f);
    Perception::FindNearestWithTag(GetTag(m_owner), Position(m_owner), m_owner);

    Vec3 desired = Steering::Seek(Position(m_owner), Position(m_owner) + Vec3(1, 0, 0), 3.f);
    Steering::ApplySteering(Vec3::Zero, desired, 3.f, 10.f, dt);

    (void)hit;
}

void Test::Destroy(){
}

void Test::Editor(){
}

std::string Test::Save() const{
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    return buf.GetString();
}

void Test::Load(const std::string& json){
    Document doc; doc.Parse(json.c_str());
    if (doc.HasParseError()) return;
}

IScript* Create_Test(){ return new Test(); }
