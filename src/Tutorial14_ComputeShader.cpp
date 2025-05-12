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

namespace Diligent
{

SampleBase* CreateSample()
{
    return new Tutorial14_ComputeShader();
}

namespace
{

    const int3  kGridSize = {32, 32, 32};
    const float TimeStep  = 0.1f;

} // namespace

struct ConstantsStruct
{
    float timestep;
    float3 vec;
};
RefCntAutoPtr<IBuffer> m_pConstantsCB;

void Tutorial14_ComputeShader::CreateFluidTextures()
{
    // 1. Create and fill the CPU-side data buffer
    std::vector<float4> velocityData(kGridSize.x * kGridSize.y * kGridSize.z, float4{0, 0, 0, 0});

    for (int z = 0; z < kGridSize.z; ++z)
        for (int y = 0; y < kGridSize.y; ++y)
            for (int x = 0; x < kGridSize.x; ++x)
                velocityData[x + y * kGridSize.x + z * kGridSize.x * kGridSize.y] = float4{1, 0, 0, 1};

    // 2. Describe the texture
    TextureDesc texDesc;
    texDesc.Type      = RESOURCE_DIM_TEX_3D;
    texDesc.Width     = kGridSize.x;
    texDesc.Height    = kGridSize.y;
    texDesc.Depth     = kGridSize.z;
    texDesc.MipLevels = 1;
    texDesc.Usage     = USAGE_DEFAULT;
    texDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
    texDesc.Format    = TEX_FORMAT_RGBA32_FLOAT;

    // 3. Prepare subresource data
    TextureSubResData subresData;
    subresData.pData       = velocityData.data();
    subresData.Stride      = sizeof(float4) * kGridSize.x;               // row size in bytes
    subresData.DepthStride = sizeof(float4) * kGridSize.x * kGridSize.y; // slice size in bytes

    TextureData initData;
    initData.pSubResources   = &subresData;
    initData.NumSubresources = 1;

    // 4. Create the texture with initial data
    m_pDevice->CreateTexture(texDesc, &initData, &m_pVelocityTex[0]);

    // 5. Create the other textures without data
    m_pDevice->CreateTexture(texDesc, nullptr, &m_pVelocityTex[1]);

    // Create pressure and divergence textures (no init data)
    texDesc.Format = TEX_FORMAT_R32_FLOAT;
    m_pDevice->CreateTexture(texDesc, nullptr, &m_pPressureTex[0]);
    m_pDevice->CreateTexture(texDesc, nullptr, &m_pPressureTex[1]);
    m_pDevice->CreateTexture(texDesc, nullptr, &m_pDivergenceTex);
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

void Tutorial14_ComputeShader::UpdateFluidSimulation()
{
    DispatchComputeAttribs attribs;
    attribs.ThreadGroupCountX = (kGridSize.x + 7) / 8;
    attribs.ThreadGroupCountY = (kGridSize.y + 7) / 8;
    attribs.ThreadGroupCountZ = (kGridSize.z + 7) / 8;

    {
        MapHelper<ConstantsStruct> CBData(m_pImmediateContext, m_pConstantsAdvectCB, MAP_WRITE, MAP_FLAG_DISCARD);
        CBData->timestep = TimeStep;
        CBData->vec = float3{1.0f / kGridSize.x, 1.0f / kGridSize.y, 1.0f / kGridSize.z};
    } // <== Now buffer is unmapped

    if (auto* var = m_pAdvectSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "Constants"))
        var->Set(m_pConstantsAdvectCB);

    m_pImmediateContext->SetPipelineState(m_pAdvectPSO);
    m_pImmediateContext->CommitShaderResources(m_pAdvectSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->DispatchCompute(attribs);

    // FORCES
    {
        MapHelper<ConstantsStruct> CBData(m_pImmediateContext, m_pConstantsForcesCB, MAP_WRITE, MAP_FLAG_DISCARD);
        CBData->timestep = TimeStep;
        CBData->vec = float3{0.0f, -9.8f, 0.0f};
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
        std::swap(m_pPressureTex[0], m_pPressureTex[1]);
    }

    m_pImmediateContext->SetPipelineState(m_pProjectPSO);
    m_pImmediateContext->CommitShaderResources(m_pProjectSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->DispatchCompute(attribs);
    std::swap(m_pVelocityTex[0], m_pVelocityTex[1]);
}

void Tutorial14_ComputeShader::CreateRenderVolumePSO()
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    // Configurar el dise�o de recursos
    PipelineResourceLayoutDesc Layout;
    Layout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

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
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable      = false;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = false;

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
        if (auto* var = m_pRenderVolumeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "VolumeTex"))
            var->Set(m_pPressureTex[0]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

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

void Tutorial14_ComputeShader::RenderVolume()
{
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
    UpdateFluidSimulation();
    RenderVolume();
}

void Tutorial14_ComputeShader::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
}

} // namespace Diligent
