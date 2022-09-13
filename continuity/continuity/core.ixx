export module core;

import <utility>;

export namespace continuity
{

template<typename t>
struct tessellatable
{
	template<typename ...args_t>
	auto tessellate(args_t&&... args) const
	{
		return tessellate(*static_cast<const t*>(this), std::forward<args_t>(args)...);
	}
};

}