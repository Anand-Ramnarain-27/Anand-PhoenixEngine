#include "Globals.h"
//#include "ComponentMesh.h"
//#include "GameObject.h"
//#include "ComponentTransform.h"
//#include "Model.h"
//
//ComponentMesh::ComponentMesh(GameObject* owner)
//    : Component(owner)
//{
//    m_model = std::make_unique<Model>();
//}
//
//ComponentMesh::~ComponentMesh() = default;
//
//bool ComponentMesh::loadModel(const char* filePath)
//{
//    return m_model->loadPBRPhong(filePath);
//}
//
//void ComponentMesh::render(ID3D12GraphicsCommandList* cmd)
//{
//    if (!m_model)
//        return;
//
//    m_model->setModelMatrix(
//        owner->getTransform()->getGlobalMatrix()
//    );
//
//    m_model->draw(cmd);
//}
