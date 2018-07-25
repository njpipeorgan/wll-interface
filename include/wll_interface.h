#pragma once

#include <cassert>
#include <array>
#include <complex>
#include <exception>
#include <numeric>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>

#include "WolframLibrary.h"

namespace wll
{

#ifdef NDEBUG
#define WLL_ASSERT(x) ((void)0)
#else
#define WLL_ASSERT(x) assert((x))
#endif

#if defined(__GNUC__)
static_assert(__GNUC__ >= 7, "");
#define WLL_CURRENT_FUNCTION __PRETTY_FUNCTION__
#elif defined(__INTEL_COMPILER)
static_assert(__INTEL_COMPILER >= 1900, "");
#define WLL_CURRENT_FUNCTION __FUNCTION__
#elif defined(__clang__)
static_assert(__clang_major__ >= 4, "");
#define WLL_CURRENT_FUNCTION __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
static_assert(_MSC_VER >= 1911, "");
#define WLL_CURRENT_FUNCTION __FUNCSIG__
#endif


struct exception_status
{
    int error_type_ = LIBRARY_NO_ERROR;
    std::string message_{};
};

struct library_error
{
    library_error(int type, std::string message ={}) :
        type_{type}, message_{message} {}

    int type() const { return type_; }
    std::string what() const { return message_; }

    int type_;
    std::string message_;
};
struct library_type_error : library_error
{
    library_type_error(std::string message ={}) :
        library_error(LIBRARY_TYPE_ERROR, message) {}
};
struct library_rank_error : library_error
{
    library_rank_error(std::string message ={}) :
        library_error(LIBRARY_RANK_ERROR, message) {}
};
struct library_dimension_error : library_error
{
    library_dimension_error(std::string message ={}) :
        library_error(LIBRARY_DIMENSION_ERROR, message) {}
};
struct library_numerical_error : library_error
{
    library_numerical_error(std::string message ={}) :
        library_error(LIBRARY_NUMERICAL_ERROR, message) {}
};
struct library_memory_error : library_error
{
    library_memory_error(std::string message ={}) :
        library_error(LIBRARY_MEMORY_ERROR, message) {}
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

exception_status   global_exception;
WolframLibraryData global_lib_data;
log_stringstream_t global_log;
std::string        global_string_result;


#ifdef NDEBUG
#define WLL_DEBUG_EXECUTE(any) ((void)0)
#else
#define WLL_DEBUG_EXECUTE(any) (any)
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

template<typename>
constexpr bool _always_false_v = false;


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
class tensor
{
public:
    using value_type   = T;
    static constexpr size_t _rank = Rank;
    using _ptr_t       = value_type*;
    using _const_ptr_t = const value_type*;
    using _dims_t      = std::array<size_t, _rank>;
    static_assert(_rank > 0, "");

    tensor() noexcept = default;

    tensor(MTensor mtensor, memory_type access) :
        mtensor_{mtensor}, access_{access}
    {
        WLL_ASSERT(_rank == global_lib_data->MTensor_getRank(mtensor));
        WLL_ASSERT(access_ == memory_type::owned ||
                   access_ == memory_type::proxy ||
                   access_ == memory_type::shared);

        size_ = global_lib_data->MTensor_getFlattenedLength(mtensor);
        const mint* dims_ptr = global_lib_data->MTensor_getDimensions(mtensor);
        for (size_t i = 0; i < _rank; ++i)
            dims_[i] = size_t(dims_ptr[i]);

        void* src_ptr = nullptr;
        bool  do_copy = false;
        int   mtype   = int(global_lib_data->MTensor_getType(mtensor));
        WLL_ASSERT(mtype == MType_Integer ||
                   mtype == MType_Real    ||
                   mtype == MType_Complex);

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
            WLL_DEBUG_EXECUTE(global_log << "tensor(MTensor) with data copy\n");
            ptr_ = reinterpret_cast<_ptr_t>(malloc(size_ * sizeof(value_type)));
            if (ptr_ == nullptr)
                throw library_memory_error(WLL_CURRENT_FUNCTION"\nmalloc failed when copying data.");
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
            WLL_DEBUG_EXECUTE(global_log << "tensor(MTensor) with no data copy, "
                              << (access_ == memory_type::proxy ? "access == proxy\n" : "access == shared\n"));
            ptr_ = reinterpret_cast<_ptr_t>(src_ptr);
        }
    }

