#pragma once

#if defined(_MSC_VER)
// disable C4996 issued by std::copy_n
#pragma warning( disable : 4996 ) 
#endif

#include <cassert>
#include <algorithm>
#include <array>
#include <complex>
#include <exception>
#include <initializer_list>
#include <iterator>
#include <numeric>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "WolframLibrary.h"
#include "WolframSparseLibrary.h"


namespace wll
{


#if defined(__GNUC__)
static_assert(__GNUC__ >= 7);
#define WLL_CURRENT_FUNCTION std::string(__PRETTY_FUNCTION__)
#elif defined(__INTEL_COMPILER)
static_assert(__INTEL_COMPILER >= 1900);
#define WLL_CURRENT_FUNCTION std::string(__FUNCTION__)
#elif defined(__clang__)
static_assert(__clang_major__ >= 4);
#define WLL_CURRENT_FUNCTION std::string(__PRETTY_FUNCTION__)
#elif defined(_MSC_VER)
static_assert(_MSC_VER >= 1911);
#define WLL_CURRENT_FUNCTION std::string(__FUNCSIG__)
#endif

static_assert(sizeof(mint) == sizeof(size_t));


struct exception_status
{
    int error_type_ = LIBRARY_NO_ERROR;
    std::string message_{};
};

struct library_error
{
    explicit library_error(int type, std::string message ={}) :
        type_{type}, message_{std::move(message)} {}

    [[nodiscard]] int type() const { return type_; }
    [[nodiscard]] std::string what() const { return message_; }

    int type_;
    std::string message_;
};
struct library_type_error : library_error
{
    explicit library_type_error(std::string message ={}) :
        library_error(LIBRARY_TYPE_ERROR, std::move(message)) {}
};
struct library_rank_error : library_error
{
    explicit library_rank_error(std::string message ={}) :
        library_error(LIBRARY_RANK_ERROR, std::move(message)) {}
};
struct library_dimension_error : library_error
{
    explicit library_dimension_error(std::string message ={}) :
        library_error(LIBRARY_DIMENSION_ERROR, std::move(message)) {}
};
struct library_numerical_error : library_error
{
    explicit library_numerical_error(std::string message ={}) :
        library_error(LIBRARY_NUMERICAL_ERROR, std::move(message)) {}
};
struct library_memory_error : library_error
{
    explicit library_memory_error(std::string message ={}) :
        library_error(LIBRARY_MEMORY_ERROR, std::move(message)) {}
};
struct library_function_error : library_error
{
    explicit library_function_error(std::string message ={}) :
        library_error(LIBRARY_FUNCTION_ERROR, std::move(message)) {}
};

struct log_stringstream_t
{
    std::stringstream ss_{};
    mutable std::string string_{};

    void update_string() const
    {
        string_ = ss_.str();
    }
    void clear()
    {
        ss_ = std::stringstream{};
        string_ = std::string{};
    }
    template<typename Any>
    auto& operator<<(Any&& any)
    {
        return ss_ << std::forward<Any>(any);
    }
};


WolframLibraryData global_lib_data;
using sparse_fn_lib_t = decltype(global_lib_data->sparseLibraryFunctions);
sparse_fn_lib_t    global_sparse_fn;

exception_status   global_exception;
log_stringstream_t global_log;
std::string        global_string_result;


#ifdef NDEBUG
#define WLL_DEBUG_EXECUTE(expr) ((void)0)
#define WLL_ASSERT(expr) ((void)0)
#else
#define WLL_DEBUG_EXECUTE(expr) (any)
#define WLL_ASSERT(expr) assert((expr))
#endif


template<typename LinkType, typename UserType>
struct is_same_layout :
    std::false_type {};
template<typename UserType>
struct is_same_layout<mint, UserType> :
    std::bool_constant<std::is_integral_v<UserType> && sizeof(mint) == sizeof(UserType)> {};
template<>
struct is_same_layout<mreal, double> :
    std::true_type {};
template<>
struct is_same_layout<mcomplex, std::complex<double>> :
    std::true_type {};
template<typename LinkType, typename UserType>
constexpr bool is_same_layout_v = is_same_layout<LinkType, UserType>::value;

template<typename Complex>
struct is_std_complex :
    std::false_type {};
template<typename T>
struct is_std_complex<std::complex<T>> :
    std::true_type {};
template<typename Complex>
constexpr bool is_std_complex_v = is_std_complex<Complex>::value;

template<typename Complex>
struct complex_value
{
    using type = void;
};
template<typename T>
struct complex_value<std::complex<T>>
{
    using type = T;
};
template<typename Complex>
using complex_value_t = typename complex_value<Complex>::type;

template<typename>
constexpr bool _always_false_v = false;


template<size_t Size>
size_t _flattened_size(const std::array<size_t, Size>& dims) noexcept
{
    size_t size = 1;
    for (size_t d : dims) size *= d;
    return size;
}

template<size_t Rank, typename T>
std::array<size_t, Rank> _convert_to_dims_array(std::initializer_list<T> dims)
{
    static_assert(std::is_integral_v<T>, "dimensions should be of integral types");
    WLL_ASSERT(dims.size() == Rank);   // dims should have size equal to the rank
    std::array<size_t, Rank> dims_array;
    std::copy_n(dims.begin(), Rank, dims_array.begin());
    return dims_array;
}

template<typename X, typename Y>
auto _add_if_negative(X val_x, Y val_y)
{
    if constexpr (std::is_signed_v<X>)
        if (val_x < X(0))
            return Y(val_x) + val_y;
    return Y(val_x);
}


template<typename Target>
struct _mtype_cast_impl
{
    template<typename Value>
    constexpr Target operator()(const Value& value) noexcept
    {
        return static_cast<Target>(value);
    }

    constexpr Target operator()(const mcomplex& value)
    {
        throw library_type_error(WLL_CURRENT_FUNCTION + "\ncannot convert from mcomplex");
        return Target{};
    }

    template<typename T>
    constexpr Target operator()(const std::complex<T>& value)
    {
        throw library_type_error(WLL_CURRENT_FUNCTION + "\ncannot convert from std::complex<T>");
        return Target{};
    }
};
template<typename T>
struct _mtype_cast_impl<std::complex<T>>
{
    template<typename Value>
    constexpr std::complex<T> operator()(const Value& value) noexcept
    {
        return std::complex<T>(static_cast<T>(value));
    }

    constexpr std::complex<T> operator()(const mcomplex& value) noexcept
    {
        return std::complex<T>(static_cast<T>(mcreal(value)),
                               static_cast<T>(mcimag(value)));
    }

    template<typename U>
    constexpr std::complex<T> operator()(const std::complex<U>& value) noexcept
    {
        return std::complex<T>(static_cast<T>(value.real()),
                               static_cast<T>(value.imag()));
    }
};
template<>
struct _mtype_cast_impl<mcomplex>
{
    template<typename Value>
    constexpr mcomplex operator()(const Value& value) noexcept
    {
        mcomplex ret;
        mcreal(ret) = static_cast<mreal>(value);
        mcimag(ret) = mreal{};
        return ret;
    }

    constexpr mcomplex operator()(const mcomplex& value) noexcept
    {
        return value;
    }

    template<typename T>
    constexpr mcomplex operator()(const std::complex<T>& value) noexcept
    {
        mcomplex ret;
        mcreal(ret) = static_cast<mreal>(value.real());
        mcimag(ret) = static_cast<mreal>(value.imag());
        return ret;
    }
};

template<typename Target, typename Value>
constexpr Target _mtype_cast(const Value& value)
{
    return _mtype_cast_impl<Target>()(value);
}

bool operator==(const mcomplex& a, const mcomplex& b)
{
    return (mcreal(a) == mcreal(b)) && (mcimag(a) == mcimag(b));
}
bool operator!=(const mcomplex& a, const mcomplex& b)
{
    return !(a == b);
}

template<typename SrcType, typename DestType>
inline void _data_copy_n(const SrcType* src_ptr, size_t count, DestType* dest_ptr)
{
    if (count > 0)
    {
        WLL_ASSERT(src_ptr != nullptr && dest_ptr != nullptr);
        WLL_ASSERT((void*)src_ptr != (void*)dest_ptr); // cannot copy to itself
        for (size_t i = 0; i < count; ++i)
            dest_ptr[i] = _mtype_cast<DestType>(src_ptr[i]);
    }
}

template<size_t Rank>
struct _index_array
{
    using _idx_t = std::array<size_t, Rank>;

    explicit _index_array(const _idx_t& idx) noexcept : idx_{idx} {}

    template<typename Value>
    std::pair<_idx_t, Value> operator=(const Value& value) const noexcept
    {
        return {idx_, value};
    }

    _idx_t idx_;
};

template<typename... Indices>
_index_array<sizeof...(Indices)> pos(const Indices&... indices)
{
    return std::array<size_t, sizeof...(Indices)>({size_t(indices)...});
}


#define MType_Void -1

template<typename T>
struct derive_tensor_data_type
{
    using strict_type =
        std::conditional_t<is_same_layout_v<mint, T>, mint,
        std::conditional_t<is_same_layout_v<mreal, T>, mreal,
        std::conditional_t<is_same_layout_v<mcomplex, T>, mcomplex, void>>>;
    using convert_type =
        std::conditional_t<std::is_integral_v<T>, mint,
        std::conditional_t<std::is_floating_point_v<T>, mreal,
        std::conditional_t<is_std_complex_v<T>, mcomplex, void>>>;
    static constexpr int strict_type_v =
        std::is_same_v<strict_type, mint> ? MType_Integer :
        std::is_same_v<strict_type, mreal> ? MType_Real :
        std::is_same_v<strict_type, mcomplex> ? MType_Complex : MType_Void;
    static constexpr int convert_type_v =
        std::is_same_v<convert_type, mint> ? MType_Integer :
        std::is_same_v<convert_type, mreal> ? MType_Real :
        std::is_same_v<convert_type, mcomplex> ? MType_Complex : MType_Void;
};

enum memory_type
{
    // who has resource    memory managed by
    empty,  // -                   -
    owned,  // wll::tensor         malloc/free
    proxy,  // kernel              -
    manual, // wll::tensor         MTensor_new/MTensor_free
    shared  // kernel/wll::tensor  MTensor_disown
};


template<typename T, size_t Rank>
struct tensor_init_data
{
    using type = std::initializer_list<typename tensor_init_data<T, Rank - 1>::type>;
};
template<typename T>
struct tensor_init_data<T, 0>
{
    using type = T;
};
template<typename T, size_t Rank>
using tensor_init_data_t = typename tensor_init_data<T, Rank>::type;


template<typename T, size_t Rank>
class tensor
{
public:
    using value_type   = T;
    static constexpr size_t _rank = Rank;
    using _ptr_t       = value_type*;
    using _const_ptr_t = const value_type*;
    using _dims_t      = std::array<size_t, _rank>;
    using _init_dims_t = std::initializer_list<size_t>;
    using _init_data_t = tensor_init_data_t<value_type, _rank>;
    static_assert(_rank > 0);

    template<typename U, size_t URank>
    friend class tensor;

    tensor() noexcept = default;

