#pragma once

#include "DescriptorBase.h"

class ModuleDSDescriptors;

class DepthStencilDesc : public DescriptorBase<DepthStencilDesc, ModuleDSDescriptors>
{
    using Base = DescriptorBase<DepthStencilDesc, ModuleDSDescriptors>;
    friend Base;

public:
    using Base::Base;

    static ModuleDSDescriptors* getModule();
};