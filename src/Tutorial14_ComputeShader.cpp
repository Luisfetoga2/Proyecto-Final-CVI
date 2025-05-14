/*
 *  Copyright 2019-2024 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#include "Tutorial14_ComputeShader.hpp"
#include "BasicMath.hpp"
#include "MapHelper.hpp"
#include "imgui.h"
#include "ShaderMacroHelper.hpp"
#include "ColorConversion.h"
#include "TextureUtilities.h"

namespace Diligent
{

SampleBase* CreateSample()
{
    return new Tutorial14_ComputeShader();
}

namespace
{

    const int3  kGridSize = {40,40,1};
    const float TimeStep  = 1.0f;

} // namespace

struct ConstantsStruct
{
    float timestep;
    float3 vec;
};
RefCntAutoPtr<IBuffer> m_pConstantsCB;

// Add these to your class (in the .hpp, but for demo here):
bool m_InjectVelocity = false;
float4 m_CustomVelocity = float4{0, 100, 0, 1};
// Visualization mode: 0 = Velocity, 1 = Pressure
int m_VisualizationMode = 0;

// Add a 1x1x1 RGBA32_FLOAT staging texture for injection
RefCntAutoPtr<ITexture> m_pVelocityInjectStagingTex;

void Tutorial14_ComputeShader::CreateFluidTextures()
{
    // Use float4 for velocity data to match the compute shader and D3D12 UAV requirements
    std::vector<float4> velocityData(kGridSize.x * kGridSize.y * kGridSize.z, float4{0, 0, 0, 0});

    for (int z = 0; z < kGridSize.z; ++z)
        for (int y = 0; y < kGridSize.y; ++y)
            for (int x = 0; x < kGridSize.x; ++x) {
                if (y>= kGridSize.y / 2 - 1 && y <= kGridSize.y / 2 )
                {
                    if (x >= kGridSize.x / 2 - 1 && x <= kGridSize.x / 2 )
                    {
                        velocityData[z * kGridSize.y * kGridSize.x + y * kGridSize.x + x] = float4{0, 1, 0, 1};
                    }
                }
            }

    TextureDesc texDesc;
    texDesc.Type      = RESOURCE_DIM_TEX_3D;
    texDesc.Width     = kGridSize.x;
    texDesc.Height    = kGridSize.y;
    texDesc.Depth     = kGridSize.z;
    texDesc.MipLevels = 1;
    texDesc.Usage     = USAGE_DEFAULT;
    texDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
    texDesc.Format    = TEX_FORMAT_RGBA32_FLOAT; // Use RGBA32_FLOAT for UAV support

    TextureSubResData subresData;
    subresData.pData       = velocityData.data();
    subresData.Stride      = sizeof(float4) * kGridSize.x;
    subresData.DepthStride = sizeof(float4) * kGridSize.x * kGridSize.y;

    TextureData initData;
    initData.pSubResources   = &subresData;
    initData.NumSubresources = 1;

    m_pDevice->CreateTexture(texDesc, &initData, &m_pVelocityTex[0]);
    m_pDevice->CreateTexture(texDesc, nullptr, &m_pVelocityTex[1]);

    texDesc.Format = TEX_FORMAT_R32_FLOAT;
    m_pDevice->CreateTexture(texDesc, nullptr, &m_pPressureTex[0]);
    m_pDevice->CreateTexture(texDesc, nullptr, &m_pPressureTex[1]);
    m_pDevice->CreateTexture(texDesc, nullptr, &m_pDivergenceTex);

    // Create a staging texture for readback (also RGBA32_FLOAT)
    TextureDesc stagingDesc = texDesc;
    stagingDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
    stagingDesc.Usage = USAGE_STAGING;
    stagingDesc.BindFlags = BIND_NONE;
    stagingDesc.CPUAccessFlags = CPU_ACCESS_READ;
    m_pDevice->CreateTexture(stagingDesc, nullptr, &m_pVelocityStagingTex);

    // Create a 1x1x1 staging texture for injection (CPU write)
    TextureDesc injectDesc = stagingDesc;
    injectDesc.Width = 1;
    injectDesc.Height = 1;
    injectDesc.Depth = 1;
    injectDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    m_pDevice->CreateTexture(injectDesc, nullptr, &m_pVelocityInjectStagingTex);
}

void Tutorial14_ComputeShader::CreateFluidShaders()
{
    const char* shaderFiles[] = {
        "advect.csh", "apply_forces.csh", "divergence.csh",
        "jacobi.csh", "project.csh"};

    ShaderCreateInfo shaderCI;
    shaderCI.SourceLanguage                  = SHADER_SOURCE_LANGUAGE_HLSL;
    shaderCI.Desc.UseCombinedTextureSamplers = false;
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderFactory);
    shaderCI.pShaderSourceStreamFactory = pShaderFactory;

    const char* names[] = {"Advect", "Forces", "Divergence", "Jacobi", "Project"};

    for (int i = 0; i < 5; ++i)
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
        shaderCI.EntryPoint      = "main";
        shaderCI.Desc.Name       = names[i];
        shaderCI.FilePath        = shaderFiles[i];

        RefCntAutoPtr<IShader> pCS;
        m_pDevice->CreateShader(shaderCI, &pCS);

        if (!pCS)
        {
            LOG_ERROR_MESSAGE("Error compilando shader: ", shaderFiles[i]);
            continue;
        }

        ComputePipelineStateCreateInfo psoCI;
        psoCI.PSODesc.PipelineType                       = PIPELINE_TYPE_COMPUTE;
        psoCI.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
        psoCI.PSODesc.Name                               = names[i];
        psoCI.pCS                                        = pCS;

        RefCntAutoPtr<IPipelineState>& pPSO =
            (i == 1) ? m_pForcePSO :
            (i == 2) ? m_pDivergencePSO :
            (i == 3) ? m_pJacobiPSO :
            (i == 4) ? m_pProjectPSO :
                       m_pAdvectPSO;

        m_pDevice->CreateComputePipelineState(psoCI, &pPSO);
        if (!pPSO)
        {
            LOG_ERROR_MESSAGE("Error creando PSO: ", names[i]);
        }

    }
}

void Tutorial14_ComputeShader::CreateShaderResourceBindings()
{
    if (!m_pAdvectPSO || !m_pForcePSO || !m_pDivergencePSO || !m_pJacobiPSO || !m_pProjectPSO)
    {
        LOG_ERROR_MESSAGE("Uno o m�s PSO no se crearon correctamente. SRBs no ser�n inicializados.");
        return;
    }

    // Crear SRB para cada PSO
    m_pAdvectPSO->CreateShaderResourceBinding(&m_pAdvectSRB, true);
    m_pForcePSO->CreateShaderResourceBinding(&m_pForceSRB, true);
    m_pDivergencePSO->CreateShaderResourceBinding(&m_pDivergenceSRB, true);
    m_pJacobiPSO->CreateShaderResourceBinding(&m_pJacobiSRB, true);
    m_pProjectPSO->CreateShaderResourceBinding(&m_pProjectSRB, true);

    if (!m_pAdvectSRB || !m_pForceSRB || !m_pDivergenceSRB || !m_pJacobiSRB || !m_pProjectSRB)
    {
        LOG_ERROR_MESSAGE("FIFO: Error creando SRBs.");
        return;
    }
    else {
        LOG_INFO_MESSAGE("FIFO: SRBs creados correctamente.");
    }

    // Sampler for Advect
    SamplerDesc SamDesc;
    SamDesc.MinFilter = FILTER_TYPE_LINEAR;
    SamDesc.MagFilter = FILTER_TYPE_LINEAR;
    SamDesc.MipFilter = FILTER_TYPE_LINEAR;
    RefCntAutoPtr<ISampler> pLinearSampler;
    m_pDevice->CreateSampler(SamDesc, &pLinearSampler);

    // ADVECT: Bind VelocityInSampler (SRV) and VelocityOut (UAV)
    if (auto* var = m_pAdvectSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "VelocityInSampler"))
        var->Set(m_pVelocityTex[0]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    if (auto* var = m_pAdvectSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "VelocityInSampler_sampler"))
        var->Set(pLinearSampler);
    if (auto* var = m_pAdvectSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "VelocityOut"))
        var->Set(m_pVelocityTex[1]->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
    if (auto* var = m_pAdvectSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "Constants"))
        var->Set(m_pConstantsCB);

    // FORCES: Bind Velocity (UAV)
    if (auto* var = m_pForceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "Velocity"))
        var->Set(m_pVelocityTex[1]->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
    if (auto* var = m_pForceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "Constants"))
        var->Set(m_pConstantsCB);

    // DIVERGENCE: Bind VelocitySampler (SRV) and Divergence (UAV)
    if (auto* var = m_pDivergenceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "VelocitySampler"))
        var->Set(m_pVelocityTex[1]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    if (auto* var = m_pDivergenceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "Divergence"))
        var->Set(m_pDivergenceTex->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));

    // JACOBI: Bind PressureIn (SRV), Divergence (SRV), PressureOut (UAV)
    if (auto* var = m_pJacobiSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "PressureIn"))
        var->Set(m_pPressureTex[0]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    if (auto* var = m_pJacobiSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "Divergence"))
        var->Set(m_pDivergenceTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    if (auto* var = m_pJacobiSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "PressureOut"))
        var->Set(m_pPressureTex[1]->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));

    // PROJECT: Bind Pressure (SRV), Velocity (UAV)
    if (auto* var = m_pProjectSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "Pressure"))
        var->Set(m_pPressureTex[0]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    if (auto* var = m_pProjectSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "Velocity"))
        var->Set(m_pVelocityTex[1]->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));

    if (auto* var = m_pAdvectSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "VelocityInSampler_sampler"))
        var->Set(pLinearSampler);
}

void Tutorial14_ComputeShader::UpdateFluidSimulation(double ElapsedTime)
{
    LOG_INFO_MESSAGE("UpdateFluidSimulation called, ElapsedTime = ", ElapsedTime);
    DispatchComputeAttribs attribs;
    attribs.ThreadGroupCountX = (kGridSize.x + 7) / 8;
    attribs.ThreadGroupCountY = (kGridSize.y + 7) / 8;
    attribs.ThreadGroupCountZ = (kGridSize.z + 7) / 8;

    // ADVECT
    {
        MapHelper<ConstantsStruct> CBData(m_pImmediateContext, m_pConstantsAdvectCB, MAP_WRITE, MAP_FLAG_DISCARD);
        CBData->timestep = TimeStep * static_cast<float>(ElapsedTime);
        CBData->vec = float3{1.0f / kGridSize.x, 1.0f / kGridSize.y, 1.0f / kGridSize.z};
    }

    if (auto* var = m_pAdvectSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "Constants"))
        var->Set(m_pConstantsAdvectCB);

    m_pImmediateContext->SetPipelineState(m_pAdvectPSO);
    m_pImmediateContext->CommitShaderResources(m_pAdvectSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->DispatchCompute(attribs);

    // Inject custom velocity if requested
    if (m_InjectVelocity)
    {
        // Write to the 1x1x1 staging texture
        MappedTextureSubresource mapped;
        m_pImmediateContext->MapTextureSubresource(
            m_pVelocityInjectStagingTex, 0, 0, MAP_WRITE, MAP_FLAG_DISCARD, nullptr, mapped);
        if (mapped.pData)
        {
            float* cell = reinterpret_cast<float*>(mapped.pData);
            cell[0] = m_CustomVelocity.x;
            cell[1] = m_CustomVelocity.y;
            cell[2] = m_CustomVelocity.z;
            cell[3] = m_CustomVelocity.w;
        }
        m_pImmediateContext->UnmapTextureSubresource(m_pVelocityInjectStagingTex, 0, 0);

        // Removed invalid debug log: cannot map CPU_WRITE-only texture for reading

        int centerX = kGridSize.x / 2;
        int centerY = kGridSize.y / 2;
        int centerZ = kGridSize.z / 2;

        // Explicitly transition destination textures to COPY_DEST
        StateTransitionDesc transitions[2] = {
            {m_pVelocityTex[0], RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_DEST, STATE_TRANSITION_FLAG_UPDATE_STATE},
            {m_pVelocityTex[1], RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_DEST, STATE_TRANSITION_FLAG_UPDATE_STATE}
        };
        m_pImmediateContext->TransitionResourceStates(2, transitions);

        // Copy from staging to both velocity textures
        CopyTextureAttribs copyAttribs = {};
        copyAttribs.pSrcTexture = m_pVelocityInjectStagingTex;
        copyAttribs.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        copyAttribs.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        copyAttribs.DstX = centerX;
        copyAttribs.DstY = centerY;
        copyAttribs.DstZ = centerZ;
        copyAttribs.pDstTexture = m_pVelocityTex[0];
        m_pImmediateContext->CopyTexture(copyAttribs);
        copyAttribs.pDstTexture = m_pVelocityTex[1];
        m_pImmediateContext->CopyTexture(copyAttribs);

        // Transition back to UAV for simulation
        StateTransitionDesc transitionsBack[2] = {
            {m_pVelocityTex[0], RESOURCE_STATE_COPY_DEST, RESOURCE_STATE_UNORDERED_ACCESS, STATE_TRANSITION_FLAG_UPDATE_STATE},
            {m_pVelocityTex[1], RESOURCE_STATE_COPY_DEST, RESOURCE_STATE_UNORDERED_ACCESS, STATE_TRANSITION_FLAG_UPDATE_STATE}
        };
        m_pImmediateContext->TransitionResourceStates(2, transitionsBack);

        m_InjectVelocity = false;
        // Log the injected value immediately after injection (on m_pVelocityTex[1])
        Box logRegion;
        logRegion.MinX = centerX; logRegion.MaxX = centerX + 1;
        logRegion.MinY = centerY; logRegion.MaxY = centerY + 1;
        logRegion.MinZ = centerZ; logRegion.MaxZ = centerZ + 1;
        CopyTextureAttribs logCopyAttribs = {};
        logCopyAttribs.pSrcTexture = m_pVelocityTex[1];
        logCopyAttribs.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        logCopyAttribs.pDstTexture = m_pVelocityStagingTex;
        logCopyAttribs.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        logCopyAttribs.pSrcBox = &logRegion;
        logCopyAttribs.DstX = 0;
        logCopyAttribs.DstY = 0;
        logCopyAttribs.DstZ = 0;
        m_pImmediateContext->CopyTexture(logCopyAttribs);
        MappedTextureSubresource mappedData;
        m_pImmediateContext->MapTextureSubresource(
            m_pVelocityStagingTex, 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT, nullptr, mappedData);
        if (mappedData.pData)
        {
            float* cell = reinterpret_cast<float*>(mappedData.pData);
            LOG_INFO_MESSAGE("[Injection] Center cell velocity: ", cell[0], ", ", cell[1], ", ", cell[2], ", ", cell[3]);
        }
        m_pImmediateContext->UnmapTextureSubresource(m_pVelocityStagingTex, 0, 0);
    }

    // Copy and read center cell velocity from the output texture (m_pVelocityTex[1]) BEFORE swapping
    {
        int centerX = kGridSize.x / 2;
        int centerY = kGridSize.y / 2;
        int centerZ = kGridSize.z / 2;

        // Also check a cell far from the center
        int edgeX = 0;
        int edgeY = 0;
        int edgeZ = 0;

        // Copy the region from simulation texture to staging texture
        Box region;
        region.MinX = centerX; region.MaxX = centerX + 1;
        region.MinY = centerY; region.MaxY = centerY + 1;
        region.MinZ = centerZ; region.MaxZ = centerZ + 1;

        CopyTextureAttribs copyAttribs = {};
        copyAttribs.pSrcTexture = m_pVelocityTex[1]; // Use the output texture
        copyAttribs.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        copyAttribs.pDstTexture = m_pVelocityStagingTex;
        copyAttribs.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        copyAttribs.pSrcBox = &region;
        copyAttribs.DstX = 0;
        copyAttribs.DstY = 0;
        copyAttribs.DstZ = 0;
        m_pImmediateContext->CopyTexture(copyAttribs);

        // Now map the staging texture
        MappedTextureSubresource mappedData;
        m_pImmediateContext->MapTextureSubresource(
            m_pVelocityStagingTex, // ITexture*
            0,                     // MipLevel
            0,                     // ArraySlice
            MAP_READ,              // MAP_TYPE
            MAP_FLAG_DO_NOT_WAIT,  // MAP_FLAGS
            nullptr,               // Box* (null means whole subresource)
            mappedData             // MappedTextureSubresource&
        );

        if (mappedData.pData)
        {
            float* cell = reinterpret_cast<float*>(mappedData.pData);
            LOG_INFO_MESSAGE("Center cell velocity: ", cell[0], ", ", cell[1], ", ", cell[2], ", ", cell[3]);
        }
        m_pImmediateContext->UnmapTextureSubresource(m_pVelocityStagingTex, 0, 0);

        // Now check the edge cell
        Box edgeRegion;
        edgeRegion.MinX = edgeX; edgeRegion.MaxX = edgeX + 1;
        edgeRegion.MinY = edgeY; edgeRegion.MaxY = edgeY + 1;
        edgeRegion.MinZ = edgeZ; edgeRegion.MaxZ = edgeZ + 1;
        copyAttribs.pSrcBox = &edgeRegion;
        copyAttribs.DstX = 0;
        copyAttribs.DstY = 0;
        copyAttribs.DstZ = 0;
        m_pImmediateContext->CopyTexture(copyAttribs);
        m_pImmediateContext->MapTextureSubresource(
            m_pVelocityStagingTex, 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT, nullptr, mappedData);
        if (mappedData.pData)
        {
            float* cell = reinterpret_cast<float*>(mappedData.pData);
            LOG_INFO_MESSAGE("Edge cell velocity: ", cell[0], ", ", cell[1], ", ", cell[2], ", ", cell[3]);
        }
        m_pImmediateContext->UnmapTextureSubresource(m_pVelocityStagingTex, 0, 0);
    }

    // Now swap textures AFTER reading
    std::swap(m_pVelocityTex[0], m_pVelocityTex[1]);

    // Update Advect SRB for next frame
    if (auto* var = m_pAdvectSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "VelocityInSampler"))
        var->Set(m_pVelocityTex[0]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE), SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
    if (auto* var = m_pAdvectSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "VelocityOut"))
        var->Set(m_pVelocityTex[1]->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS), SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);

    // Update RenderVolume SRB for visualization
    if (auto* var = m_pRenderVolumeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "VolumeTex"))
        var->Set(m_pVelocityTex[0]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE), SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);

    // FORCES
    {
        MapHelper<ConstantsStruct> CBData(m_pImmediateContext, m_pConstantsForcesCB, MAP_WRITE, MAP_FLAG_DISCARD);
        CBData->timestep = TimeStep * static_cast<float>(ElapsedTime);
        CBData->vec = float3{0.0f, 0.0f, 0.0f};
    }
    if (auto* var = m_pForceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "Constants"))
        var->Set(m_pConstantsForcesCB);
    m_pImmediateContext->SetPipelineState(m_pForcePSO);
    m_pImmediateContext->CommitShaderResources(m_pForceSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->DispatchCompute(attribs);


    m_pImmediateContext->SetPipelineState(m_pDivergencePSO);
    m_pImmediateContext->CommitShaderResources(m_pDivergenceSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->DispatchCompute(attribs);

    for (int i = 0; i < 40; ++i)
    {
        m_pImmediateContext->SetPipelineState(m_pJacobiPSO);
        m_pImmediateContext->CommitShaderResources(m_pJacobiSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->DispatchCompute(attribs);
    }

    m_pImmediateContext->SetPipelineState(m_pProjectPSO);
    m_pImmediateContext->CommitShaderResources(m_pProjectSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->DispatchCompute(attribs);

    std::swap(m_pPressureTex[0], m_pPressureTex[1]);
    //std::swap(m_pVelocityTex[0], m_pVelocityTex[1]);
}

void Tutorial14_ComputeShader::CreateRenderVolumePSO()
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    // Configurar el dise�o de recursos
    PipelineResourceLayoutDesc Layout;
    Layout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    ShaderResourceVariableDesc Vars[] = {
        {SHADER_TYPE_PIXEL, "VolumeTex", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
    };
    Layout.Variables = Vars;
    Layout.NumVariables = _countof(Vars);

    PSOCreateInfo.PSODesc.ResourceLayout = Layout;

    PSOCreateInfo.PSODesc.PipelineType              = PIPELINE_TYPE_GRAPHICS;
    PSOCreateInfo.PSODesc.Name                      = "Render Volume PSO";
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;

    // Configurar formatos de render target y depth-stencil
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat     = m_pSwapChain->GetDesc().DepthBufferFormat;

    // Configurar blending para volumen transparente
    BlendStateDesc BlendDesc;
    BlendDesc.RenderTargets[0].BlendEnable   = true;
    BlendDesc.RenderTargets[0].SrcBlend      = BLEND_FACTOR_SRC_ALPHA;
    BlendDesc.RenderTargets[0].DestBlend     = BLEND_FACTOR_INV_SRC_ALPHA;
    PSOCreateInfo.GraphicsPipeline.BlendDesc = BlendDesc;

    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology                 = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable      = true;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = true;

    // Crear shaders
    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderFactory;
    ShaderCI.EntryPoint                 = "main";

    RefCntAutoPtr<IShader> pVS, pPS;

    // Crear Vertex Shader
    ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
    ShaderCI.Desc.Name       = "Volume VS";
    ShaderCI.FilePath        = "volume.vsh";
    m_pDevice->CreateShader(ShaderCI, &pVS);

    // Crear Pixel Shader
    ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
    ShaderCI.Desc.Name       = "Volume PS";
    ShaderCI.FilePath        = "volume.psh";
    m_pDevice->CreateShader(ShaderCI, &pPS);

    PSOCreateInfo.pVS                    = pVS;
    PSOCreateInfo.pPS                    = pPS;
    PSOCreateInfo.PSODesc.ResourceLayout = Layout;

    // Crear el Pipeline State Object
    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pRenderVolumePSO);

    if (m_pRenderVolumePSO)
        m_pRenderVolumePSO->CreateShaderResourceBinding(&m_pRenderVolumeSRB, true);
        
        if (auto* var = m_pRenderVolumeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "sampLinear"))
        {
            SamplerDesc SamDesc;
            SamDesc.MinFilter = FILTER_TYPE_LINEAR;
            SamDesc.MagFilter = FILTER_TYPE_LINEAR;
            SamDesc.MipFilter = FILTER_TYPE_LINEAR;
            RefCntAutoPtr<ISampler> pSampler;
            m_pDevice->CreateSampler(SamDesc, &pSampler);
            var->Set(pSampler);
        }
    else
        LOG_ERROR_MESSAGE("FIFO: Error creando el PSO de renderizado de volumen.");
}

void Tutorial14_ComputeShader::CreateConsantBuffer()
{
    BufferDesc CBDesc;
    CBDesc.Size = sizeof(ConstantsStruct);
    CBDesc.Usage = USAGE_DYNAMIC;
    CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
    CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;

    CBDesc.Name = "Constants Advect CB";
    m_pDevice->CreateBuffer(CBDesc, nullptr, &m_pConstantsAdvectCB);

    CBDesc.Name = "Constants Forces CB";
    m_pDevice->CreateBuffer(CBDesc, nullptr, &m_pConstantsForcesCB);
}


void Tutorial14_ComputeShader::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);
    
    CreateConsantBuffer();
    CreateFluidTextures();
    CreateFluidShaders();
    CreateShaderResourceBindings();
    CreateRenderVolumePSO();
}

// Add this function to handle the UI (call it from your Render or a dedicated UI function):
void Tutorial14_ComputeShader::RenderUI()
{
    ImGui::Begin("Fluid Controls");
    ImGui::InputFloat4("Custom Velocity", &m_CustomVelocity.x);
    if (ImGui::Button("Inject Center Velocity"))
        m_InjectVelocity = true;
    const char* visModes[] = { "Velocity", "Pressure" };
    ImGui::Combo("Visualization", &m_VisualizationMode, visModes, IM_ARRAYSIZE(visModes));
    ImGui::End();
}

void Tutorial14_ComputeShader::RenderVolume()
{
    // Choose which texture to visualize
    ITextureView* pSRV = nullptr;
    if (m_VisualizationMode == 0) // Velocity
    {
        pSRV = m_pVelocityTex[1]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        // Transition the velocity texture to SHADER_RESOURCE state before binding
        StateTransitionDesc transitionDesc(
            m_pVelocityTex[1],
            RESOURCE_STATE_UNKNOWN,
            RESOURCE_STATE_SHADER_RESOURCE,
            STATE_TRANSITION_FLAG_UPDATE_STATE
        );
        m_pImmediateContext->TransitionResourceStates(1, &transitionDesc);
    }
    else // Pressure
    {
        pSRV = m_pPressureTex[1]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        StateTransitionDesc transitionDesc(
            m_pPressureTex[1],
            RESOURCE_STATE_UNKNOWN,
            RESOURCE_STATE_SHADER_RESOURCE,
            STATE_TRANSITION_FLAG_UPDATE_STATE
        );
        m_pImmediateContext->TransitionResourceStates(1, &transitionDesc);
    }

    if (auto* var = m_pRenderVolumeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "VolumeTex"))
        var->Set(pSRV);

    m_pImmediateContext->SetPipelineState(m_pRenderVolumePSO);
    m_pImmediateContext->CommitShaderResources(m_pRenderVolumeSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

    DrawAttribs DrawAttrs;
    DrawAttrs.NumVertices = 6; // Fullscreen quad
    DrawAttrs.Flags       = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->Draw(DrawAttrs);
}

// Render a frame
void Tutorial14_ComputeShader::Render()
{
    ITextureView* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    ITextureView* pDSV = m_pSwapChain->GetDepthBufferDSV();
    float4        ClearColor = {1.0f, 1.0f, 1.0f, 1.0f};
    
    // Let the engine perform required state transitions
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    RenderVolume();
    RenderUI();
}

void Tutorial14_ComputeShader::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    UpdateFluidSimulation(ElapsedTime);

}

} // namespace Diligent