    tensor(MTensor mtensor, memory_type access) :
        mtensor_{mtensor}, access_{access}
    {
        auto mtensor_rank = global_lib_data->MTensor_getRank(mtensor);
        WLL_ASSERT(_rank == mtensor_rank);
        WLL_ASSERT(access_ == memory_type::owned ||
                   access_ == memory_type::proxy ||
                   access_ == memory_type::shared);

        const mint* dims_ptr = global_lib_data->MTensor_getDimensions(mtensor);
        std::copy_n(dims_ptr, _rank, dims_.begin());
        size_ = global_lib_data->MTensor_getFlattenedLength(mtensor);

        void* src_ptr = nullptr;
        bool  do_copy = false;
        int   mtype   = int(global_lib_data->MTensor_getType(mtensor));
        WLL_ASSERT(mtype == MType_Integer || mtype == MType_Real || mtype == MType_Complex);

        if (mtype == MType_Integer)
        {
            src_ptr = reinterpret_cast<void*>(global_lib_data->MTensor_getIntegerData(mtensor));
            do_copy = !is_same_layout_v<mint, value_type>;
        }
        else if (mtype == MType_Real)
        {
            src_ptr = reinterpret_cast<void*>(global_lib_data->MTensor_getRealData(mtensor));
            do_copy = !is_same_layout_v<mreal, value_type>;
        }
        else // mtype == MType_Complex
        {
            src_ptr = reinterpret_cast<void*>(global_lib_data->MTensor_getComplexData(mtensor));
            do_copy = !is_same_layout_v<mcomplex, value_type>;
        }

        if (access_ == memory_type::owned)
            do_copy = true; // do copy anyway

        if (do_copy)
        {
            WLL_ASSERT(access_ == memory_type::owned ||
                       access_ == memory_type::proxy);
            mtensor_ = nullptr;
            access_  = memory_type::owned; // *this will own data after copy
            ptr_ = reinterpret_cast<_ptr_t>(malloc(size_ * sizeof(value_type)));
            if (ptr_ == nullptr)
                throw library_memory_error(WLL_CURRENT_FUNCTION + "\nmalloc failed when copying data.");
            if (mtype == MType_Integer)
                _data_copy_n(reinterpret_cast<mint*>(src_ptr), size_, ptr_);
            else if (mtype == MType_Real)
                _data_copy_n(reinterpret_cast<mreal*>(src_ptr), size_, ptr_);
            else // mtype == MType_Complex
                _data_copy_n(reinterpret_cast<mcomplex*>(src_ptr), size_, ptr_);
        }
        else // do_copy == false
        {
            WLL_ASSERT(access_ == memory_type::proxy ||
                       access_ == memory_type::shared);
            ptr_ = reinterpret_cast<_ptr_t>(src_ptr);
        }
    }

    explicit tensor(_dims_t dims, memory_type access = memory_type::owned) :
        dims_{dims}, size_{_flattened_size(dims)}, access_{access}
    {
        WLL_ASSERT(access_ == memory_type::owned ||
                   access_ == memory_type::manual);
        if (access_ == memory_type::owned)
        {
            ptr_ = reinterpret_cast<_ptr_t>(calloc(size_, sizeof(value_type)));
            if (ptr_ == nullptr)
                throw library_memory_error(WLL_CURRENT_FUNCTION + "\ncalloc failed, access_ == owned.");
        }
        else // access_ == memory_type::manual
        {
            //throw library_dimension_error(std::to_string(dims[0]));
            constexpr int mtype = derive_tensor_data_type<value_type>::strict_type_v;
            if (mtype == MType_Void)
                throw library_type_error(WLL_CURRENT_FUNCTION + "\nvalue_type cannot be strictly matched to any MType.");
            int err = global_lib_data->MTensor_new(
                mtype, _rank, reinterpret_cast<mint*>(dims_.data()), &mtensor_);
            if (err != LIBRARY_NO_ERROR)
                throw library_error(err, WLL_CURRENT_FUNCTION + "\nMTensor_new() failed.");
            if constexpr (mtype == MType_Integer)
                ptr_ = reinterpret_cast<_ptr_t>(global_lib_data->MTensor_getIntegerData(mtensor_));
            else if constexpr (mtype == MType_Real)
                ptr_ = reinterpret_cast<_ptr_t>(global_lib_data->MTensor_getRealData(mtensor_));
            else // mtype == MType_Complex
                ptr_ = reinterpret_cast<_ptr_t>(global_lib_data->MTensor_getComplexData(mtensor_));
        }
    }

    tensor(_init_dims_t dims, memory_type access = memory_type::owned) :
        tensor(_convert_to_dims_array<_rank>(dims), access) {}

    tensor(_dims_t dims, const _init_data_t& data, memory_type access = memory_type::owned) :
        tensor(dims, access)
    {
        this->_fill_init_data(data);
    }

    tensor(_init_dims_t dims, const _init_data_t& data, memory_type access = memory_type::owned) :
        tensor(_get_init_dims(dims, data), data, access)
    {
        this->_fill_init_data(data);
    }

    tensor(const tensor& other) :
        dims_{other.dims_}, size_{other.size_}, access_{memory_type::owned}
    {
        ptr_ = reinterpret_cast<_ptr_t>(malloc(size_ * sizeof(value_type)));
        if (ptr_ == nullptr)
            throw library_memory_error(WLL_CURRENT_FUNCTION + "\nmalloc failed when copying data.");
        _data_copy_n(other.ptr_, size_, ptr_);
    }

    tensor(tensor&& other) noexcept :
        dims_{other.dims_}, size_{other.size_}
    {
        std::swap(ptr_, other.ptr_);
        std::swap(access_, other.access_);
        std::swap(mtensor_, other.mtensor_);
    }

    template<typename U>
    explicit tensor(const tensor<U, _rank>& other) :
        dims_{other.dims_}, size_{other.size_}, access_{memory_type::owned}
    {
        ptr_ = reinterpret_cast<_ptr_t>(malloc(size_ * sizeof(value_type)));
        if (ptr_ == nullptr)
            throw library_memory_error(WLL_CURRENT_FUNCTION + "\nmalloc failed when copying data.");
        _data_copy_n(other.ptr_, size_, ptr_);
    }

    template<typename U>
    explicit tensor(tensor<U, _rank>&& other) :
        dims_{other.dims_}, size_{other.size_}, access_{memory_type::owned}
    {
        ptr_ = reinterpret_cast<_ptr_t>(malloc(size_ * sizeof(value_type)));
        if (ptr_ == nullptr)
            throw library_memory_error(WLL_CURRENT_FUNCTION + "\nmalloc failed when copying data.");
        _data_copy_n(other.ptr_, size_, ptr_);
    }

    tensor& operator=(const tensor& other)
    {
        if(this == &other) return *this;
        WLL_ASSERT(other.access_ != memory_type::empty); // other is empty
        WLL_ASSERT(this->access_ != memory_type::empty); // *this is empty
        if (this->ptr_ != other.ptr_)
        {
            if (!(this->_has_same_dims(other.dims_.data())))
                throw library_dimension_error(WLL_CURRENT_FUNCTION + "\ntensors have different dimensions.");
            _data_copy_n(other.ptr_, size_, ptr_);
        }
        return *this;
    }

    tensor& operator=(tensor&& other)
    {
        if(this == &other) return *this;
        WLL_ASSERT(other.access_ != memory_type::empty); // other is empty
        WLL_ASSERT(this->access_ != memory_type::empty); // *this is empty
        if (this->ptr_ != other.ptr_)
        {
            if (!(this->_has_same_dims(other.dims_.data())))
                throw library_dimension_error(WLL_CURRENT_FUNCTION + "\ntensors have different dimensions.");
            if (other.access_ == memory_type::proxy  ||
                other.access_ == memory_type::shared ||
                this->access_ == memory_type::proxy  ||
                this->access_ == memory_type::shared)
            {
                _data_copy_n(other.ptr_, size_, ptr_);
            }
            else
            {
                this->_swap_pointers(std::move(other));
            }
        }
        return *this;
    }

    template<typename U>
    tensor& operator=(const tensor<U, _rank>& other)
    {
        if(this == &other) return *this;
        WLL_ASSERT(other.access_ != memory_type::empty); // other is empty
        WLL_ASSERT(this->access_ != memory_type::empty); // *this is empty
        // tensors with difference value_type should not have the same ptr_
        WLL_ASSERT((void*)(this->ptr_) != (void*)(other.ptr_));
        _data_copy_n(other.ptr_, size_, ptr_);
        return *this;
    }

    template<typename U>
    tensor& operator=(tensor<U, _rank>&& other)
    {
        WLL_ASSERT(other.access_ != memory_type::empty); // other is empty
        WLL_ASSERT(this->access_ != memory_type::empty); // *this is empty
        // tensors with difference value_type should not have the same ptr_
        WLL_ASSERT((void*)(this->ptr_) != (void*)(other.ptr_));
        _data_copy_n(other.ptr_, size_, ptr_);
        return *this;
    }

    [[nodiscard]] tensor clone(memory_type access = memory_type::owned) const
    {
        WLL_ASSERT(this->access_ != memory_type::empty); // cannot clone an empty tensor
        WLL_ASSERT(access == memory_type::owned || access == memory_type::manual);
        tensor ret(this->dims_, access);
        _data_copy_n(ptr_, size_, ret.ptr_);
        return ret;
    }

    ~tensor()
    {
        this->_destroy();
    }

    [[nodiscard]] constexpr size_t rank() const noexcept
    {
        return _rank;
    }

    [[nodiscard]] size_t size() const noexcept
    {
        return size_;
    }

    [[nodiscard]] _dims_t dimensions() const noexcept
    {
        return dims_;
    }

    [[nodiscard]] size_t dimension(size_t level) const noexcept
    {
        return dims_[level];
    }

    _ptr_t data() noexcept
    {
        return this->ptr_;
    }

    [[nodiscard]] _const_ptr_t data() const noexcept
    {
        return this->ptr_;
    }

    bool operator==(const tensor& other) const
    {
        if (this->dims_ != other.dims_)
            return false;
        if (this->ptr_ == other.ptr_)
            return true;
        return std::equal(this->cbegin(), this->cend(), other.cbegin());
    }

    template<typename... Idx>
    value_type at(Idx... idx) const
    {
        static_assert(sizeof...(idx) == _rank);
        return (*this)[_get_flat_idx(std::make_tuple(idx...))];
    }

    template<typename... Idx>
    value_type& at(Idx... idx)
    {
        static_assert(sizeof...(idx) == _rank);
        return (*this)[_get_flat_idx(std::make_tuple(idx...))];
    }

    template<typename... Idx>
    value_type operator()(Idx... idx) const
    {
        static_assert(sizeof...(idx) == _rank);
        return (*this)[_get_flat_idx_unsafe(std::make_tuple(idx...))];
    }

    template<typename... Idx>
    value_type& operator()(Idx... idx)
    {
        static_assert(sizeof...(idx) == _rank);
        return (*this)[_get_flat_idx_unsafe(std::make_tuple(idx...))];
    }

    template<typename IdxTuple>
    value_type& _tuple_at(const IdxTuple& idx_tuple)
    {
        static_assert(std::tuple_size_v<IdxTuple> == _rank);
        return this->_tuple_at_impl(idx_tuple, std::make_index_sequence<_rank>{});
    }

    template<typename IdxTuple>
    value_type _tuple_at(const IdxTuple& idx_tuple) const
    {
        static_assert(std::tuple_size_v<IdxTuple> == _rank);
        return this->_tuple_at_impl(idx_tuple, std::make_index_sequence<_rank>{});
    }

    value_type operator[](size_t idx) const
    {
        WLL_ASSERT(idx < size_); // index out of range
        return ptr_[idx];
    }

