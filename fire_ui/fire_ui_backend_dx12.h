
typedef struct UI_DX12_State {
	ID3D12Device* device;
	D3D12_CPU_DESCRIPTOR_HANDLE atlas_cpu_descriptor;
	D3D12_GPU_DESCRIPTOR_HANDLE atlas_gpu_descriptor;

	ID3D12RootSignature* root_signature;
	ID3D12PipelineState* pipeline_state;

	ID3D12Resource* atlases[2];
	ID3D12Resource* atlases_staging[2];
	uint32_t atlases_width[2];
	uint32_t atlases_height[2];
	bool atlas_is_mapped[2];

	ID3D12Resource* buffers[UI_MAX_BACKEND_BUFFERS];
	uint32_t buffer_sizes[UI_MAX_BACKEND_BUFFERS];
	bool buffer_is_mapped[UI_MAX_BACKEND_BUFFERS];
} UI_DX12_State;

static UI_DX12_State UI_DX12_STATE;

static UI_TextureID UI_DX12_CreateAtlas(int atlas_id, uint32_t width, uint32_t height) {
	// Create the GPU-local texture
	ID3D12Resource* texture;
	{
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Alignment = 0;
		desc.Width = width;
		desc.Height = height;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		
		D3D12_HEAP_PROPERTIES props = {};
		props.Type = D3D12_HEAP_TYPE_DEFAULT;
		props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		bool ok = UI_DX12_STATE.device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&texture)) == S_OK;
		UI_ASSERT(ok);
		UI_DX12_STATE.atlases[atlas_id] = texture;
		UI_DX12_STATE.atlases_width[atlas_id] = width;
		UI_DX12_STATE.atlases_height[atlas_id] = height;
	}

	// Create the staging buffer
	{
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment = 0;
		desc.Width = height * (width * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_HEAP_PROPERTIES props = {};
		props.Type = D3D12_HEAP_TYPE_UPLOAD;
		props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		bool ok = UI_DX12_STATE.device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&UI_DX12_STATE.atlases_staging[atlas_id])) == S_OK;
		UI_ASSERT(ok);
	}

	// Create texture view
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipLevels = 1;
		
		UI_ASSERT(atlas_id == 0); // TODO: support for multiple atlases / dynamic atlas growth
		UI_DX12_STATE.device->CreateShaderResourceView(texture, &desc, UI_DX12_STATE.atlas_cpu_descriptor);
	}

	return (UI_TextureID)UI_DX12_STATE.atlas_gpu_descriptor.ptr;
}

static void UI_DX12_DestroyAtlas(int atlas_id) {
	__debugbreak(); //TODO
}

static void* UI_DX12_AtlasMapUntilDraw(int atlas_id) {
	void* ptr;
	D3D12_RANGE read_range = {}; // We do not intend to read from this resource on the CPU.
	bool ok = UI_DX12_STATE.atlases_staging[atlas_id]->Map(0, &read_range, &ptr) == S_OK;
	UI_ASSERT(ok);
	UI_DX12_STATE.atlas_is_mapped[atlas_id] = true;
	return ptr;
}

static void* UI_DX12_BufferMapUntilDraw(int buffer_id) {
	void* ptr;
	D3D12_RANGE read_range = {}; // We do not intend to read from this resource on the CPU.
	bool ok = UI_DX12_STATE.buffers[buffer_id]->Map(0, &read_range, &ptr) == S_OK;
	UI_ASSERT(ok);
	UI_DX12_STATE.buffer_is_mapped[buffer_id] = true;
	return ptr;
}

static void UI_DX12_CreateBuffer(int buffer_id, uint32_t size_in_bytes) {
	D3D12_HEAP_PROPERTIES heap_props = {};
	heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
	heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = size_in_bytes;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	bool ok = UI_DX12_STATE.device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
		IID_PPV_ARGS(&UI_DX12_STATE.buffers[buffer_id])) == S_OK;
	UI_DX12_STATE.buffer_is_mapped[buffer_id] = false;
	UI_DX12_STATE.buffer_sizes[buffer_id] = size_in_bytes;
	UI_ASSERT(ok);
}

