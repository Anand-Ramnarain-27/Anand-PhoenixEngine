#pragma once

#include "DescriptorBase.h"

class ModuleRTDescriptors;

class RenderTargetDesc : public DescriptorBase<RenderTargetDesc, ModuleRTDescriptors>
{
    using Base = DescriptorBase<RenderTargetDesc, ModuleRTDescriptors>;
    friend Base;

public:
    using Base::Base;

    static ModuleRTDescriptors* getModule();
};