    value_type& operator[](size_t idx)
    {
        WLL_ASSERT(idx < size_); // index out of range
        return ptr_[idx];
    }

    [[nodiscard]] _const_ptr_t cbegin() const noexcept
    {
        WLL_ASSERT(ptr_ != nullptr);
        return ptr_;
    }

    [[nodiscard]] _const_ptr_t cend() const noexcept
    {
        return this->cbegin() + size_;
    }

    [[nodiscard]] _const_ptr_t begin() const noexcept
    {
        return this->cbegin();
    }

    [[nodiscard]] _const_ptr_t end() const noexcept
    {
        return this->cend();
    }

    _ptr_t begin() noexcept
    {
        WLL_ASSERT(ptr_ != nullptr);
        return ptr_;
    }

    _ptr_t end() noexcept
    {
        return this->begin() + size_;
    }

    [[nodiscard]] MTensor get_mtensor() const &
    {
        return _get_mtensor_lvalue();
    }

    MTensor get_mtensor() &&
    {
        return _get_mtensor_rvalue();
    }

    template<typename InputIter>
    void copy_data_from(InputIter src, size_t count = size_t(-1))
    {
        if (count == size_t(-1))
            count = size_;
        WLL_ASSERT(count == size_);
        if (count > 0)
        {
            _ptr_t dest = this->ptr_;
            for (size_t i = 0; i < count; ++i, ++src, ++dest)
                *dest = _mtype_cast<value_type>(*src);
        }
    }

    template<typename OutputIter>
    void copy_data_to(OutputIter dest, size_t count = size_t(-1))
    {
        if (count == size_t(-1))
            count = size_;
        WLL_ASSERT(count == size_);
        if (count > 0)
        {
            _ptr_t src = this->ptr_;
            for (size_t i = 0; i < count; ++i, ++src, ++dest)
                *dest = _mtype_cast<decltype(*dest)>(*src);
        }

    }

    template<size_t Level = _rank - 1, typename IdxTuple>
    size_t _get_flat_idx(const IdxTuple& idx_tuple) const
    {
        auto plain_idx = std::get<Level>(idx_tuple);
        using plain_type = decltype(plain_idx);
        static_assert(std::is_integral_v<plain_type>, "index must be of an integral type");

        size_t unsigned_idx = size_t(plain_idx);
        if constexpr (std::is_signed_v<plain_type>)
            if (plain_idx < plain_type(0))
                unsigned_idx += dims_[Level];
        if (unsigned_idx >= dims_[Level])
            throw std::out_of_range(WLL_CURRENT_FUNCTION + "\nindex out of range");
        if constexpr (Level == 0)
            return unsigned_idx;
        else
            return _get_flat_idx<Level - 1>(idx_tuple) * dims_[Level] + unsigned_idx;
    }

    template<size_t Level = _rank - 1, typename IdxTuple>
    size_t _get_flat_idx_unsafe(const IdxTuple& idx_tuple) const noexcept
    {
        auto plain_idx = std::get<Level>(idx_tuple);
        using plain_type = decltype(plain_idx);
        static_assert(std::is_integral_v<plain_type>, "index must be of an integral type");

        size_t unsigned_idx = size_t(plain_idx);
        if constexpr (std::is_signed_v<plain_type>)
            if (plain_idx < plain_type(0))
                unsigned_idx += dims_[Level];
        WLL_ASSERT(unsigned_idx < dims_[Level]);
        if constexpr (Level == 0)
            return unsigned_idx;
        else
            return _get_flat_idx_unsafe<Level - 1>(idx_tuple) * dims_[Level] + unsigned_idx;
    }

    template<typename IdxTuple, size_t... Is>
    value_type& _tuple_at_impl(const IdxTuple& idx_tuple, std::index_sequence<Is...>)
    {
        return this->operator()(std::get<Is>(idx_tuple)...);
    }

    template<typename IdxTuple, size_t... Is>
    value_type _tuple_at_impl(const IdxTuple& idx_tuple, std::index_sequence<Is...>) const
    {
        return this->operator()(std::get<Is>(idx_tuple)...);
    }

private:

    template<size_t I = 0, typename Dims>
    bool _has_same_dims(const Dims* other_dims)
    {
        if constexpr (I + 1 == _rank)
            return (this->dims_[I] == size_t(other_dims[I])) && _has_same_dims<I + 1, Dims>(other_dims);
        else
            return this->dims_[_rank - 1] == size_t(other_dims[_rank - 1]);
    }

    void _destroy()
    {
        if (access_ == memory_type::empty)
        {
            WLL_ASSERT(mtensor_ == nullptr);
            WLL_ASSERT(ptr_ == nullptr);
        }
        else if (access_ == memory_type::owned)
        {
            WLL_ASSERT(mtensor_ == nullptr);
            free(ptr_);
        }
        else if (access_ == memory_type::proxy)
        {
            // do nothing
        }
        else if (access_ == memory_type::manual)
        {
            global_lib_data->MTensor_free(mtensor_);
        }
        else if (access_ == memory_type::shared)
        {
            global_lib_data->MTensor_disown(mtensor_);
        }
        ptr_     = nullptr;
        mtensor_ = nullptr;
        access_  = memory_type::empty;
    }

    void _release_ownership()
    {
        // destruct the object without free resource, manual tensor only
        WLL_ASSERT(access_ == memory_type::manual);
        ptr_     = nullptr;
        mtensor_ = nullptr;
        access_  = memory_type::empty;
    }

    [[nodiscard]] MTensor _get_mtensor_lvalue() const
    {
        WLL_ASSERT(access_ != memory_type::empty);

        using mtype = typename derive_tensor_data_type<value_type>::convert_type;
        constexpr int mtype_v = derive_tensor_data_type<value_type>::convert_type_v;
        static_assert(!std::is_same_v<void, mtype>, "invalid data type to convert to MType");

        MTensor ret_tensor = nullptr;
        int err = global_lib_data->MTensor_new(
            mtype_v, _rank, reinterpret_cast<mint*>(const_cast<size_t*>(dims_.data())), &ret_tensor);
        if (err != LIBRARY_NO_ERROR)
            throw library_error(err, WLL_CURRENT_FUNCTION + "\nMTensor_new() failed.");
        if constexpr (mtype_v == MType_Integer)
            _data_copy_n(ptr_, size_, global_lib_data->MTensor_getIntegerData(ret_tensor));
        else if constexpr (mtype_v == MType_Real)
            _data_copy_n(ptr_, size_, global_lib_data->MTensor_getRealData(ret_tensor));
        else  // mtype_v == MType_Complex
            _data_copy_n(ptr_, size_, global_lib_data->MTensor_getComplexData(ret_tensor));
        return ret_tensor;
    }

    MTensor _get_mtensor_rvalue()
    {
        WLL_ASSERT(access_ != memory_type::empty);
        if (access_ == memory_type::manual)
        {
            // return the containing mtensor and destroy *this
            MTensor ret = this->mtensor_;
            this->_release_ownership();
            return ret;
        }
        else // access == owned / proxy / shared
        {
            // return a copy
            return _get_mtensor_lvalue();
        }
    }

    template<typename Other>
    void _swap_pointers(Other&& other)
    {
        WLL_ASSERT(this->access_ == memory_type::owned ||
                   this->access_ == memory_type::manual); // only tensors own data can swap
        WLL_ASSERT(other.access_ == memory_type::owned ||
                   other.access_ == memory_type::manual);
        if (!(this->_has_same_dims(other.dims_.data())))
            throw library_dimension_error(WLL_CURRENT_FUNCTION + "\ntensors have different dimensions.");
        std::swap(this->ptr_, other.ptr_);
        std::swap(this->mtensor_, other.mtensor_);
        std::swap(this->access_, other.access_);
    }

    template<size_t Level, typename Data>
    static auto _get_init_data_dims_impl(_dims_t& dims, const Data& data) noexcept
        -> std::enable_if_t<Level + 1 != _rank>
    {
        const size_t size = data.size();
        WLL_ASSERT(size > 0);
        if (dims[Level] == size_t(0))
            dims[Level] = size;
        else
            WLL_ASSERT(dims[Level] == size); // shape of the array should be regular

        for (const auto& sub_data : data)
            _get_init_data_dims_impl<Level + 1>(dims, sub_data);
    }

    template<size_t Level, typename Data>
    static auto _get_init_data_dims_impl(_dims_t& dims, const Data& data) noexcept
        -> std::enable_if_t<Level + 1 == _rank>
    {
        const size_t size = data.size();
        WLL_ASSERT(size > 0);
        if (dims[Level] == size_t(0))
            dims[Level] = size;
        else
            WLL_ASSERT(dims[Level] == size); // shape of the array should be regular
    }

    static _dims_t _get_init_data_dims(const _init_data_t& data) noexcept
    {
        _dims_t dims ={};
        _get_init_data_dims_impl<0>(dims, data);
        return dims;
    }

    static _dims_t _get_init_dims(const _init_dims_t& dims, const _init_data_t& data) noexcept
    {
        return (dims.size() == 0) ? _get_init_data_dims(data) : _convert_to_dims_array<_rank>(dims);
    }

    template<size_t Level>
    [[nodiscard]] size_t _size_by_level() const noexcept
    {
        static_assert(Level <= _rank);
        if constexpr (Level == _rank)
            return size_t(1);
        else
            return dims_[Level] * _size_by_level<Level + 1>();
    }

    template<size_t Level, typename Data>
    void _fill_init_data_impl(_ptr_t& ptr, const Data& data)
    {
        static_assert(Level + 1 <= _rank);
        const size_t size = data.size();
        const size_t pad_size = this->dims_[Level] - size;
        WLL_ASSERT(size > 0 && pad_size >= 0);
        if constexpr (Level + 1 == _rank)
        {
            for (const auto& value : data)
                *(ptr++) = value;
            ptr += pad_size;
        }
        else
        {
            for (const auto& sub_data : data)
                this->_fill_init_data_impl<Level + 1>(ptr, sub_data);
            ptr += _size_by_level<Level + 1>() * pad_size;
        }
    }

