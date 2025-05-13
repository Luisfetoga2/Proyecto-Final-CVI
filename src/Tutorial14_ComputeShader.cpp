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

namespace Diligent
{

SampleBase* CreateSample()
{
    return new Tutorial14_ComputeShader();
}

namespace
{

    const int3  kGridSize = {32, 32, 32};
    const float TimeStep  = 1.0f;

} // namespace

struct ConstantsStruct
{
    float timestep;
    float3 vec;
};
RefCntAutoPtr<IBuffer> m_pConstantsCB;

void Tutorial14_ComputeShader::CreateFluidTextures()
{
    std::vector<float4> velocityData(kGridSize.x * kGridSize.y * kGridSize.z, float4{0, 0, 0, 0});

    //velocityData[0] = float4(1, 0, 0, 1);

    for (int z = 0; z < kGridSize.z; ++z)
        for (int y = 0; y < kGridSize.y; ++y)
            for (int x = 0; x < kGridSize.x; ++x) {
                if (y>= kGridSize.y / 2 - 2 && y <= kGridSize.y / 2 + 2)
                {
                    if (x >= kGridSize.x / 2 - 2 && x <= kGridSize.x / 2 + 2)
                    {
                        velocityData[z * kGridSize.y * kGridSize.x + y * kGridSize.x + x] = float4{0, -1, 0, 1};
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
    texDesc.Format    = TEX_FORMAT_RGBA32_FLOAT;

    
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
    
    // ADVECT
    {
        MapHelper<ConstantsStruct> CBData(m_pImmediateContext, m_pConstantsAdvectCB, MAP_WRITE, MAP_FLAG_DISCARD);
        CBData->timestep = TimeStep;
        CBData->vec = float3{1.0f / kGridSize.x, 1.0f / kGridSize.y, 1.0f / kGridSize.z};
    }

    if (auto* var = m_pAdvectSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "Constants"))
        var->Set(m_pConstantsAdvectCB);

    m_pImmediateContext->SetPipelineState(m_pAdvectPSO);
    m_pImmediateContext->CommitShaderResources(m_pAdvectSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->DispatchCompute(attribs);

    // FORCES
    {
        MapHelper<ConstantsStruct> CBData(m_pImmediateContext, m_pConstantsForcesCB, MAP_WRITE, MAP_FLAG_DISCARD);
        CBData->timestep = TimeStep;
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
    std::swap(m_pVelocityTex[0], m_pVelocityTex[1]);
}

void Tutorial14_ComputeShader::CreateRenderVolumePSO()
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;

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

    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat     = m_pSwapChain->GetDesc().DepthBufferFormat;

    BlendStateDesc BlendDesc;
    BlendDesc.RenderTargets[0].BlendEnable   = true;
    BlendDesc.RenderTargets[0].SrcBlend      = BLEND_FACTOR_SRC_ALPHA;
    BlendDesc.RenderTargets[0].DestBlend     = BLEND_FACTOR_INV_SRC_ALPHA;
    PSOCreateInfo.GraphicsPipeline.BlendDesc = BlendDesc;

    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology                 = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable      = true;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = true;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderFactory;
    ShaderCI.EntryPoint                 = "main";

    RefCntAutoPtr<IShader> pVS, pPS;

    // Vertex Shader
    ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
    ShaderCI.Desc.Name       = "Volume VS";
    ShaderCI.FilePath        = "volume.vsh";
    m_pDevice->CreateShader(ShaderCI, &pVS);

    if (!pVS)
    {
        LOG_ERROR_MESSAGE("FIFO: Error creando el shader de vértices.");
        return;
    }

    // Pixel Shader
    ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
    ShaderCI.Desc.Name       = "Volume PS";
    ShaderCI.FilePath        = "volume.psh";
    m_pDevice->CreateShader(ShaderCI, &pPS);

    if (!pPS)
    {
        LOG_ERROR_MESSAGE("FIFO: Error creando el shader de píxeles.");
        return;
    }

    PSOCreateInfo.pVS                    = pVS;
    PSOCreateInfo.pPS                    = pPS;
    PSOCreateInfo.PSODesc.ResourceLayout = Layout;

    // Create the Pipeline State Object
    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pRenderVolumePSO);

    if (m_pRenderVolumePSO)
    {
        LOG_INFO_MESSAGE("FIFO: PSO de renderizado de volumen creado correctamente.");
        m_pRenderVolumePSO->CreateShaderResourceBinding(&m_pRenderVolumeSRB, true);

        // Bind the velocity texture to the pixel shader
        if (auto* var = m_pRenderVolumeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "VolumeTex"))
            var->Set(m_pVelocityTex[0]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

        // Create and bind the linear sampler to the pixel shader
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
    }
    else
    {
        LOG_ERROR_MESSAGE("FIFO: Error creando el PSO de renderizado de volumen.");
    }

    // Create the camera constant buffer
    BufferDesc CBDesc = {};
    CBDesc.Size = sizeof(float4x4);
    CBDesc.Usage = USAGE_DYNAMIC;
    CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
    CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    CBDesc.Name = "CameraCB";
    m_pDevice->CreateBuffer(CBDesc, nullptr, &m_pCameraCB);

    if (!m_pCameraCB)
    {
        LOG_ERROR_MESSAGE("FIFO: Error creando el buffer de constantes de la cámara.");
    }
    else
    {
        LOG_INFO_MESSAGE("FIFO: Buffer de constantes de la cámara creado correctamente.");
    }
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

    m_Camera.SetRotation(0, 0);
    m_Camera.SetMoveSpeed(5.f);
}

void Tutorial14_ComputeShader::RenderVolume()
{
    // Transition the velocity texture to SHADER_RESOURCE state before binding
    ITextureView* pSRV = m_pVelocityTex[0]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    StateTransitionDesc transitionDesc(
        m_pVelocityTex[0],
        RESOURCE_STATE_UNKNOWN,                // Old state (let engine detect)
        RESOURCE_STATE_SHADER_RESOURCE,        // New state
        STATE_TRANSITION_FLAG_UPDATE_STATE     // Flags
    );
    m_pImmediateContext->TransitionResourceStates(1, &transitionDesc);

    if (auto* var = m_pRenderVolumeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "VolumeTex"))
        var->Set(pSRV);

    m_pImmediateContext->SetPipelineState(m_pRenderVolumePSO);
    m_pImmediateContext->CommitShaderResources(m_pRenderVolumeSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

    DrawAttribs DrawAttrs;
    DrawAttrs.NumVertices = 6; // Fullscreen quad
    DrawAttrs.Flags       = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->Draw(DrawAttrs);

    {
        MapHelper<float4x4> CBData(m_pImmediateContext, m_pCameraCB, MAP_WRITE, MAP_FLAG_DISCARD);
        *CBData = m_ViewProj.Transpose();
    }
    m_pRenderVolumeSRB->GetVariableByName(SHADER_TYPE_VERTEX, "CameraCB")->Set(m_pCameraCB);
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

    UpdateFluidSimulation();
    RenderVolume();
}

void Tutorial14_ComputeShader::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    float4x4 View = m_Camera.GetViewMatrix();
    float4x4 Proj = float4x4::Projection(
        PI_F / 4.0f,
        static_cast<float>(m_pSwapChain->GetDesc().Width) / m_pSwapChain->GetDesc().Height,
        0.1f, 100.f, false
    );
    m_ViewProj = View * Proj;
}

} // namespace Diligent
