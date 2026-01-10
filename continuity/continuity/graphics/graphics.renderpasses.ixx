module;

#include "thirdparty/d3dx12.h"

export module graphics:renderpasses;

import vec;
import graphicscore;
import :resourcetypes;

export namespace gfx
{

struct uavbarrierpass
{
	template<typename... args>
	uavbarrierpass(args const&... resources);
	void operator()(deviceresources& devres);

	std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
};

template<typename ...args>
uavbarrierpass::uavbarrierpass(args const& ...resources)
{
	constexpr int num_var_args = sizeof ... (args);
	static_assert(num_var_args > 0);

	// could have done this using tuple, but unfortunately tuple::get<i> doesn't work due to for loops not being constexpr friendly
	resource resources_array[num_var_args] = { resources... };

	barriers.reserve(num_var_args);
	for (auto i : stdx::range(num_var_args)) barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(resources_array[i].d3dresource.Get()));
}

}