    void _fill_init_data(const _init_data_t& data)
    {
        WLL_ASSERT(this->access_ != memory_type::empty);
        _ptr_t ptr = ptr_;
        this->_fill_init_data_impl<0>(ptr, data);
    }


private:
    _dims_t dims_;
    size_t  size_{};
    _ptr_t  ptr_ = nullptr;
    MTensor mtensor_ = nullptr;
    memory_type access_ = memory_type::empty;
};

template<typename T>
using list = tensor<T, 1>;
template<typename T>
using matrix = tensor<T, 2>;

template<typename T>
MTensor _scalar_mtensor(const T& value)
{
    using mtype = typename derive_tensor_data_type<T>::convert_type;
    constexpr int mtype_v = derive_tensor_data_type<T>::convert_type_v;
    static_assert(!std::is_same_v<void, mtype>, "invalid data type to convert to MType");

    MTensor mtensor;
    int err = global_lib_data->MTensor_new(mtype_v, 0, nullptr, &mtensor);
    if (err != LIBRARY_NO_ERROR)
        throw library_error(err, WLL_CURRENT_FUNCTION + "\nMTensor_new() failed.");

    if constexpr (mtype_v == MType_Integer)
        *(global_lib_data->MTensor_getIntegerData(mtensor)) = _mtype_cast<mtype>(value);
    else if constexpr (mtype_v == MType_Real)
        *(global_lib_data->MTensor_getRealData(mtensor)) = _mtype_cast<mtype>(value);
    else // mtype_v == MType_Complex
        *(global_lib_data->MTensor_getComplexData(mtensor)) = _mtype_cast<mtype>(value);

    return mtensor;
}


enum class tensor_passing_by
{
    value,     //  Automatic   tensor<T,R>
    reference, // "Shared"     tensor<T,R>&
    constant,  // "Constant"   const tensor<T,R>(&)
    unknown
};

template<typename Tensor>
struct tensor_passing_category :
    std::integral_constant<tensor_passing_by, tensor_passing_by::unknown> {};
template<typename T, size_t Rank>
struct tensor_passing_category<tensor<T, Rank>> :
    std::integral_constant<tensor_passing_by, tensor_passing_by::value> {};
template<typename T, size_t Rank>
struct tensor_passing_category<tensor<T, Rank>&> :
    std::integral_constant<tensor_passing_by, tensor_passing_by::reference> {};
template<typename T, size_t Rank>
struct tensor_passing_category<const tensor<T, Rank>> :
    std::integral_constant<tensor_passing_by, tensor_passing_by::constant> {};
template<typename T, size_t Rank>
struct tensor_passing_category<const tensor<T, Rank>&> :
    std::integral_constant<tensor_passing_by, tensor_passing_by::constant> {};
template<typename Tensor>
constexpr tensor_passing_by tensor_passing_category_v = tensor_passing_category<Tensor>::value;



template<typename T, size_t Rank, bool IsConst>
class _sparse_element;

template<typename T, size_t Rank, bool IsConst>
class _sparse_iterator;

template<typename T, size_t Rank>
class sparse_array
{
public:
    using value_type   = T;
    static constexpr size_t _rank = Rank;
    using _ptr_t       = value_type*;
    using _const_ptr_t = const value_type*;
    using _dims_t      = std::array<size_t, _rank>;
    using _idx_t       = std::array<size_t, _rank>;
    static constexpr size_t _column_size = (Rank >= 2) ? (Rank - 1) : 1;
    using _column_t    = std::array<size_t, _column_size>;
    using _init_dims_t = std::initializer_list<size_t>;
    using _init_rule_t = std::pair<_idx_t, value_type>;
    using _init_data_t = std::initializer_list<_init_rule_t>;
    static_assert(_rank > 0);

    using iterator        = _sparse_iterator<value_type, _rank, false>;
    using const_iterator  = _sparse_iterator<value_type, _rank, true>;
    using reference       = _sparse_element<value_type, _rank, false>;
    using const_reference = _sparse_element<value_type, _rank, true>;
    friend reference;
    friend const_reference;

    template<typename U, size_t URank>
    friend class sparse_array;

    sparse_array() = default;

    sparse_array(MSparseArray msparse, memory_type access) :
        access_{access}
    {
        WLL_ASSERT(_rank == global_sparse_fn->MSparseArray_getRank(msparse));
        WLL_ASSERT(access_ == memory_type::owned ||
                   access_ == memory_type::proxy ||
                   access_ == memory_type::shared);

        const mint* dims_ptr = global_sparse_fn->MSparseArray_getDimensions(msparse);
        std::copy_n(dims_ptr, _rank, dims_.begin());
        size_ = _flattened_size(dims_);

        MTensor m_values   = *(global_sparse_fn->MSparseArray_getExplicitValues(msparse));
        MTensor m_columns  = *(global_sparse_fn->MSparseArray_getColumnIndices(msparse));
        MTensor m_row_idx  = *(global_sparse_fn->MSparseArray_getRowPointers(msparse));
        MTensor m_implicit = *(global_sparse_fn->MSparseArray_getImplicitValue(msparse));
        nz_size_ = (m_values == nullptr) ? 0 : *(global_lib_data->MTensor_getDimensions(m_values));

        mint* m_columns_ptr  = global_lib_data->MTensor_getIntegerData(m_columns); // can be nullptr
        mint* m_row_idx_ptr  = global_lib_data->MTensor_getIntegerData(m_row_idx); // can be nullptr
        void* m_values_ptr   = nullptr;  // type dependent
        bool  do_copy = false;
        int   mtype   = int(global_lib_data->MTensor_getType(m_implicit));
        WLL_ASSERT(mtype == MType_Integer ||
                   mtype == MType_Real    ||
                   mtype == MType_Complex);

        if (mtype == MType_Integer)
        {
            m_values_ptr = reinterpret_cast<void*>(global_lib_data->MTensor_getIntegerData(m_values));
            do_copy      = !is_same_layout_v<mint, value_type>;
            this->implicit_value_ =
                _mtype_cast<value_type>(*(global_lib_data->MTensor_getIntegerData(m_implicit)));
        }
        else if (mtype == MType_Real)
        {
            m_values_ptr = reinterpret_cast<void*>(global_lib_data->MTensor_getRealData(m_values));
            do_copy      = !is_same_layout_v<mreal, value_type>;
            this->implicit_value_ =
                _mtype_cast<value_type>(*(global_lib_data->MTensor_getRealData(m_implicit)));
        }
        else // mtype == MType_Complex
        {
            m_values_ptr = reinterpret_cast<void*>(global_lib_data->MTensor_getComplexData(m_values));
            do_copy      = !is_same_layout_v<mcomplex, value_type>;
            this->implicit_value_ =
                _mtype_cast<value_type>(*(global_lib_data->MTensor_getComplexData(m_implicit)));
        }

        if (do_copy)
        {
            WLL_ASSERT(access_ != memory_type::shared);
            this->values_vec_.resize(_nz_size());
            this->columns_vec_.resize(_nz_size());
            this->row_idx_vec_.resize(_row_idx_size());

            if (mtype == MType_Integer)
                _data_copy_n(reinterpret_cast<mint*>(m_values_ptr), _nz_size(), values_vec_.data());
            else if (mtype == MType_Real)
                _data_copy_n(reinterpret_cast<mreal*>(m_values_ptr), _nz_size(), values_vec_.data());
            else // mtype == MType_Complex
                _data_copy_n(reinterpret_cast<mcomplex*>(m_values_ptr), _nz_size(), values_vec_.data());

            std::copy_n(reinterpret_cast<_column_t*>(m_columns_ptr), _nz_size(), columns_vec_.data());
            std::copy_n(reinterpret_cast<size_t*>(m_row_idx_ptr), _row_idx_size(), row_idx_vec_.data());

            this->access_  = memory_type::owned;
            this->_update_pointers();
        }
        else // do_copy == false
        {
            this->values_  = reinterpret_cast<_ptr_t>(m_values_ptr);
            this->columns_ = reinterpret_cast<_column_t*>(m_columns_ptr);
            this->row_idx_ = reinterpret_cast<size_t*>(m_row_idx_ptr);

            if (access == memory_type::owned)
                this->_convert_to_owned();
            else if (access == memory_type::shared)
                this->msparse_ = msparse;
        }
    }

    explicit sparse_array(const tensor<value_type, _rank>& other, value_type value = value_type{},
                 double reserve_density = -1.0) :
        dims_{other.dimensions()}, size_{other.size()},
        implicit_value_{value}, access_{memory_type::owned}
    {
        constexpr size_t reserve_multiplier = 2;
        constexpr size_t min_reserve_size   = 1000;
        constexpr size_t reserve_sqrt_size  = (1000 / 2) * (1000 / 2);

        size_t reserve_size = 0;
        if (0.0 <= reserve_density && reserve_density <= 1.0)
            reserve_size = size_t(std::round(reserve_density * size_));
        else if (size_ <= min_reserve_size)
            reserve_size = size_;
        else if (size_ <= reserve_sqrt_size)
            reserve_size = min_reserve_size;
        else
            reserve_size = size_t(std::round(std::sqrt(size_) * reserve_multiplier));

        this->columns_vec_.reserve(reserve_size);
        this->values_vec_.reserve(reserve_size);
        this->row_idx_vec_.reserve(this->_row_idx_size());
        this->row_idx_vec_.push_back(0);

        if constexpr (_rank == 1)
        {
            const value_type* data_ptr = other.data();
            for (size_t i_col = 1; i_col <= dims_[0]; ++i_col, ++data_ptr)
            {
                if (*data_ptr != this->implicit_value_)
                {
                    this->columns_vec_.push_back(_column_t({i_col}));
                    this->values_vec_.push_back(*data_ptr);
                }
            }
            this->nz_size_ = values_vec_.size();
            this->row_idx_vec_.push_back(nz_size_);
        }
        else if (_rank == 2)
        {
            size_t i_nz = 0;
            const value_type* data_ptr = other.data();
            for (size_t i_row = 0; i_row < dims_[0]; ++i_row)
            {
                for (size_t i_col = 1; i_col <= dims_[1]; ++i_col, ++data_ptr)
                {
                    if (*data_ptr != this->implicit_value_)
                    {
                        this->columns_vec_.push_back(_column_t({i_col}));
                        this->values_vec_.push_back(*data_ptr);
                        ++i_nz;
                    }
                }
                this->row_idx_vec_.push_back(i_nz);
            }
            this->nz_size_ = i_nz;
        }
        else // _rank >= 3
        {
            size_t i_nz = 0;
            const value_type* data_ptr = other.data();
            for (size_t i_row = 0; i_row < dims_[0]; ++i_row)
            {
                this->_construct_from_tensor_impl<1>(i_nz, data_ptr);
                this->row_idx_vec_.push_back(i_nz);
            }
            this->nz_size_ = i_nz;
        }
        this->_update_pointers();
    }

    explicit sparse_array(_dims_t dims, value_type value = value_type{}) :
        dims_{dims}, size_{_flattened_size(dims)}, nz_size_{size_t(0)},
        implicit_value_{value}, access_{memory_type::owned}
    {
        this->row_idx_vec_.resize(_row_idx_size(), size_t(0));
        this->_update_pointers();
    }

    sparse_array(_init_dims_t dims, value_type value = value_type{}) :
        sparse_array(_convert_to_dims_array<_rank>(dims), value) {}

    sparse_array(_dims_t dims, _init_data_t rules, value_type value = value_type{}) :
        sparse_array(dims, value)
    {
        std::vector<_init_rule_t> rules_vec(rules);
        WLL_ASSERT(_rules_vector_index_check(rules_vec)); // some indices are out of range

        // stable sort by indices
        std::stable_sort(rules_vec.begin(), rules_vec.end(),
                         [](auto& r1, auto& r2) { return r1.first < r2.first; });
        // remove duplicates, keeping the last one
        auto iter = std::unique(rules_vec.rbegin(), rules_vec.rend(),
                                [](auto& r1, auto& r2) { return r1.first == r2.first; });
        rules_vec.erase(rules_vec.begin(), iter.base());

        if constexpr (_rank == 1)
        {
            this->row_idx_vec_[1] = rules_vec.size();
            for (const auto& rule : rules_vec)
            {
                if (rule.second != this->implicit_value_)
                {
                    this->columns_vec_.push_back({rule.first[0] + 1});
                    this->values_vec_.push_back(rule.second);
                }
            }
        }
        else
        {
            auto first = rules_vec.cbegin();
            auto last  = rules_vec.cend();
            for (size_t i_row = 0; i_row < dims_[0]; ++i_row)
            {
                auto upper = std::upper_bound(
                    first, last, i_row, [](size_t i, auto& r) { return i < r.first[0]; });
                this->row_idx_vec_[i_row + 1] = upper - rules_vec.cbegin();
                for (; first != upper; ++first)
                {
                    if (first->second != this->implicit_value_)
                    {
                        _column_t col_idx{};
                        for (size_t i_col = 0; i_col < _rank - 1; ++i_col)
                            col_idx[i_col] = first->first[i_col + 1] + 1;
                        this->columns_vec_.push_back(col_idx);
                        this->values_vec_.push_back(first->second);
                    }
                }
            }
        }
        this->nz_size_ = values_vec_.size();
        this->_update_pointers();
    }

