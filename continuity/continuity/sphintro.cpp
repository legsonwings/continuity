module sphintro;

namespace sample_creator
{

template <>
std::unique_ptr<sample_base> create_instance<samples::sphintro>(view_data const& data)
{
	return std::move(std::make_unique<sphintro::sphfluidintro>());
}

}