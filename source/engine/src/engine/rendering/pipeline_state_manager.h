#pragma once
#include "rendering/pipeline_state.h"

namespace my {

class PipelineStateManager {
public:
    virtual ~PipelineStateManager() = default;

    bool Initialize();
    void Finalize();

    PipelineState* Find(PipelineStateName p_name);

protected:
    virtual std::shared_ptr<PipelineState> CreateInternal(const PipelineStateDesc& p_info) = 0;

private:
    bool Create(PipelineStateName p_name, const PipelineStateDesc& p_info);

    std::array<std::shared_ptr<PipelineState>, PSO_MAX> m_cache;
};

}  // namespace my