    sparse_array(_init_dims_t dims, _init_data_t rules, value_type value = value_type{}) :
        sparse_array(_get_init_dims(dims, rules), rules, value) {}

    template<bool UsePointers, bool SwapColRow, bool SwapValues, bool SameType, typename Other>
    void _ctor_impl(Other&& other)
    {
        WLL_ASSERT(other.access_ == memory_type::owned ||
                   other.access_ == memory_type::proxy ||
                   other.access_ == memory_type::shared);
        WLL_ASSERT(this->dims_ == other.dims_);
        WLL_ASSERT(this->size_ == other.size_);

        this->nz_size_ = other.nz_size_;
        this->access_  = memory_type::owned;
        if constexpr (SameType)
            this->implicit_value_ = other.implicit_value_;
        else
            this->implicit_value_ = _mtype_cast<value_type>(other.implicit_value_);

        if constexpr (UsePointers)
        {
            static_assert(SameType);
            this->values_  = other.values_;
            this->columns_ = other.columns_;
            this->row_idx_ = other.row_idx_;
            this->_convert_to_owned();
        }
        else
        {
            if constexpr (SwapColRow)
            {
                std::swap(this->columns_vec_, other.columns_vec_);
                std::swap(this->row_idx_vec_, other.row_idx_vec_);
            }
            else // CopyColRow
            {
                this->columns_vec_ = std::vector<_column_t>(other.columns_, other.columns_ + _nz_size());
                this->row_idx_vec_ = std::vector<size_t>(other.row_idx_, other.row_idx_ + _row_idx_size());
            }

            if constexpr (SwapValues)
            {
                static_assert(SameType);
                std::swap(this->values_vec_, other.values_vec_);
                this->_update_pointers();
            }
            else if constexpr (SameType) // CopyValues
            {
                this->values_vec_ = std::vector<size_t>(other.values_, other.values_ + _nz_size());
                this->_update_pointers();
            }
            else // DifferentType CopyValues
            {
                this->values_vec_.resize(_nz_size());
                _data_copy_n(other.values_, _nz_size(), this->values_vec_.data());
                this->_update_pointers();
                this->refresh_implicit();
            }
        }
    }

    sparse_array(const sparse_array& other) :
        dims_{other.dims_}, size_{other.size_}
    {
        if (other.access_ == memory_type::owned)
            this->_ctor_impl<false, false, false, true>(other);
        else // proxy or shared
            this->_ctor_impl<true, false, false, true>(other);
    }

    sparse_array(sparse_array&& other) noexcept :
        dims_{other.dims_}, size_{other.size_}
    {
        if (other.access_ == memory_type::owned)
            this->_ctor_impl<false, true, true, true>(std::move(other));
        else // proxy or shared
            this->_ctor_impl<true, false, false, true>(other);
    }

    template<typename U>
    explicit sparse_array(const sparse_array<U, _rank>& other) :
        dims_{other.dims_}, size_{other.size_}
    {
        this->_ctor_impl<false, false, false, false>(other);
    }

    template<typename U>
    explicit sparse_array(sparse_array<U, _rank>&& other) :
        dims_{other.dims_}, size_{other.size_}
    {
        this->_ctor_impl<false, false, false, false>(std::move(other));
    }

    sparse_array& operator=(const sparse_array& other)
    {
        if(this == &other) return *this;
        WLL_ASSERT(this->access_ != memory_type::empty);
        WLL_ASSERT(other.access_ != memory_type::empty);
        if (this->values_ != other.values_)
        {
            if (!(this->_has_same_dims(other.dims_.data())))
                throw library_dimension_error(WLL_CURRENT_FUNCTION + "\nSparse arrays have different dimensions.");
            if (this->access_ == memory_type::shared)
            {
                if (this->nz_size_ != other.nz_size_)
                    throw library_dimension_error(WLL_CURRENT_FUNCTION + "\nDifferent numbers of non-zero values.");
                if (this->implicit_value_ != other.implicit_value_)
                    throw library_dimension_error(WLL_CURRENT_FUNCTION + "\nDifferent implicit values.");
                if (!std::equal(this->columns_, this->columns_ + _nz_size(), other.columns_) ||
                    !std::equal(this->row_idx_, this->row_idx_ + _nz_size(), other.row_idx_))
                    throw library_dimension_error(WLL_CURRENT_FUNCTION + "\nDifferent non-zero value positions.");
                std::copy_n(other.values_, _nz_size(), this->values_);
            }
            else if (other.access_ == memory_type::owned)
                this->_ctor_impl<false, false, false, true>(other);
            else // other.access_ == proxy / shared
                this->_ctor_impl<true, false, false, true>(other);
        }
        return *this;
    }

    sparse_array& operator=(sparse_array&& other)
    {
        if(this == &other) return *this;
        WLL_ASSERT(this->access_ != memory_type::empty);
        WLL_ASSERT(other.access_ != memory_type::empty);
        if (this->values_ != other.values_)
        {
            if (!(this->_has_same_dims(other.dims_.data())))
                throw library_dimension_error(WLL_CURRENT_FUNCTION + "\nSparse arrays have different dimensions.");
            if (this->access_ == memory_type::shared)
            {
                if (this->nz_size_ != other.nz_size_)
                    throw library_dimension_error(WLL_CURRENT_FUNCTION + "\nDifferent numbers of non-zero values.");
                if (this->implicit_value_ != other.implicit_value_)
                    throw library_dimension_error(WLL_CURRENT_FUNCTION + "\nDifferent implicit values.");
                if (!std::equal(this->columns_, this->columns_ + _nz_size(), other.columns_) ||
                    !std::equal(this->row_idx_, this->row_idx_ + _nz_size(), other.row_idx_))
                    throw library_dimension_error(WLL_CURRENT_FUNCTION + "\nDifferent non-zero value positions.");
                std::copy_n(other.values_, _nz_size(), this->values_);
            }
            else if (other.access_ == memory_type::owned)
                this->_ctor_impl<false, true, true, true>(other);
            else // other.access_ == proxy / shared
                this->_ctor_impl<true, false, false, true>(other);
        }
        return *this;
    }

    template<typename U>
    sparse_array& operator=(const sparse_array<U, _rank>& other)
    {
        if(this == &other) return *this;
        WLL_ASSERT(this->access_ != memory_type::empty);
        WLL_ASSERT(other.access_ != memory_type::empty);
        WLL_ASSERT((void*)this->values != (void*)other.values_);
        if (!(this->_has_same_dims(other.dims_.data())))
            throw library_dimension_error(WLL_CURRENT_FUNCTION + "\nSparse arrays have different dimensions.");
        if (this->access_ == memory_type::shared)
        {
            if (this->nz_size_ != other.nz_size_)
                throw library_dimension_error(WLL_CURRENT_FUNCTION + "\nDifferent numbers of non-zero values.");
            if (this->implicit_value_ != _mtype_cast<value_type>(other.implicit_value_))
                throw library_dimension_error(WLL_CURRENT_FUNCTION + "\nDifferent implicit values.");
            if (!std::equal(this->columns_, this->columns_ + _nz_size(), other.columns_) ||
                !std::equal(this->row_idx_, this->row_idx_ + _nz_size(), other.row_idx_))
                throw library_dimension_error(WLL_CURRENT_FUNCTION + "\nDifferent non-zero value positions.");
            _data_copy_n(other.values_, _nz_size(), this->values_);
        }
        else
        {
            this->_ctor_impl<false, false, false, false>(other);
        }
        return *this;
    }

    template<typename U>
    sparse_array& operator=(sparse_array<U, _rank>&& other)
    {
        if(this == &other) return *this;
        WLL_ASSERT(this->access_ != memory_type::empty);
        WLL_ASSERT(other.access_ != memory_type::empty);
        WLL_ASSERT((void*)this->values != (void*)other.values_);
        if (!(this->_has_same_dims(other.dims_.data())))
            throw library_dimension_error(WLL_CURRENT_FUNCTION + "\nSparse arrays have different dimensions.");
        if (this->access_ == memory_type::shared)
        {
            if (this->nz_size_ != other.nz_size_)
                throw library_dimension_error(WLL_CURRENT_FUNCTION + "\nDifferent numbers of non-zero values.");
            if (this->implicit_value_ != _mtype_cast<value_type>(other.implicit_value_))
                throw library_dimension_error(WLL_CURRENT_FUNCTION + "\nDifferent implicit values.");
            if (!std::equal(this->columns_, this->columns_ + _nz_size(), other.columns_) ||
                !std::equal(this->row_idx_, this->row_idx_ + _nz_size(), other.row_idx_))
                throw library_dimension_error(WLL_CURRENT_FUNCTION + "\nDifferent non-zero value positions.");
            _data_copy_n(other.values_, _nz_size(), this->values_);
        }
        else
        {
            this->_ctor_impl<false, false, false, false>(other);
        }
        return *this;
    }

    ~sparse_array()
    {
        if (this->access_ == memory_type::shared)
            global_sparse_fn->MSparseArray_disown(this->msparse_);
        this->values_  = nullptr;
        this->columns_ = nullptr;
        this->row_idx_ = nullptr;
        this->access_  = memory_type::empty;
        this->msparse_ = nullptr;
    }

    [[nodiscard]] constexpr size_t rank() const noexcept
    {
        return _rank;
    }

    [[nodiscard]] size_t size() const noexcept
    {
        return size_;
    }

    _dims_t dimensions() const noexcept
    {
        return dims_;
    }

    [[nodiscard]] size_t dimension(size_t level) const noexcept
    {
        return dims_[level];
    }

    value_type implicit_value() const noexcept
    {
        return this->implicit_value_;
    }

    const _column_t* columns_pointer() const noexcept
    {
        return this->columns_;
    }

    const value_type* values_pointer() const noexcept
    {
        return this->values_;
    }

    [[nodiscard]] const size_t* row_indices_pointer() const noexcept
    {
        return this->row_idx_;
    }

    bool operator==(const sparse_array& other) const
    {
        if (this->dims_ != other.dims_)
            return false;
        if (this->implicit_value_ == other.implicit_value_)
        {
            if (this->_nz_size() != other._nz_size())
                return false;
            if (this->row_idx_ != other.row_idx_ &&
                !std::equal(this->row_idx_, this->row_idx_ + _row_idx_size(), other.row_idx_))
                return false;
            if (this->values_ != other.values_ &&
                !std::equal(this->values_, this->values_ + _nz_size(), other.values_))
                return false;
            if (this->columns_ != other.columns_ &&
                !std::equal(this->columns_, this->columns_ + _nz_size(), other.columns_))
                return false;
        }
        else
        {
            if (this->_nz_size() + other._nz_size() < this->size())
                return false;
            tensor<value_type, _rank> tensor_this(*this);
            tensor<value_type, _rank> tensor_other(other);
            return (tensor_this == tensor_other);
        }
        return true;
    }

    template<typename... Idx>
    reference operator()(Idx... idx) noexcept
    {
        static_assert(sizeof...(idx) == _rank);
        return this->_get_element_ref(std::make_index_sequence<_rank>{}, idx...);
    }

