#pragma once
// Ref-counted handle to a depth-stencil view.

#include "DescriptorBase.h"

class ModuleDSDescriptors;

class DepthStencilDesc : public DescriptorBase<DepthStencilDesc, ModuleDSDescriptors> {
    using Base = DescriptorBase<DepthStencilDesc, ModuleDSDescriptors>;
    friend Base;

public:
    using Base::Base;

    static ModuleDSDescriptors* getModule();
};