    tensor(_dims_t dims, memory_type access = memory_type::owned) :
        dims_{dims}, size_{_flattened_size(dims)}, access_{access}
    {
        WLL_ASSERT(access_ == memory_type::owned ||
                   access_ == memory_type::manual);
        if (access_ == memory_type::owned)
        {
            WLL_DEBUG_EXECUTE(global_log << "tensor(_dims_t) with data copy\n");
            ptr_ = reinterpret_cast<_ptr_t>(calloc(size_, sizeof(value_type)));
            if (ptr_ == nullptr)
                throw library_memory_error(WLL_CURRENT_FUNCTION"\ncalloc failed, access_ == owned.");
        }
        else // access_ == memory_type::manual
        {
            //throw library_dimension_error(std::to_string(dims[0]));
            constexpr int mtype = derive_tensor_data_type<value_type>::strict_type_v;
            if (mtype == MType_Void)
                throw library_type_error(WLL_CURRENT_FUNCTION"\nvalue_type cannot be strictly matched to any MType.");
            std::array<mint, _rank> mt_dims;
            for (size_t i = 0; i < _rank; ++i)
                mt_dims[i] = dims_[i];
            WLL_DEBUG_EXECUTE(global_log << "tensor(_dims_t, ::manual)\n");
            int err = global_lib_data->MTensor_new(mtype, _rank, mt_dims.data(), &mtensor_);
            if (err != LIBRARY_NO_ERROR)
                throw library_error(err, WLL_CURRENT_FUNCTION"\nMTensor_new() failed.");
            if constexpr (mtype == MType_Integer)
                ptr_ = reinterpret_cast<_ptr_t>(global_lib_data->MTensor_getIntegerData(mtensor_));
            else if constexpr (mtype == MType_Real)
                ptr_ = reinterpret_cast<_ptr_t>(global_lib_data->MTensor_getRealData(mtensor_));
            else // mtype == MType_Complex
                ptr_ = reinterpret_cast<_ptr_t>(global_lib_data->MTensor_getComplexData(mtensor_));
        }
    }

    template<typename T>
    tensor(std::initializer_list<T> dims, memory_type access = memory_type::owned) :
        tensor(_convert_to_dims_array(dims), access) {}

    tensor(const tensor& other) :
        dims_{other.dims_}, size_{other.size_}, access_{memory_type::owned}
    {
        WLL_DEBUG_EXECUTE(global_log << "tensor(const tensor&) with data copy\n");
        ptr_ = reinterpret_cast<_ptr_t>(malloc(size_ * sizeof(value_type)));
        if (ptr_ == nullptr)
            throw library_memory_error(WLL_CURRENT_FUNCTION"\nmalloc failed when copying data.");
        _data_copy_n(other.ptr_, size_, ptr_);
    }

    tensor(tensor&& other) :
        dims_{other.dims_}, size_{other.size_}
    {
        std::swap(ptr_, other.ptr_);
        std::swap(access_, other.access_);
        std::swap(mtensor_, other.mtensor_);
        WLL_DEBUG_EXECUTE(global_log << "tensor(tensor&&) with member swap\n");
    }