    template<typename... Idx>
    value_type operator()(Idx... idx) const
    {
        static_assert(sizeof...(idx) == _rank);
        const_reference ref = this->_get_element_ref(std::make_index_sequence<_rank>{}, idx...);
        return value_type(ref);
    }

    template<typename... Idx>
    reference at(Idx... idx)
    {
        static_assert(sizeof...(idx) == _rank);
        reference ref = this->_get_element_ref(std::make_index_sequence<_rank>{}, idx...);
        if (!ref._check_range())
            throw std::out_of_range(WLL_CURRENT_FUNCTION + "\nindex out of range");
        return ref;
    }

    template<typename... Idx>
    value_type at(Idx... idx) const
    {
        static_assert(sizeof...(idx) == _rank);
        const_reference ref = this->_get_element_ref(std::make_index_sequence<_rank>{}, idx...);
        if (!ref._check_range())
            throw std::out_of_range(WLL_CURRENT_FUNCTION + "\nindex out of range");
        return value_type(ref);
    }

    const_iterator cbegin() const noexcept
    {
        return {*this, _idx_t{}};
    }

    const_iterator cend() const noexcept
    {
        _idx_t idx{{}};
        idx[0] = this->dims_[0];
        return {*this, idx};
    }

    const_iterator begin() const noexcept
    {
        return this->cbegin();
    }

    const_iterator end() const noexcept
    {
        return this->cend();
    }

    iterator begin() noexcept
    {
        return {*this, _idx_t{}};
    }

    iterator end() noexcept
    {
        _idx_t idx{{}};
        idx[0] = this->dims_[0];
        return {*this, idx};
    }

    [[nodiscard]] MSparseArray get_msparse() const
    {
        using mtype = typename derive_tensor_data_type<value_type>::convert_type;
        constexpr int mtype_v = derive_tensor_data_type<value_type>::convert_type_v;
        static_assert(!std::is_same_v<void, mtype>, "invalid data type to convert to MType");

        tensor<mint, 1> dims({_rank}, memory_type::manual);
        dims.copy_data_from(this->dims_.data(), _rank);

        tensor<mint, 2> poss({_nz_size(), _rank}, memory_type::manual);
        auto* poss_ptr = reinterpret_cast<std::array<mint, _rank>*>(poss.data());
        for (size_t i_row = 1; i_row < _row_idx_size(); ++i_row)
        {
            for (size_t i_nz = row_idx_[i_row - 1]; i_nz < row_idx_[i_row]; ++i_nz, ++poss_ptr)
            {
                if constexpr (_rank == 1)
                {
                    *reinterpret_cast<_column_t*>(&(*poss_ptr)[0]) = this->columns_[i_nz];
                }
                else
                {
                    (*poss_ptr)[0] = i_row;
                    *reinterpret_cast<_column_t*>(&(*poss_ptr)[1]) = this->columns_[i_nz];
                }
            }
        }

        tensor<mtype, 1> vals({_nz_size()}, memory_type::manual);
        vals.copy_data_from(this->values_, _nz_size());

        MSparseArray msparse = nullptr;
        int err = global_sparse_fn->MSparseArray_fromExplicitPositions(
            std::move(poss).get_mtensor(),
            std::move(vals).get_mtensor(),
            std::move(dims).get_mtensor(),
            _scalar_mtensor(_mtype_cast<mtype>(this->implicit_value_)),
            &msparse);
        if (err != LIBRARY_NO_ERROR)
            throw library_error(err, WLL_CURRENT_FUNCTION + "\nMSparseArray_fromExplicitPositions() failed.");


        return msparse;
    }

    void refresh_implicit()
    {
        WLL_ASSERT(this->access_ == memory_type::owned);
        WLL_ASSERT(this->_check_consistency());

        size_t i_nz = 0;
        size_t new_i_nz = 0;
        for (size_t i_row = 1; i_row < _row_idx_size(); ++i_row)
        {
            for (; i_nz < row_idx_vec_[i_row]; ++i_nz)
            {
                if (values_vec_[i_nz] != this->implicit_value_)
                {
                    values_vec_[new_i_nz]  = values_vec_[i_nz];
                    columns_vec_[new_i_nz] = columns_vec_[i_nz];
                    ++new_i_nz;
                }
            }
            row_idx_vec_[i_row] = new_i_nz;
        }
        nz_size_ = new_i_nz;
        values_vec_.resize(new_i_nz);
        columns_vec_.resize(new_i_nz);
        this->_update_pointers();
    }

    template<bool RefreshImplicit = false, typename Fn>
    void transform(Fn fn)
    {
        WLL_ASSERT(_check_consistency());
        this->implicit_value_ = static_cast<value_type>(fn(this->implicit_value_));
        std::for_each(this->values_, this->values_ + _nz_size(),
                      [=](auto& value) { value = static_cast<value_type>(fn(value)); });
        if constexpr (RefreshImplicit)
            this->refresh_implicit();
    }

    explicit operator tensor<value_type, _rank>() const
    {
        WLL_ASSERT(this->_check_consistency());
        tensor<value_type, _rank> ret(this->dims_);
        std::fill(ret.begin(), ret.end(), this->implicit_value_);

        if constexpr (_rank == 1)
        {
            size_t i_nz = 0;
            for (; i_nz < _nz_size(); ++i_nz)
                ret._tuple_at(_make_zero_based_idx(0, i_nz)) = values_[i_nz];
        }
        else
        {
            size_t i_nz = 0;
            for (size_t i_row = 1; i_row < _row_idx_size(); ++i_row)
            {
                for (; i_nz < row_idx_vec_[i_row]; ++i_nz)
                    ret._tuple_at(_make_zero_based_idx(i_row - 1, i_nz)) = values_[i_nz];
            }
        }
        return ret;
    }

private:

    [[nodiscard]] size_t _nz_size() const noexcept
    {
        return this->nz_size_;
    }

    [[nodiscard]] size_t _row_idx_size() const noexcept
    {
        return _rank == 1 ? 2 : (dims_[0] + 1);
    }

    template<size_t Level, typename... ICol>
    void _construct_from_tensor_impl(size_t& i_nz, const value_type*& data_ptr, ICol... i_col)
    {
        if constexpr (Level + 1 == _rank)
        {
            for (size_t i_col_x = 1; i_col_x <= this->dims_[Level]; ++i_col_x, ++data_ptr)
            {
                if (*data_ptr != this->implicit_value_)
                {
                    this->columns_vec_.push_back(_column_t({i_col..., i_col_x}));
                    this->values_vec_.push_back(*data_ptr);
                    ++i_nz;
                }
            }
        }
        else
        {
            for (size_t i_col_x = 1; i_col_x <= this->dims_[Level]; ++i_col_x)
            {
                this->_construct_from_tensor_impl<Level + 1>(i_nz, data_ptr, i_col..., i_col_x);
            }
        }
    }

    template<size_t... Is>
    _idx_t _make_zero_based_idx_impl(size_t row_idx, size_t i_nz,
                                     std::index_sequence<Is...>) const
    {
        return {row_idx, (std::get<Is>(columns_[i_nz]) - 1)...};
    }

    _idx_t _make_zero_based_idx(size_t row_idx, size_t i_nz) const
    {
        if constexpr (_rank == 1)
            return {std::get<0>(columns_[i_nz]) - 1};
        else
            return _make_zero_based_idx_impl(
                row_idx, i_nz, std::make_index_sequence<_column_size>{});
    }

    template<size_t I = 0, typename Dims>
    bool _has_same_dims(const Dims* other_dims)
    {
        if constexpr (I + 1 == _rank)
            return (this->dims_[I] == size_t(other_dims[I])) && _has_same_dims<I + 1, Dims>(other_dims);
        else
            return this->dims_[_rank - 1] == size_t(other_dims[_rank - 1]);
    }

    void _update_pointers()
    {
        WLL_ASSERT(access_ == memory_type::owned);
        this->values_  = values_vec_.data();
        this->columns_ = columns_vec_.data();
        this->row_idx_ = row_idx_vec_.data();
    }

    [[nodiscard]] bool _check_consistency() const
    {
        WLL_ASSERT(this->access_ == memory_type::owned ||
                   this->access_ == memory_type::proxy ||
                   this->access_ == memory_type::shared);
        if (this->access_ == memory_type::owned)
        {
            if (values_vec_.data()  != this->values_)   assert(false);
            if (columns_vec_.data() != this->columns_)  assert(false);
            if (row_idx_vec_.data() != this->row_idx_)  assert(false);
            if (values_vec_.size()  != _nz_size())      assert(false);
            if (columns_vec_.size() != _nz_size())      assert(false);
            if (row_idx_vec_.size() != _row_idx_size()) assert(false);
            if (_flattened_size<_rank>(dims_) != size_) assert(false);
        }
        else
        {
            if (_flattened_size<_rank>(dims_) != size_) assert(false);
            if (msparse_ == nullptr)                    assert(false);
        }
        return true;
    }

    void _convert_to_owned()
    {
        WLL_ASSERT(access_ == memory_type::proxy);
        this->values_vec_.resize(_nz_size());
        this->columns_vec_.resize(_nz_size());
        this->row_idx_vec_.resize(_row_idx_size());

        std::copy_n(values_, _nz_size(), values_vec_.begin());
        std::copy_n(columns_, _nz_size(), columns_vec_.begin());
        std::copy_n(row_idx_, _row_idx_size(), row_idx_vec_.begin());

        this->access_  = memory_type::owned;
        this->msparse_ = nullptr;
        this->_update_pointers();
    }

    void _insert_explicit(size_t offset, const value_type& value,
                          const _column_t& col_idx, size_t row_idx_offset)
    {
        WLL_ASSERT(offset <= this->nz_size_);
        WLL_ASSERT(value != this->implicit_value_); // not effectively adding element
        WLL_ASSERT(this->access_ != memory_type::shared);
        if (this->access_ == memory_type::proxy)
            this->_convert_to_owned();
        values_vec_.insert(values_vec_.cbegin() + offset, value);
        columns_vec_.insert(columns_vec_.cbegin() + offset, col_idx);
        std::for_each(row_idx_vec_.begin() + row_idx_offset + 1, row_idx_vec_.end(),
                      [](size_t& idx) { ++idx; });
        ++nz_size_;
        this->_update_pointers();
    }

    void _change_explicit(size_t offset, const value_type& value)
    {
        WLL_ASSERT(offset < nz_size_);
        WLL_ASSERT(value != this->implicit_value_); // not effectively adding element
        this->values_[offset] = value;
    }

    void _erase_explicit(size_t offset, size_t row_idx_offset)
    {
        WLL_ASSERT(offset < nz_size_);
        WLL_ASSERT(this->access_ != memory_type::shared);
        if (this->access_ == memory_type::proxy)
            this->_convert_to_owned();
        values_vec_.erase(values_vec_.cbegin() + offset);
        columns_vec_.erase(columns_vec_.cbegin() + offset);
        std::for_each(row_idx_vec_.begin() + row_idx_offset + 1, row_idx_vec_.end(),
                      [](size_t& idx) { --idx; });
        WLL_ASSERT(this->_check_consistency());
    }

    template<typename... Idx, size_t... Is>
    reference _get_element_ref(std::index_sequence<Is...>, Idx... idx) noexcept
    {
        return {*this, {(_add_if_negative(idx, this->dims_[Is]) + column_base_correction<Is>())...}};
    }

