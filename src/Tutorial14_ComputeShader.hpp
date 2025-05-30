/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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

#pragma once

#include "SampleBase.hpp"
#include "ResourceMapping.h"
#include "BasicMath.hpp"

namespace Diligent
{

class Tutorial14_ComputeShader final : public SampleBase
{
public:
    virtual void Initialize(const SampleInitInfo& InitInfo) override final;

    virtual void Render() override final;
    virtual void Update(double CurrTime, double ElapsedTime) override final;

    virtual const Char* GetSampleName() const override final { return "Tutorial14: Compute Shader"; }

private:
    void CreateFluidTextures();
    void CreateFluidShaders();
    void CreateShaderResourceBindings();
    void UpdateFluidSimulation(double ElapsedTime);
    void RenderVolume();
    void CreateConsantBuffer();
    void CreateRenderVolumePSO();

    int m_ThreadGroupSize = 256;
    int3 m_GridSize       = {32, 32, 32};

    RefCntAutoPtr<ITexture> m_pVelocityStagingTex;
    
    RefCntAutoPtr<ITexture> m_pVelocityTex[2];
    RefCntAutoPtr<ITexture> m_pPressureTex[2];
    RefCntAutoPtr<ITexture> m_pDivergenceTex;

    RefCntAutoPtr<IPipelineState> m_pAdvectPSO;
    RefCntAutoPtr<IPipelineState> m_pForcePSO;
    RefCntAutoPtr<IPipelineState> m_pDivergencePSO;
    RefCntAutoPtr<IPipelineState> m_pJacobiPSO;
    RefCntAutoPtr<IPipelineState> m_pProjectPSO;

    RefCntAutoPtr<IShaderResourceBinding> m_pAdvectSRB;
    RefCntAutoPtr<IShaderResourceBinding> m_pForceSRB;
    RefCntAutoPtr<IShaderResourceBinding> m_pDivergenceSRB;
    RefCntAutoPtr<IShaderResourceBinding> m_pJacobiSRB;
    RefCntAutoPtr<IShaderResourceBinding> m_pProjectSRB;

    RefCntAutoPtr<IPipelineState>         m_pRenderVolumePSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pRenderVolumeSRB;

    RefCntAutoPtr<IBuffer> m_pConstantsAdvectCB;
    RefCntAutoPtr<IBuffer> m_pConstantsForcesCB;

    bool m_InjectVelocity = false;
    float4 m_CustomVelocity = float4{0, 100, 0, 1};
    void RenderUI();
};

} // namespace Diligent
