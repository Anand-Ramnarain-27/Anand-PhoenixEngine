#include "Globals.h"
#include "API/Phoenix_Navigation.h"
#include "Application.h"
#include "RuntimeCore.h"
#include "NavigationSystem.h"

namespace Phoenix {

static NavigationSystem* getNav(){
    if (!app || !app->getRuntimeCore()) return nullptr;
    return app->getRuntimeCore()->getNavigationSystem();
}

bool Navigation::FindPath(Vec3 start, Vec3 end, std::vector<Vec3>& outPath){
    if (NavigationSystem* nav = getNav())
        return nav->FindPath(start, end, outPath);
    return false;
}

bool Navigation::IsWalkable(Vec3 point){
    if (NavigationSystem* nav = getNav())
        return nav->IsWalkable(point);
    return false;
}

bool Navigation::LoadGraph(const std::string& name, const std::string& path){
    if (NavigationSystem* nav = getNav())
        return nav->LoadNamedGraph(name, path);
    return false;
}

bool Navigation::FindPathNamed(const std::string& name, Vec3 start, Vec3 end, std::vector<Vec3>& outPath){
    if (NavigationSystem* nav = getNav())
        return nav->FindPathNamed(name, start, end, outPath);
    return false;
}

bool Navigation::IsWalkableNamed(const std::string& name, Vec3 point){
    if (NavigationSystem* nav = getNav())
        return nav->IsWalkableNamed(name, point);
    return false;
}

} // namespace Phoenix