    template<typename... Idx, size_t... Is>
    const_reference _get_element_ref(std::index_sequence<Is...>, Idx... idx) const noexcept
    {
        return {*this, {(_add_if_negative(idx, this->dims_[Is]) + column_base_correction<Is>())...}};
    }

    template<size_t Level>
    [[nodiscard]] constexpr size_t column_base_correction() const
    {
        static_assert(Level < _rank);
        return (_rank == 1 || Level > size_t(0)) ? 1 : 0;
    }
    
    static _dims_t _get_init_data_dims(const _init_data_t& rules) noexcept
    {
        _dims_t dims{};
        for (const auto& rule : rules)
        {
            const _idx_t& idx = rule.first;
            for (size_t i = 0; i < _rank; ++i)
                if (dims[i] < idx[i])
                    dims[i] = idx[i];
        }
        for (auto& e : dims)
            ++e;
        return dims;
    }

    static _dims_t _get_init_dims(const _init_dims_t& dims, const _init_data_t& rules) noexcept
    {
        return (dims.size() == 0) ? _get_init_data_dims(rules) : _convert_to_dims_array<_rank>(dims);
    }

    bool _rules_vector_index_check(const std::vector<_init_rule_t>& rules_vec) const
    {
        for (const auto& rule : rules_vec)
        {
            for (size_t i = 0; i < _rank; ++i)
                if (rule.first[i] >= dims_[i])
                    return false;
        }
        return true;
    }

private:
    _dims_t    dims_;
    size_t     size_{};              // total size of the array
    size_t     nz_size_{};           // number of non-zero elements
    value_type implicit_value_{};
    _ptr_t     values_  = nullptr; // (nz_size_)
    _column_t* columns_ = nullptr; // (nz_size_) * (_column_size)
    size_t*    row_idx_ = nullptr; // (_rank == 1 ? 2 : dims_[0])

    memory_type  access_  = memory_type::empty;
    MSparseArray msparse_ = nullptr;
    std::vector<value_type> values_vec_{};
    std::vector<_column_t>  columns_vec_{}; // 1-based numbering
    std::vector<size_t>     row_idx_vec_{};
};

template<typename T, size_t Rank, bool IsConst>
class _sparse_element
{
public:
    using value_type = T;
    static constexpr size_t _rank = Rank;
    static constexpr bool _is_const = IsConst;
    using _idx_t    = std::array<size_t, _rank>;
    using _sparse_t = std::conditional_t<IsConst,
        const sparse_array<value_type, _rank>&, sparse_array<value_type, _rank>&>;
    using _column_t = typename sparse_array<value_type, _rank>::_column_t;
    static_assert(_rank > 0);

    _sparse_element(_sparse_t sparse, _idx_t idx) :
        sparse_{sparse}, idx_{idx} {}

    explicit operator value_type() const
    {
        const auto[is_explicit, offset] = this->_find_element();
        return is_explicit ? sparse_.values_[offset] : sparse_.implicit_value_;
    }

    _sparse_element& operator=(const value_type& value)
    {
        if constexpr (_is_const)
            WLL_ASSERT(false); // cannot assign to a const sparse array
        else // not const
        {
            const auto[is_explicit, offset] = this->_find_element();
            if (value == sparse_.implicit_value())
            {
                if (is_explicit)
                {
                    WLL_ASSERT(sparse_.access_ != memory_type::shared); // cannot erase explicit from shared
                    sparse_._erase_explicit(offset, _row_idx_offset());
                }
            }
            else // value != implicit_element
            {
                if (is_explicit)
                    sparse_._change_explicit(offset, value);
                else
                {
                    WLL_ASSERT(sparse_.access_ != memory_type::shared); // cannot insert explicit to shared
                    sparse_._insert_explicit(offset, value, _col_idx_ref(), _row_idx_offset());
                }
            }
        }
        return *this;
    }

    [[nodiscard]] bool _check_range() const noexcept
    {
        const auto& dims = sparse_.dimensions();
        if constexpr (_rank == 1)
        {
            if (!(1 <= idx_[0] && idx_[0] <= dims[0]))
                return false;
        }
        else
        {
            if (!(idx_[0] < dims[0]))
                return false;
            for (size_t i = 1; i < _rank; ++i)
                if (!(1 <= idx_[i] && idx_[i] <= dims[i]))
                    return false;
        }
        return true;
    }

    [[nodiscard]] std::pair<bool, size_t> _find_element() const
    {
        WLL_ASSERT(sparse_._check_consistency());
        WLL_ASSERT(_check_range());
        const _column_t* columns = sparse_.columns_;
        const _column_t* first = (_rank == 1) ?
            columns : (columns + sparse_.row_idx_[idx_[0]]);
        const _column_t* last = (_rank == 1) ?
            columns + sparse_._nz_size() : (columns + sparse_.row_idx_[idx_[0] + 1]);
        const auto[lower, upper] = std::equal_range(first, last, _col_idx_ref());

        if (lower < upper)
        { // the element is explicit
            WLL_ASSERT(lower + 1 == upper);
            return std::make_pair(true, lower - columns);
        }
        else
        { // the element is implicit
            WLL_ASSERT(lower == upper);
            return std::make_pair(false, lower - columns);
        }
    }

    [[nodiscard]] size_t _row_idx_offset() const
    {
        return (_rank == 1) ? 0 : idx_[0];
    }
    const _column_t& _col_idx_ref() const
    {
        return *reinterpret_cast<const _column_t*>(&idx_[(_rank == 1) ? 0 : 1]);
    }

private:
    _sparse_t sparse_;
    _idx_t    idx_;
};

template<typename T, size_t Rank, bool IsConst>
class _sparse_iterator
{
public:
    using value_type = T;
    static constexpr size_t _rank = Rank;
    static constexpr bool _is_const = IsConst;

    using iterator_category = std::random_access_iterator_tag;
    using reference         = _sparse_element<value_type, _rank, _is_const>;
    using pointer           = reference*;

    using _my_type    = _sparse_iterator;
    using _deref_type = std::conditional_t<_is_const, value_type, reference>;
    using _idx_t      = typename reference::_idx_t;
    using _sparse_t   = typename reference::_sparse_t;
    using _column_t   = typename reference::_column_t;

    _sparse_iterator(_sparse_t sparse, _idx_t idx) :
        sparse_{sparse}, idx_{idx} {}

    _my_type& operator+=(ptrdiff_t diff)
    {
        this->inc(diff);
        return *this;
    }

    _my_type& operator-=(ptrdiff_t diff)
    {
        this->dec(diff);
        return *this;
    }

    _my_type operator+(ptrdiff_t diff) const
    {
        _my_type ret = *this;
        ret += diff;
        return ret;
    }

    _my_type operator-(ptrdiff_t diff) const
    {
        _my_type ret = *this;
        ret -= diff;
        return ret;
    }

    _my_type& operator++()
    {
        this->explicit_inc();
        return *this;
    }

    _my_type& operator--()
    {
        this->explicit_dec();
        return *this;
    }

    _my_type operator++(int)
    {
        _my_type ret = *this;
        ++(*this);
        return ret;
    }

    _my_type operator--(int)
    {
        _my_type ret = *this;
        --(*this);
        return ret;
    }

    template<size_t... Is>
    _deref_type _deref_impl(std::index_sequence<Is...>) const
    {
        return sparse_(this->idx_[Is]...);
    }

    _deref_type operator*() const
    {
        return this->_deref_impl(std::make_index_sequence<_rank>{});
    }

    _deref_type operator[](ptrdiff_t diff) const
    {
        return *((*this) + diff);
    }

    ptrdiff_t operator-(const _my_type& other) const
    {
        return this->difference(other);
    }

    bool operator==(const _my_type& other) const
    {
        return this->cmp_equal(other);
    }

    bool operator<(const _my_type& other) const
    {
        return this->cmp_less(other);
    }

    bool operator>(const _my_type& other) const
    {
        return this->cmp_greater(other);
    }

    bool operator!=(const _my_type& other) const
    {
        return !((*this) == other);
    }

    bool operator<=(const _my_type& other) const
    {
        return !((*this) > other);
    }

    bool operator>=(const _my_type& other) const
    {
        return !((*this) < other);
    }

    const _idx_t& _get_idx_cref() const
    {
        return this->idx_;
    }

private:
    template<typename Diff>
    void inc(Diff diff)
    {
        if constexpr (std::is_unsigned_v<Diff>)
            explicit_inc(diff);
        else if (diff >= 0)
            explicit_inc(diff);
        else
            explicit_dec(-diff);
    }

    template<typename Diff>
    void dec(Diff diff)
    {
        if constexpr (std::is_unsigned_v<Diff>)
            explicit_dec(diff);
        else if (diff >= 0)
            explicit_dec(diff);
        else
            explicit_inc(-diff);
    }

    // increment by 1 on Level
    template<size_t Level = _rank - 1>
    void explicit_inc()
    {
        const size_t dim = sparse_.dimension(Level);
        idx_[Level]++;
        if constexpr (Level > 0)
        {
            if (idx_[Level] < dim) // if did not overflow
            {
            }
            else // if did overflow
            {
                idx_[Level] = 0;
                explicit_inc<Level - 1>(); // carry
            }
        }
    }

    // increment by diff (diff >= 0) on Level
    template<size_t Level = _rank - 1>
    void explicit_inc(size_t diff)
    {
        const size_t dim = sparse_.dimension(Level);
        idx_[Level] += diff;
        if constexpr (Level > 0)
        {
            if (idx_[Level] < dim) // if did not overflow
            { // do nothing
            }
            else if (idx_[Level] < 2 * dim) // if did overflow only once
            {
                idx_[Level] -= dim;
                explicit_inc<Level - 1>(); // carry
            }
            else // if did overflow more than once
            {
                size_t val  = idx_[Level];
                size_t quot = val / dim;
                size_t rem  = val % dim;
                idx_[Level] = rem;
                explicit_inc<Level - 1>(quot); // carry
            }
        }
    }

    // decrement by 1 on Level
    template<size_t Level = _rank - 1>
    void explicit_dec()
    {
        const size_t dim = sparse_.dimension(Level);
        if constexpr (Level > 0)
        {
            if (idx_[Level] > 0) // if will not underflow
            {
                idx_[Level]--;
            }
            else
            {
                idx_[Level] = dim - 1;
                explicit_dec<Level - 1>(); // borrow
            }
        }
        else
        {
            idx_[Level]--;
        }
    }

    // decrement by diff (diff >= 0) on Level
    template<size_t Level = _rank - 1>
    void explicit_dec(size_t diff)
    {
        const size_t dim = sparse_.dimension(Level);
        ptrdiff_t post_sub = idx_[Level] - diff;
        if constexpr (Level > 0)
        {
            if (post_sub >= 0) // if will not underflow
            {
                idx_[Level] -= diff;
            }
            else if (size_t(-post_sub) <= dim) // if will underflow only once
            {
                idx_[Level] -= (diff - dim);
                explicit_dec<Level - 1>(); // carry
            }
            else // if underflow more than once
            {
                size_t val  = size_t(-post_sub) - 1;
                size_t quot = val / dim;
                size_t rem  = val % dim;
                idx_[Level] = dim - (rem + 1);
                explicit_dec<Level - 1>(quot);
            }
        }
        else
        {
            idx_[Level] = size_t(post_sub);
        }
    }

    // calculate the diff, where indices1 = indices2 + diff
    template<size_t Level = _rank - 1>
    ptrdiff_t difference(const _my_type& other) const
    {
        ptrdiff_t    diff = this->idx_[Level] - other.idx_[Level];
        const size_t dim  = sparse_.dimension(Level);
        if constexpr (Level == 0)
            return diff;
        else
            return dim * difference<Level - 1>(other) + diff;
    }