    template<typename OtherType>
    tensor(const tensor<OtherType, _rank>& other) :
        dims_{other.dims_}, size_{other.size_}, access_{memory_type::owned}
    {
        WLL_DEBUG_EXECUTE(global_log << "tensor(tensor&&) with data copy, difference types\n");
        ptr_ = reinterpret_cast<_ptr_t>(malloc(size_ * sizeof(value_type)));
        if (ptr_ == nullptr)
            throw library_memory_error(WLL_CURRENT_FUNCTION"\nmalloc failed when copying data.");
        _data_copy_n(other.ptr_, size_, ptr_);
    }

    template<typename OtherType>
    tensor(tensor<OtherType, _rank>&& other) :
        dims_{other.dims_}, size_{other.size_}, access_{memory_type::owned}
    {
        WLL_DEBUG_EXECUTE(global_log << "tensor(tensor&&) with data copy, difference types\n");
        ptr_ = reinterpret_cast<_ptr_t>(malloc(size_ * sizeof(value_type)));
        if (ptr_ == nullptr)
            throw library_memory_error(WLL_CURRENT_FUNCTION"\nmalloc failed when copying data.");
        _data_copy_n(other.ptr_, size_, ptr_);
    }

    tensor& operator=(const tensor& other)
    {
        WLL_ASSERT(other.access_ != memory_type::empty); // other is empty
        WLL_ASSERT(this->access_ != memory_type::empty); // *this is empty
        if (this->ptr_ != other.ptr_)
        {
            if (!(this->_has_same_dims(other.dims_.data())))
                throw library_dimension_error(WLL_CURRENT_FUNCTION"\ntensors have different dimensions.");
            WLL_DEBUG_EXECUTE(global_log << "operator=(const tensor&) with data copy\n");
            _data_copy_n(other.ptr_, size_, ptr_);
        }
        else
        {
            WLL_DEBUG_EXECUTE(global_log << "operator=(const tensor&) do nothing\n");
        }
        return *this;
    }

    tensor& operator=(tensor&& other)
    {
        WLL_ASSERT(other.access_ != memory_type::empty); // other is empty
        WLL_ASSERT(this->access_ != memory_type::empty); // *this is empty
        if (this->ptr_ != other.ptr_)
        {
            if (!(this->_has_same_dims(other.dims_.data())))
                throw library_dimension_error(WLL_CURRENT_FUNCTION"\ntensors have different dimensions.");
            if (other.access_ == memory_type::proxy  ||
                other.access_ == memory_type::shared ||
                this->access_ == memory_type::proxy  ||
                this->access_ == memory_type::shared)
            {
                WLL_DEBUG_EXECUTE(global_log << "operator=(tensor&&) with data copy\n");
                _data_copy_n(other.ptr_, size_, ptr_);
            }
            else
            {
                WLL_DEBUG_EXECUTE(global_log << "operator=(tensor&&) with pointer swap\n");
                this->_swap_pointers(std::move(other));
            }
        }
        else
        {
            WLL_DEBUG_EXECUTE(global_log << "operator=(tensor&&) do nothing\n");
        }
        return *this;
    }

    template<typename OtherType>
    tensor& operator=(const tensor<OtherType, _rank>& other)
    {
        WLL_ASSERT(other.access_ != memory_type::empty); // other is empty
        WLL_ASSERT(this->access_ != memory_type::empty); // *this is empty
        // tensors with difference value_type should not have the same ptr_
        WLL_ASSERT((void*)(this->ptr_) != (void*)(other.ptr_));
        WLL_DEBUG_EXECUTE(global_log << "operator=(tensor&&) with data copy, different types\n");
        _data_copy_n(other.ptr_, size_, ptr_);
        return *this;
    }

    template<typename OtherType>
    tensor& operator=(tensor<OtherType, _rank>&& other)
    {
        WLL_ASSERT(other.access_ != memory_type::empty); // other is empty
        WLL_ASSERT(this->access_ != memory_type::empty); // *this is empty
        // tensors with difference value_type should not have the same ptr_
        WLL_ASSERT((void*)(this->ptr_) != (void*)(other.ptr_));
        WLL_DEBUG_EXECUTE(global_log << "operator=(tensor&&) with data copy, different types\n");
        _data_copy_n(other.ptr_, size_, ptr_);
        return *this;
    }

