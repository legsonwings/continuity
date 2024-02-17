module activesample;

namespace sample_creator
{

template <>
std::unique_ptr<sample_base> create_instance<samples::basic>(view_data const& data)
{
	return std::move(std::make_unique<basic_sample>());
}

}

