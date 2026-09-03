#ifndef RESERVOIRCPP_TYPING_TUPLE_HPP
#define RESERVOIRCPP_TYPING_TUPLE_HPP

#include <tuple>

namespace reservoircpp {
namespace typing {

template<typename... Types>
using Tuple = std::tuple<Types...>;

} // namespace typing
} // namespace reservoircpp

#endif // RESERVOIRCPP_TYPING_TUPLE_HPP
