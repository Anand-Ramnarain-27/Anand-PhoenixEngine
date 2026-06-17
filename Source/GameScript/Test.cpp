#include "Test.h"
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
using namespace rapidjson;

#include "Globals.h"

Test::Test() = default;

void Test::Start(GameObject* owner){
    m_owner = owner;
}

void Test::Update(float /*dt*/){
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