    tensor clone(memory_type access = memory_type::owned) const
    {
        WLL_ASSERT(this->access_ != memory_type::empty); // cannot clone an empty tensor
        WLL_ASSERT(access == memory_type::owned || access == memory_type::manual);
        WLL_DEBUG_EXECUTE(global_log << "tensor::clone()\n");
        tensor ret(this->dims_, access);
        _data_copy_n(ptr_, size_, ret.ptr_);
        WLL_DEBUG_EXECUTE(global_log << "leaving tensor::clone()\n");
        return ret;
    }

    ~tensor()
    {
        WLL_DEBUG_EXECUTE(global_log << "tensor::~tensor()\n");
        this->_destroy();
        WLL_DEBUG_EXECUTE(global_log << "leaving tensor::~tensor()\n");
    }

    constexpr size_t rank() const noexcept
    {
        return _rank;
    }

    size_t size() const noexcept
    {
        return size_;
    }

    _dims_t dimensions() const noexcept
    {
        return dims_;
    }

    size_t dimension(size_t level) const noexcept
    {
        return dims_[level];
    }

    template<typename... Idx>
    value_type at(Idx... idx) const
    {
        return (*this)[_get_flat_idx(std::make_tuple(idx...))];
    }

    template<typename... Idx>
    value_type& at(Idx... idx)
    {
        return (*this)[_get_flat_idx(std::make_tuple(idx...))];
    }

    template<typename... Idx>
    value_type operator()(Idx... idx) const
    {
        return (*this)[_get_flat_idx_unsafe(std::make_tuple(idx...))];
    }

