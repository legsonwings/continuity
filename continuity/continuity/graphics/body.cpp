module;

#include "thirdparty/d3dx12.h"

module body;

namespace gfx
{

void dispatch(resource_bindings const& bindings, bool wireframe, uint dispatchx)
{
    auto& globares = gfx::globalresources::get();
    auto cmd_list = globares.cmdlist();

    cmd_list->SetGraphicsRootSignature(bindings.pipelineobjs.root_signature.Get());

    ID3D12DescriptorHeap* heaps[] = { globares.resourceheap().d3dheap.Get(), globares.samplerheap().d3dheap.Get() };
    cmd_list->SetDescriptorHeaps(_countof(heaps), heaps);

    //cmd_list->SetGraphicsRootConstantBufferView(bindings.constant.slot, bindings.constant.address);
    //
    //if (bindings.objectconstant.address != 0)
    //    cmd_list->SetGraphicsRootConstantBufferView(bindings.objectconstant.slot, bindings.objectconstant.address);
    //cmd_list->SetGraphicsRootShaderResourceView(bindings.vertex.slot, bindings.vertex.address);
    cmd_list->SetGraphicsRoot32BitConstants(bindings.rootconstants.slot, static_cast<UINT>(bindings.rootconstants.values.size()), bindings.rootconstants.values.data(), 0);

   /* if (bindings.instance.address != 0)
        cmd_list->SetGraphicsRootShaderResourceView(bindings.instance.slot, bindings.instance.address);*/

    /*if(bindings.texture.deschandle.ptr)
        cmd_list->SetGraphicsRootDescriptorTable(bindings.texture.slot, bindings.texture.deschandle);*/

    if (wireframe && bindings.pipelineobjs.pso_wireframe)
        cmd_list->SetPipelineState(bindings.pipelineobjs.pso_wireframe.Get());
    else if (bindings.pipelineobjs.pso_twosided)
        cmd_list->SetPipelineState(bindings.pipelineobjs.pso_twosided.Get());
    else
        cmd_list->SetPipelineState(bindings.pipelineobjs.pso.Get());

    cmd_list->DispatchMesh(static_cast<UINT>(dispatchx), 1, 1);
}

}