    template<size_t Level = 0>
    bool cmp_equal(const _my_type& other) const
    {
        bool equal = (this->idx_[Level] == other.idx_[Level]);
        if constexpr (Level + 1 < _rank)
            return equal && cmp_equal<Level + 1>(other);
        else
            return equal;
    }

    template<size_t Level = 0>
    bool cmp_less(const _my_type& other) const
    {
        if constexpr (Level + 1 < _rank)
        {
            ptrdiff_t diff = this->idx_[Level] - other.idx_[Level];
            if (diff < 0)
                return true;
            else if (diff > 0)
                return false;
            else
                return cmp_less<Level + 1>(other);
        }
        else
        {
            return this->idx_[Level] < other.idx_[Level];
        }
    }

    template<size_t Level = 0>
    bool cmp_greater(const _my_type& other) const
    {
        if constexpr (Level + 1 < _rank)
        {
            ptrdiff_t diff = this->idx_[Level] - other.idx_[Level];
            if (diff > 0)
                return true;
            else if (diff < 0)
                return false;
            else
                return cmp_greater<Level + 1>(other);
        }
        else
        {
            return this->idx_[Level] > other.idx_[Level];
        }
    }

private:
    _sparse_t sparse_;
    _idx_t    idx_;
};


enum class sparse_passing_by
{
    value,     //  Automatic   sparse_array<T,R>
    reference, // "Shared"     sparse_array<T,R>&
    constant,  // "Constant"   const sparse_array<T,R>(&)
    unknown
};

template<typename Sparse>
struct sparse_passing_category :
    std::integral_constant<sparse_passing_by, sparse_passing_by::unknown> {};
template<typename T, size_t Rank>
struct sparse_passing_category<sparse_array<T, Rank>> :
    std::integral_constant<sparse_passing_by, sparse_passing_by::value> {};
template<typename T, size_t Rank>
struct sparse_passing_category<sparse_array<T, Rank>&> :
    std::integral_constant<sparse_passing_by, sparse_passing_by::reference> {};
template<typename T, size_t Rank>
struct sparse_passing_category<const sparse_array<T, Rank>> :
    std::integral_constant<sparse_passing_by, sparse_passing_by::constant> {};
template<typename T, size_t Rank>
struct sparse_passing_category<const sparse_array<T, Rank>&> :
    std::integral_constant<sparse_passing_by, sparse_passing_by::constant> {};
template<typename Sparse>
constexpr sparse_passing_by sparse_passing_category_v = sparse_passing_category<Sparse>::value;



template<typename Arg>
auto transform_arg(MArgument arg)
{
    using scalar_arg_t = std::remove_const_t<Arg>;
    using tensor_arg_t = std::remove_reference_t<std::remove_const_t<Arg>>;
    using sprase_arg_t = std::remove_reference_t<std::remove_const_t<Arg>>;
    if constexpr (std::is_same_v<bool, scalar_arg_t>)
    {
        return static_cast<scalar_arg_t>(MArgument_getBoolean(arg));
    }
    else if constexpr (std::is_integral_v<scalar_arg_t>)
    {
        return static_cast<scalar_arg_t>(MArgument_getInteger(arg));
    }
    else if constexpr (std::is_floating_point_v<scalar_arg_t>)
    {
        return static_cast<scalar_arg_t>(MArgument_getReal(arg));
    }
    else if constexpr (std::is_same_v<std::string, scalar_arg_t>)
    {
        return static_cast<scalar_arg_t>(MArgument_getUTF8String(arg));
    }
    else if constexpr (std::is_same_v<const char*, scalar_arg_t>)
    {
        return static_cast<scalar_arg_t>(MArgument_getUTF8String(arg));
    }
    else if constexpr (is_std_complex_v<scalar_arg_t>)
    {
        const mcomplex& complex_arg = MArgument_getComplex(arg);
        return scalar_arg_t(mcreal(complex_arg), mcimag(complex_arg));
    }
    else if constexpr (tensor_passing_category_v<Arg> == tensor_passing_by::value)
    {
        return tensor_arg_t(MArgument_getMTensor(arg), memory_type::proxy);
    }
    else if constexpr (tensor_passing_category_v<Arg> == tensor_passing_by::constant)
    {
        return tensor_arg_t(MArgument_getMTensor(arg), memory_type::proxy);
    }
    else if constexpr (tensor_passing_category_v<Arg> == tensor_passing_by::reference)
    {
        return tensor_arg_t(MArgument_getMTensor(arg), memory_type::shared);
    }
    else if constexpr (sparse_passing_category_v<Arg> == sparse_passing_by::value)
    {
        return sprase_arg_t(MArgument_getMSparseArray(arg), memory_type::proxy);
    }
    else if constexpr (sparse_passing_category_v<Arg> == sparse_passing_by::constant)
    {
        return sprase_arg_t(MArgument_getMSparseArray(arg), memory_type::proxy);
    }
    else if constexpr (sparse_passing_category_v<Arg> == sparse_passing_by::reference)
    {
        return sprase_arg_t(MArgument_getMSparseArray(arg), memory_type::shared);
    }
    else
    {
        static_assert(_always_false_v<Arg>, "not a valid argument type");
    }
    return tensor_arg_t{};
}

template<typename Ret>
void submit_result(Ret&& result, MArgument mresult)
{
    if constexpr (std::is_same_v<bool, Ret>)
    {
        MArgument_setBoolean(mresult, result);
    }
    else if constexpr (std::is_integral_v<Ret>)
    {
        MArgument_setInteger(mresult, result);
    }
    else if constexpr (std::is_floating_point_v<Ret>)
    {
        MArgument_setReal(mresult, result);
    }
    else if constexpr (is_std_complex_v<Ret>)
    {
        mcomplex& complex_arg = MArgument_getComplex(mresult);
        mcreal(complex_arg) = result.real();
        mcimag(complex_arg) = result.imag();
    }
    else if constexpr (std::is_same_v<std::string, Ret>)
    {
        global_string_result = std::forward<Ret>(result);
        char* string_ptr = const_cast<char*>(global_string_result.c_str());
        MArgument_setUTF8String(mresult, string_ptr);
    }
    else if constexpr (std::is_same_v<const char*, Ret> || 
                       std::is_same_v<char*, Ret>)
    {
        global_string_result = std::string(result);
        char* string_ptr = const_cast<char*>(global_string_result.c_str());
        MArgument_setUTF8String(mresult, string_ptr);
    }
    else if constexpr (tensor_passing_category_v<Ret> == tensor_passing_by::value)
    {
        MTensor ret = std::forward<Ret>(result).get_mtensor();
        MArgument_setMTensor(mresult, ret);
    }
    else if constexpr (sparse_passing_category_v<Ret> == sparse_passing_by::value)
    {
        MSparseArray ret = std::forward<Ret>(result).get_msparse();
        MArgument_setMSparseArray(mresult, ret);
    }
    else
    {
        static_assert(_always_false_v<Ret>, "not a valid return type");
    }
}

template<typename... ArgTypes, size_t... Is>
auto get_args_impl(MArgument* args, std::index_sequence<Is...>)
{
    return std::make_tuple(transform_arg<ArgTypes>(args[Is])...);
}

template<typename... ArgTypes>
auto get_args(MArgument* args)
{
    return get_args_impl<ArgTypes...>(args, std::make_index_sequence<sizeof...(ArgTypes)>{});
}


template<typename Ret, typename... Args, typename ArgsTuple, size_t... Is>
auto tuple_invoke_impl(Ret fn(Args...), ArgsTuple& args, std::index_sequence<Is...>)
{
    return fn(static_cast<Args&&>(std::get<Is>(args))...);
}

template<typename Fn, typename ArgsTuple>
auto tuple_invoke(Fn&& fn, ArgsTuple& args)
{
    return tuple_invoke_impl(std::forward<Fn>(fn), args,
                             std::make_index_sequence<std::tuple_size_v<ArgsTuple>>{});
}

inline void handle_exception(const std::exception_ptr& eptr) noexcept
{
    try
    {
        WLL_ASSERT(bool(eptr)); // there should be an exception in eptr
        std::rethrow_exception(eptr);
    }
    catch (std::exception& std_error)
    {
        global_exception.error_type_ = -1; //LIBRARY_USER_ERROR
        global_exception.message_ = std::string("Standard Library Exception\n") + std_error.what();
    }
    catch (library_error& lib_error)
    {
        global_exception.error_type_ = lib_error.type();
        global_exception.message_ = std::string("Wolfram Library Exception\n") + lib_error.what();
    }
    catch (...)
    {
        global_exception.error_type_ = -1; //LIBRARY_USER_ERROR
        global_exception.message_ = std::string("Unknown Exception Type");
    }
}

template<typename Ret, typename... Args>
int library_eval(Ret fn(Args...), mint argc, MArgument* args, MArgument& mresult)
{
#ifndef WLL_DISABLE_EXCEPTION_HANDLING
    try
    {
#endif

        WLL_ASSERT(sizeof...(Args) == size_t(argc));
        static_assert(!std::is_reference_v<Ret>, "cannot return a reference type");
        auto args_tuple = get_args<Args...>(args);
        if constexpr (std::is_same_v<void, Ret>)
            tuple_invoke(fn, args_tuple);
        else
            submit_result(tuple_invoke(fn, args_tuple), mresult);

#ifndef WLL_DISABLE_EXCEPTION_HANDLING
    }
    catch (...)
    {
        handle_exception(std::current_exception());
        return global_exception.error_type_;
    }
#endif

    return LIBRARY_NO_ERROR;
}


inline bool has_abort() noexcept
{
    return bool(global_lib_data->AbortQ());
}

}

EXTERN_C DLLEXPORT mint WolframLibrary_getVersion()
{
    return WolframLibraryVersion;
}
EXTERN_C DLLEXPORT int WolframLibrary_initialize(WolframLibraryData lib_data)
{
    wll::global_lib_data  = lib_data;
    wll::global_sparse_fn = wll::global_lib_data->sparseLibraryFunctions;
    wll::global_exception = wll::exception_status{};
    wll::global_log.clear();
    return 0;
}
EXTERN_C DLLEXPORT void WolframLibrary_uninitialize(WolframLibraryData){ }
EXTERN_C DLLEXPORT int wll_exception_msg(WolframLibraryData, mint, MArgument*, MArgument res)
{
    MArgument_setUTF8String(res, const_cast<char*>(wll::global_exception.message_.c_str()));
    return LIBRARY_NO_ERROR;
}
EXTERN_C DLLEXPORT int wll_log_content(WolframLibraryData, mint, MArgument*, MArgument res)
{
    wll::global_log.update_string();
    MArgument_setUTF8String(res, const_cast<char*>(wll::global_log.string_.c_str()));
    return LIBRARY_NO_ERROR;
}
EXTERN_C DLLEXPORT int wll_log_clear(WolframLibraryData, mint, MArgument*, MArgument res)
{
    wll::global_log.clear();
    return LIBRARY_NO_ERROR;
}

#define DEFINE_WLL_FUNCTION(fn)                                                                 \
EXTERN_C DLLEXPORT int wll_##fn(WolframLibraryData, mint argc, MArgument* args, MArgument res)  \
{                                                                                               \
    return wll::library_eval(fn, argc, args, res);                                              \
}