    template<typename... Idx>
    value_type& operator()(Idx... idx)
    {
        return (*this)[_get_flat_idx_unsafe(std::make_tuple(idx...))];
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

    _const_ptr_t cbegin() const noexcept
    {
        WLL_ASSERT(ptr_ != nullptr);
        return ptr_;
    }

    _const_ptr_t cend() const noexcept
    {
        return this->cbegin() + size_;
    }

    _const_ptr_t begin() const noexcept
    {
        return this->cbegin();
    }

    _const_ptr_t end() const noexcept
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

    MTensor get_mtensor() const &
    {
        return _get_mtensor_lvalue();
    }

    MTensor get_mtensor() &&
    {
        return _get_mtensor_rvalue();
    }

    template<typename InputIter>
    void copy_data_from(InputIter src, size_t count)
    {
        WLL_ASSERT(count == size_);
        if (count > 0)
        {
            _ptr_t dest = this->ptr_;
            for (size_t i = 0; i < count; ++i, ++src, ++dest)
                *dest = _mtype_cast<value_type>(*src);
        }
    }

    template<typename OutputIter>
    void copy_data_to(OutputIter dest, size_t count)
    {
        WLL_ASSERT(count == size_);
        if (count > 0)
        {
            _ptr_t src = this->ptr_;
            for (size_t i = 0; i < count; ++i, ++src, ++dest)
                *dest = _mtype_cast<decltype(*dest)>(*src);
        }

    }

private:
    static size_t _flattened_size(_dims_t dims) noexcept
    {
        size_t size = 1;
        for (size_t i = 0; i < _rank; ++i)
            size *= dims[i];
        return size;
    }

    template<typename T>
    static _dims_t _convert_to_dims_array(std::initializer_list<T> dims)
    {
        static_assert(std::is_integral_v<T>, "dimensions should be of integral types");
        WLL_ASSERT(dims.size() == _rank);
        _dims_t dims_array;
        for (size_t i = 0; i < _rank; ++i)
            dims_array[i] = size_t(*(dims.begin() + i));
        return dims_array;
    }

    template<typename Target, typename Source>
    static Target _mtype_cast(Source value)
    {
        if constexpr (is_std_complex_v<Target>)
        {
            return Target(static_cast<typename Target::value_type>(value),
                          static_cast<typename Target::value_type>(0));
        }
        else if constexpr (std::is_same_v<mcomplex, Target>)
        {
            mcomplex result;
            mcreal(result) = static_cast<mreal>(value.real());
            mcimag(result) = static_cast<mreal>(0);
            return result;
        }
        else
        {
            return static_cast<Target>(value);
        }
    }

    template<typename Target>
    static Target _mtype_cast(mcomplex value)
    {
        if constexpr (is_std_complex_v<Target>)
        {
            return Target(static_cast<typename Target::value_type>(mcreal(value)),
                          static_cast<typename Target::value_type>(mcimag(value)));
        }
        else if constexpr (std::is_same_v<mcomplex, Target>)
        {
            return value;
        }
        else
        {
            WLL_ASSERT(false); // complex type can only be converted to complex type
            return Target{};
        }
    }

    template<typename Target, typename T>
    static Target _mtype_cast(std::complex<T> value)
    {
        if constexpr (is_std_complex_v<Target>)
        {
            return Target(static_cast<typename Target::value_type>(value.real()),
                          static_cast<typename Target::value_type>(value.imag()));
        }
        else if constexpr (std::is_same_v<mcomplex, Target>)
        {
            mcomplex result;
            mcreal(result) = static_cast<mreal>(value.real());
            mcimag(result) = static_cast<mreal>(value.imag());
            return result;
        }
        else
        {
            WLL_ASSERT(false); // complex type can only be converted to complex type
            return Target{};
        }
    }

    template<typename SrcType, typename DestType>
    static void _data_copy_n(const SrcType* src_ptr, size_t count, DestType* dest_ptr)
    {
        WLL_DEBUG_EXECUTE(global_log << "tensor::data_copy_n()\n");
        WLL_ASSERT(src_ptr != nullptr && dest_ptr != nullptr);
        WLL_ASSERT((void*)src_ptr != (void*)dest_ptr); // cannot copy to itself
        if (count > 0)
            for (size_t i = 0; i < count; ++i)
                dest_ptr[i] = _mtype_cast<DestType>(src_ptr[i]);
        WLL_DEBUG_EXECUTE(global_log << "leaving tensor::data_copy_n()\n");
    }

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
        WLL_DEBUG_EXECUTE(global_log << "tensor::_destroy(), access_ == " << int(this->access_) << '\n');
        if (access_ == memory_type::empty)
        {
            WLL_ASSERT(mtensor_ == nullptr);
            WLL_ASSERT(ptr_ == nullptr);
        }
        else if (access_ == memory_type::owned)
        {
            WLL_ASSERT(mtensor_ == nullptr);
            WLL_DEBUG_EXECUTE(global_log << "free @ " << std::hex << reinterpret_cast<uint64_t>(ptr_) << std::dec);
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
        WLL_DEBUG_EXECUTE(global_log << "leaving tensor::_destroy()\n");
    }

    void _release_ownership()
    {
        // destruct the object without free resource, manual tensor only
        WLL_ASSERT(access_ == memory_type::manual);
        WLL_DEBUG_EXECUTE(global_log << "tensor::release_ownership()\n");
        ptr_     = nullptr;
        mtensor_ = nullptr;
        access_  = memory_type::empty;
    }

    MTensor _get_mtensor_lvalue() const
    {
        WLL_ASSERT(access_ != memory_type::empty);
        WLL_DEBUG_EXECUTE(global_log << "tensor::_get_mtensor_lvalue()\n");

        std::array<mint, _rank> mt_dims;
        for (size_t i = 0; i < _rank; ++i)
            mt_dims[i] = dims_[i];

        using mtype = typename derive_tensor_data_type<value_type>::convert_type;
        constexpr int mtype_v = derive_tensor_data_type<value_type>::convert_type_v;
        static_assert(!std::is_same_v<void, mtype>, "invalid data type to convert to MType");

        MTensor ret_tensor = nullptr;
        int err = global_lib_data->MTensor_new(mtype_v, _rank, mt_dims.data(), &ret_tensor);
        if (err != LIBRARY_NO_ERROR)
            throw library_error(err, WLL_CURRENT_FUNCTION"\nMTensor_new() failed.");
        if constexpr (mtype_v == MType_Integer)
            _data_copy_n(ptr_, size_, global_lib_data->MTensor_getIntegerData(ret_tensor));
        else if constexpr (mtype_v == MType_Real)
            _data_copy_n(ptr_, size_, global_lib_data->MTensor_getRealData(ret_tensor));
        else  // mtype_v == MType_Complex
            _data_copy_n(ptr_, size_, global_lib_data->MTensor_getComplexData(ret_tensor));
        WLL_DEBUG_EXECUTE(global_log << "leaving tensor::_get_mtensor_lvalue()\n");
        return ret_tensor;
    }

    MTensor _get_mtensor_rvalue()
    {
        WLL_DEBUG_EXECUTE(global_log << "tensor::_get_mtensor_rvalue()\n");
        WLL_ASSERT(access_ != memory_type::empty);
        if (access_ == memory_type::manual)
        {
            // return the containing mtensor and destroy *this
            MTensor ret = this->mtensor_;
            this->_release_ownership();
            WLL_DEBUG_EXECUTE(global_log << "leaving tensor::_get_mtensor_rvalue(), access = manual\n");
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
        WLL_DEBUG_EXECUTE(global_log << "tensor::_swap_pointers()\n");
        WLL_ASSERT(this->access_ == memory_type::owned ||
                   this->access_ == memory_type::manual); // only tensors own data can swap
        WLL_ASSERT(other.access_ == memory_type::owned ||
                   other.access_ == memory_type::manual);
        if (!(this->_has_same_dims(other.dims_.data())))
            throw library_dimension_error(WLL_CURRENT_FUNCTION"\ntensors have different dimensions.");
        std::swap(this->ptr_, other.ptr_);
        std::swap(this->mtensor_, other.mtensor_);
        std::swap(this->access_, other.access_);
        WLL_DEBUG_EXECUTE(global_log << "leaving tensor::_swap_pointers()\n");
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
            throw std::out_of_range(WLL_CURRENT_FUNCTION"\nindex out of range");
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

private:
    _dims_t dims_;
    size_t  size_;
    _ptr_t  ptr_ = nullptr;
    MTensor mtensor_ = nullptr;
    memory_type access_ = memory_type::empty;
};

template<typename T>
using list = tensor<T, 1>;
template<typename T>
using matrix = tensor<T, 2>;


enum tensor_passing_by
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


template<typename Arg>
auto transform_arg(MArgument arg)
{
    using scalar_arg_t = std::remove_const_t<Arg>;
    using tensor_arg_t = std::remove_reference_t<std::remove_const_t<Arg>>;
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
    else
    {
        static_assert(_always_false_v<Arg>, "not a valid argument type");
    }
}

template<typename Ret>
void submit_result(Ret&& result, MArgument mresult)
{
    WLL_DEBUG_EXECUTE(global_log << "submit_result()\n");
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
        global_string_result = std::move(result);
        char* string_ptr = const_cast<char*>(global_string_result.c_str());
        MArgument_setUTF8String(mresult, string_ptr);
    }
    else if constexpr (std::is_same_v<const char*, Ret>)
    {
        global_string_result = std::string(result);
        char* string_ptr = const_cast<char*>(global_string_result.c_str());
        MArgument_setUTF8String(mresult, string_ptr);
    }
    else if constexpr (tensor_passing_category_v<Ret> == tensor_passing_by::value)
    {
        MTensor ret = std::move(result).get_mtensor();
        WLL_DEBUG_EXECUTE(global_log << "rank == " << global_lib_data->MTensor_getRank(ret) << '\n'
                          << "size == " << global_lib_data->MTensor_getFlattenedLength(ret) << '\n');
        MArgument_setMTensor(mresult, ret);
    }
    else
    {
        static_assert(_always_false_v<Ret>, "not a valid return type");
    }
    WLL_DEBUG_EXECUTE(global_log << "leaving submit_result()\n");
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
    WLL_DEBUG_EXECUTE(global_log << "tuple_invoke()\n");
    return tuple_invoke_impl(std::forward<Fn>(fn), args,
                             std::make_index_sequence<std::tuple_size_v<ArgsTuple>>{});
}

inline void handle_exception(std::exception_ptr eptr) noexcept
{
    try
    {
        WLL_ASSERT(bool(eptr)); // there should be an exception in eptr
        std::rethrow_exception(eptr);
    }
    catch (std::exception& std_error)
    {
        global_exception.error_type_ = LIBRARY_FUNCTION_ERROR;
        global_exception.message_ = std::string("Standard Library Exception\n") + std_error.what();
    }
    catch (library_error& lib_error)
    {
        global_exception.error_type_ = lib_error.type();
        global_exception.message_ = std::string("Wolfram Library Exception\n") + lib_error.what();
    }
    catch (...)
    {
        global_exception.error_type_ = LIBRARY_FUNCTION_ERROR;
        global_exception.message_ = std::string("Unknown Exception Type");
    }
}

template<typename Ret, typename... Args>
int library_eval(Ret fn(Args...), mint argc, MArgument* args, MArgument& mresult)
{
    try
    {
        WLL_ASSERT(sizeof...(Args) == size_t(argc));
        static_assert(!std::is_reference_v<Ret>, "cannot return a reference type");
        auto args_tuple = get_args<Args...>(args);
        if constexpr (std::is_same_v<void, Ret>)
            tuple_invoke(fn, args_tuple);
        else
            submit_result(tuple_invoke(fn, args_tuple), mresult);
    }
    catch (...)
    {
        handle_exception(std::current_exception());
        return global_exception.error_type_;
    }
    return LIBRARY_NO_ERROR;
}

}


EXTERN_C DLLEXPORT inline mint WolframLibrary_getVersion()
{
    return WolframLibraryVersion;
}
EXTERN_C DLLEXPORT inline int  WolframLibrary_initialize(WolframLibraryData lib_data)
{
    wll::global_lib_data  = lib_data;
    wll::global_exception = wll::exception_status{};
    wll::global_log.clear();
    return 0;
}
EXTERN_C DLLEXPORT inline void WolframLibrary_uninitialize(WolframLibraryData)
{
    return;
}

EXTERN_C DLLEXPORT inline int  wll_exception_msg(WolframLibraryData, mint, MArgument*, MArgument res)
{
    MArgument_setUTF8String(res, const_cast<char*>(wll::global_exception.message_.c_str()));
    return LIBRARY_NO_ERROR;
}
EXTERN_C DLLEXPORT inline int  wll_log_content(WolframLibraryData, mint, MArgument*, MArgument res)
{
    wll::global_log.update_string();
    MArgument_setUTF8String(res, const_cast<char*>(wll::global_log.string_.c_str()));
    return LIBRARY_NO_ERROR;
}
EXTERN_C DLLEXPORT inline int  wll_log_clear(WolframLibraryData, mint, MArgument*, MArgument res)
{
    wll::global_log.clear();
    return LIBRARY_NO_ERROR;
}

#define DEFINE_WLL_FUNCTION(fn)                                    \
EXTERN_C DLLEXPORT int wll_##fn(                                   \
    WolframLibraryData, mint argc, MArgument* args, MArgument res) \
{                                                                  \
    return wll::library_eval(fn, argc, args, res);                 \
}

