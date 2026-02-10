#include "Globals.h"
#include "ComponentTransform.h"

Matrix ComponentTransform::getLocalMatrix() const
{
    return Matrix::CreateScale(scale)
        * Matrix::CreateFromQuaternion(rotation)
        * Matrix::CreateTranslation(position);
}