static void UI_DX12_CreateVertexBuffer(int buffer_id, uint32_t size_in_bytes) {
	UI_DX12_CreateBuffer(buffer_id, size_in_bytes);
}

static void UI_DX12_CreateIndexBuffer(int buffer_id, uint32_t size_in_bytes) {
	UI_DX12_CreateBuffer(buffer_id, size_in_bytes);
}

static void UI_DX12_DestroyBuffer(int buffer_id) {
	__debugbreak(); //TODO
}

static void UI_DX12_Init(UI_Backend* backend, ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE atlas_cpu_descriptor, D3D12_GPU_DESCRIPTOR_HANDLE atlas_gpu_descriptor) {
	memset(&UI_DX12_STATE, 0, sizeof(UI_DX12_STATE));
	UI_DX12_STATE.device = device;
	UI_DX12_STATE.atlas_cpu_descriptor = atlas_cpu_descriptor;
	UI_DX12_STATE.atlas_gpu_descriptor = atlas_gpu_descriptor;

	backend->create_vertex_buffer = UI_DX12_CreateVertexBuffer;
	backend->create_index_buffer = UI_DX12_CreateIndexBuffer;
	backend->destroy_buffer = UI_DX12_DestroyBuffer;

	backend->create_atlas = UI_DX12_CreateAtlas;
	backend->destroy_atlas = UI_DX12_DestroyAtlas;

	backend->buffer_map_until_draw = UI_DX12_BufferMapUntilDraw;
	backend->atlas_map_until_draw = UI_DX12_AtlasMapUntilDraw;

	// Create the root signature
	{
		D3D12_DESCRIPTOR_RANGE descriptor_range = {};
		descriptor_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descriptor_range.NumDescriptors = 1;
		descriptor_range.BaseShaderRegister = 0;
		descriptor_range.RegisterSpace = 0;
		descriptor_range.OffsetInDescriptorsFromTableStart = 0;

		D3D12_ROOT_PARAMETER root_params[2] = {};

		root_params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		root_params[0].Constants.ShaderRegister = 0;
		root_params[0].Constants.RegisterSpace = 0;
		root_params[0].Constants.Num32BitValues = 16;
		root_params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		root_params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		root_params[1].DescriptorTable.NumDescriptorRanges = 1;
		root_params[1].DescriptorTable.pDescriptorRanges = &descriptor_range;
		root_params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_STATIC_SAMPLER_DESC sampler = {};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.pParameters = root_params;
		desc.NumParameters = DS_ArrayCount(root_params);
		desc.pStaticSamplers = &sampler;
		desc.NumStaticSamplers = 1;
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		ID3DBlob* signature = NULL;
		bool ok = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, NULL) == S_OK;
		UI_ASSERT(ok);

		ok = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&UI_DX12_STATE.root_signature)) == S_OK;
		UI_ASSERT(ok);

		signature->Release();
	}

	// Create the pipeline
	{
		ID3DBlob* vertex_shader;
		ID3DBlob* pixel_shader;

#ifdef UI_DX12_DEBUG_MODE
		UINT compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compile_flags = 0;
#endif

		static const char shader_src[] = "\
		cbuffer Vertex32BitConstants : register(b0)\
		{\
			float4x4 projection_matrix;\
		};\
		\
		struct PSInput\
		{\
			float4 position : SV_POSITION;\
			float2 uv       : TEXCOORD0;\
			float4 color    : TEXCOORD1;\
		};\
		\
		PSInput VSMain(float2 position : POSITION, float2 uv : UV, float4 color : COLOR)\
		{\
			PSInput result;\
			result.position = mul(projection_matrix, float4(position, 0.f, 1.f));\
			result.uv = uv;\
			result.color = color;\
			return result;\
		}\
		\
		SamplerState sampler0 : register(s0);\
        Texture2D texture0 : register(t0);\
		\
		float4 PSMain(PSInput input) : SV_TARGET\
		{\
			return input.color * texture0.Sample(sampler0, input.uv);\
		}";

		bool ok = D3DCompile(shader_src, sizeof(shader_src), NULL, NULL, NULL, "VSMain", "vs_5_0", compile_flags, 0, &vertex_shader, NULL) == S_OK;
		UI_ASSERT(ok);

		ok = D3DCompile(shader_src, sizeof(shader_src), NULL, NULL, NULL, "PSMain", "ps_5_0", compile_flags, 0, &pixel_shader, NULL) == S_OK;
		UI_ASSERT(ok);

		D3D12_INPUT_ELEMENT_DESC input_elem_desc[] = {
			{ "POSITION", 0,   DXGI_FORMAT_R32G32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{       "UV", 0,   DXGI_FORMAT_R32G32_FLOAT, 0,  8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{    "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};

		pso_desc.InputLayout = { input_elem_desc, _countof(input_elem_desc) };
		pso_desc.pRootSignature = UI_DX12_STATE.root_signature;
		pso_desc.VS = { vertex_shader->GetBufferPointer(), vertex_shader->GetBufferSize() };
		pso_desc.PS = { pixel_shader->GetBufferPointer(), pixel_shader->GetBufferSize() };

		pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		pso_desc.RasterizerState.FrontCounterClockwise = false;
		pso_desc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		pso_desc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		pso_desc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		pso_desc.RasterizerState.DepthClipEnable = true;
		pso_desc.RasterizerState.MultisampleEnable = false;
		pso_desc.RasterizerState.AntialiasedLineEnable = false;
		pso_desc.RasterizerState.ForcedSampleCount = 0;
		pso_desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		pso_desc.BlendState.AlphaToCoverageEnable = false;
		pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
		pso_desc.BlendState.RenderTarget[0].SrcBlend              = D3D12_BLEND_SRC_ALPHA;
		pso_desc.BlendState.RenderTarget[0].DestBlend             = D3D12_BLEND_INV_SRC_ALPHA;
		pso_desc.BlendState.RenderTarget[0].BlendOp               = D3D12_BLEND_OP_ADD;
		pso_desc.BlendState.RenderTarget[0].SrcBlendAlpha         = D3D12_BLEND_ONE;
		pso_desc.BlendState.RenderTarget[0].DestBlendAlpha        = D3D12_BLEND_INV_SRC_ALPHA;
		pso_desc.BlendState.RenderTarget[0].BlendOpAlpha          = D3D12_BLEND_OP_ADD;
		pso_desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		pso_desc.DepthStencilState.DepthEnable = false;
		pso_desc.DepthStencilState.StencilEnable = false;
		pso_desc.SampleMask = UINT_MAX;
		pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pso_desc.NumRenderTargets = 1;
		pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		pso_desc.SampleDesc.Count = 1;

		ok = device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&UI_DX12_STATE.pipeline_state)) == S_OK;
		UI_ASSERT(ok);
	}

}

static void UI_DX12_Deinit(void) {
	__debugbreak(); //TODO
}

// The following graphics state is expected to be already set, and will not be modified:
// - Primitive topology (must be D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
// - Viewport
// - Scissor
// - Render target
// - SRV descriptor heap
// The following graphics state will be overwritten:
// - Pipeline state
// - Graphics root signature
// - Graphics root descriptor table
// - Vertex buffer
// - Index buffer
static void UI_DX12_Draw(UI_Outputs* outputs, ID3D12GraphicsCommandList* command_list) {
	UI_DX12_State* s = &UI_DX12_STATE;

	for (int i = 0; i < 2; i++) {
		if (UI_DX12_STATE.atlas_is_mapped[i]) {
			ID3D12Resource* staging = UI_DX12_STATE.atlases_staging[i];
			ID3D12Resource* dst = UI_DX12_STATE.atlases[i];
			staging->Unmap(0, NULL);
		
			uint32_t width = UI_DX12_STATE.atlases_width[i];
			uint32_t height = UI_DX12_STATE.atlases_height[i];
			uint32_t row_pitch = (width * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
			
			D3D12_TEXTURE_COPY_LOCATION src_loc = {};
			src_loc.pResource = staging;
			src_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			src_loc.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			src_loc.PlacedFootprint.Footprint.Width = width;
			src_loc.PlacedFootprint.Footprint.Height = height;
			src_loc.PlacedFootprint.Footprint.Depth = 1;
			src_loc.PlacedFootprint.Footprint.RowPitch = row_pitch;

			D3D12_TEXTURE_COPY_LOCATION dst_loc = {};
			dst_loc.pResource = dst;
			dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dst_loc.SubresourceIndex = 0;
			
			command_list->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, NULL);
			
			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource   = dst;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			command_list->ResourceBarrier(1, &barrier);

			UI_DX12_STATE.atlas_is_mapped[i] = false;
		}
	}
	
	for (int i = 0; i < UI_MAX_BACKEND_BUFFERS; i++) {
		if (s->buffer_is_mapped[i]) {
			ID3D12Resource* buffer = s->buffers[i];
			buffer->Unmap(0, NULL);
			s->buffer_is_mapped[i] = false;
		}
	}

	float L = 0.f;
	float R = UI_STATE.window_size.x;
	float T = 0.f;
	float B = UI_STATE.window_size.y;
	float vertex_32bit_constants[4][4] = {
		{ 2.0f/(R-L),  0.0f,        0.0f, 0.0f },
		{ 0.0f,        2.0f/(T-B),  0.0f, 0.0f },
		{ 0.0f,        0.0f,        0.5f, 0.0f },
		{ (R+L)/(L-R), (T+B)/(B-T), 0.5f, 1.0f },
	};

	command_list->SetPipelineState(UI_DX12_STATE.pipeline_state);
	command_list->SetGraphicsRootSignature(UI_DX12_STATE.root_signature);
	command_list->SetGraphicsRoot32BitConstants(0, 16, vertex_32bit_constants, 0);

	int bound_vertex_buffer = -1;
	int bound_index_buffer = -1;

	for (int i = 0; i < outputs->draw_calls_count; i++) {
		UI_DrawCall* draw_call = &outputs->draw_calls[i];

		if (draw_call->vertex_buffer_id != bound_vertex_buffer) {
			bound_vertex_buffer = draw_call->vertex_buffer_id;
			D3D12_VERTEX_BUFFER_VIEW view;
			view.BufferLocation = s->buffers[bound_vertex_buffer]->GetGPUVirtualAddress();
			view.StrideInBytes = 2 * sizeof(float) + 2 * sizeof(float) + 4; // pos, uv, color
			view.SizeInBytes = s->buffer_sizes[bound_vertex_buffer];
			command_list->IASetVertexBuffers(0, 1, &view);
		}

		if (draw_call->index_buffer_id != bound_index_buffer) {
			bound_index_buffer = draw_call->index_buffer_id;
			D3D12_INDEX_BUFFER_VIEW view;
			view.BufferLocation = s->buffers[bound_index_buffer]->GetGPUVirtualAddress();
			view.SizeInBytes = s->buffer_sizes[bound_index_buffer];
			view.Format = DXGI_FORMAT_R32_UINT;
			command_list->IASetIndexBuffer(&view);
		}
		
		UI_ASSERT(draw_call->texture);

		D3D12_GPU_DESCRIPTOR_HANDLE handle = {};
		handle.ptr = (UINT64)draw_call->texture;
		command_list->SetGraphicsRootDescriptorTable(1, handle);
		command_list->DrawIndexedInstanced(draw_call->index_count, 1, draw_call->first_index, 0, 0);
	}
}
