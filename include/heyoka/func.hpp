// Copyright 2020, 2021 Francesco Biscani (bluescarni@gmail.com), Dario Izzo (dario.izzo@gmail.com)
//
// This file is part of the heyoka library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef HEYOKA_FUNC_HPP
#define HEYOKA_FUNC_HPP

#include <heyoka/config.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(HEYOKA_HAVE_REAL128)

#include <mp++/real128.hpp>

#endif

#include <heyoka/detail/fwd_decl.hpp>
#include <heyoka/detail/llvm_fwd.hpp>
#include <heyoka/detail/type_traits.hpp>
#include <heyoka/detail/visibility.hpp>
#include <heyoka/exceptions.hpp>

namespace heyoka
{

class HEYOKA_DLL_PUBLIC func_base
{
    std::string m_name;
    std::vector<expression> m_args;

public:
    explicit func_base(std::string, std::vector<expression>);

    func_base(const func_base &);
    func_base(func_base &&) noexcept;

    func_base &operator=(const func_base &);
    func_base &operator=(func_base &&) noexcept;

    ~func_base();

    const std::string &get_name() const;
    const std::vector<expression> &args() const;
    std::pair<std::vector<expression>::iterator, std::vector<expression>::iterator> get_mutable_args_it();
};

namespace detail
{

struct HEYOKA_DLL_PUBLIC func_inner_base {
    virtual ~func_inner_base();
    virtual std::unique_ptr<func_inner_base> clone() const = 0;

    virtual std::type_index get_type_index() const = 0;
    virtual const void *get_ptr() const = 0;
    virtual void *get_ptr() = 0;

    virtual const std::string &get_name() const = 0;
    virtual void to_stream(std::ostream &) const = 0;

    virtual const std::vector<expression> &args() const = 0;
    virtual std::pair<std::vector<expression>::iterator, std::vector<expression>::iterator> get_mutable_args_it() = 0;

    virtual llvm::Value *codegen_dbl(llvm_state &, const std::vector<llvm::Value *> &) const = 0;
    virtual llvm::Value *codegen_ldbl(llvm_state &, const std::vector<llvm::Value *> &) const = 0;
#if defined(HEYOKA_HAVE_REAL128)
    virtual llvm::Value *codegen_f128(llvm_state &, const std::vector<llvm::Value *> &) const = 0;
#endif

    virtual expression diff(const std::string &) const = 0;

    virtual double eval_dbl(const std::unordered_map<std::string, double> &, const std::vector<double> &) const = 0;
    virtual void eval_batch_dbl(std::vector<double> &, const std::unordered_map<std::string, std::vector<double>> &,
                                const std::vector<double> &) const = 0;
    virtual double eval_num_dbl(const std::vector<double> &) const = 0;
    virtual double deval_num_dbl(const std::vector<double> &, std::vector<double>::size_type) const = 0;

    virtual std::vector<std::pair<expression, std::vector<std::uint32_t>>>::size_type
    taylor_decompose(std::vector<std::pair<expression, std::vector<std::uint32_t>>> &) && = 0;
    virtual llvm::Value *taylor_diff_dbl(llvm_state &, const std::vector<std::uint32_t> &,
                                         const std::vector<llvm::Value *> &, llvm::Value *, llvm::Value *,
                                         std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) const = 0;
    virtual llvm::Value *taylor_diff_ldbl(llvm_state &, const std::vector<std::uint32_t> &,
                                          const std::vector<llvm::Value *> &, llvm::Value *, llvm::Value *,
                                          std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) const = 0;
#if defined(HEYOKA_HAVE_REAL128)
    virtual llvm::Value *taylor_diff_f128(llvm_state &, const std::vector<std::uint32_t> &,
                                          const std::vector<llvm::Value *> &, llvm::Value *, llvm::Value *,
                                          std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) const = 0;
#endif
    virtual llvm::Function *taylor_c_diff_func_dbl(llvm_state &, std::uint32_t, std::uint32_t) const = 0;
    virtual llvm::Function *taylor_c_diff_func_ldbl(llvm_state &, std::uint32_t, std::uint32_t) const = 0;
#if defined(HEYOKA_HAVE_REAL128)
    virtual llvm::Function *taylor_c_diff_func_f128(llvm_state &, std::uint32_t, std::uint32_t) const = 0;
#endif
};

template <typename T>
using func_to_stream_t
    = decltype(std::declval<std::add_lvalue_reference_t<const T>>().to_stream(std::declval<std::ostream &>()));

template <typename T>
inline constexpr bool func_has_to_stream_v = std::is_same_v<detected_t<func_to_stream_t, T>, void>;

template <typename T>
using func_codegen_dbl_t = decltype(std::declval<std::add_lvalue_reference_t<const T>>().codegen_dbl(
    std::declval<llvm_state &>(), std::declval<const std::vector<llvm::Value *> &>()));

template <typename T>
inline constexpr bool func_has_codegen_dbl_v = std::is_same_v<detected_t<func_codegen_dbl_t, T>, llvm::Value *>;

template <typename T>
using func_codegen_ldbl_t = decltype(std::declval<std::add_lvalue_reference_t<const T>>().codegen_ldbl(
    std::declval<llvm_state &>(), std::declval<const std::vector<llvm::Value *> &>()));

template <typename T>
inline constexpr bool func_has_codegen_ldbl_v = std::is_same_v<detected_t<func_codegen_ldbl_t, T>, llvm::Value *>;

#if defined(HEYOKA_HAVE_REAL128)

template <typename T>
using func_codegen_f128_t = decltype(std::declval<std::add_lvalue_reference_t<const T>>().codegen_f128(
    std::declval<llvm_state &>(), std::declval<const std::vector<llvm::Value *> &>()));

template <typename T>
inline constexpr bool func_has_codegen_f128_v = std::is_same_v<detected_t<func_codegen_f128_t, T>, llvm::Value *>;

#endif

template <typename T>
using func_diff_t
    = decltype(std::declval<std::add_lvalue_reference_t<const T>>().diff(std::declval<const std::string &>()));

template <typename T>
inline constexpr bool func_has_diff_v = std::is_same_v<detected_t<func_diff_t, T>, expression>;

template <typename T>
using func_eval_dbl_t = decltype(std::declval<std::add_lvalue_reference_t<const T>>().eval_dbl(
    std::declval<const std::unordered_map<std::string, double> &>(), std::declval<const std::vector<double> &>()));

template <typename T>
inline constexpr bool func_has_eval_dbl_v = std::is_same_v<detected_t<func_eval_dbl_t, T>, double>;

template <typename T>
using func_eval_batch_dbl_t = decltype(std::declval<std::add_lvalue_reference_t<const T>>().eval_batch_dbl(
    std::declval<std::vector<double> &>(), std::declval<const std::unordered_map<std::string, std::vector<double>> &>(),
    std::declval<const std::vector<double> &>()));

template <typename T>
inline constexpr bool func_has_eval_batch_dbl_v = std::is_same_v<detected_t<func_eval_batch_dbl_t, T>, void>;

template <typename T>
using func_eval_num_dbl_t = decltype(
    std::declval<std::add_lvalue_reference_t<const T>>().eval_num_dbl(std::declval<const std::vector<double> &>()));

template <typename T>
inline constexpr bool func_has_eval_num_dbl_v = std::is_same_v<detected_t<func_eval_num_dbl_t, T>, double>;

template <typename T>
using func_deval_num_dbl_t = decltype(std::declval<std::add_lvalue_reference_t<const T>>().deval_num_dbl(
    std::declval<const std::vector<double> &>(), std::declval<std::vector<double>::size_type>()));

template <typename T>
inline constexpr bool func_has_deval_num_dbl_v = std::is_same_v<detected_t<func_deval_num_dbl_t, T>, double>;

template <typename T>
using func_taylor_decompose_t = decltype(std::declval<std::add_rvalue_reference_t<T>>().taylor_decompose(
    std::declval<std::vector<std::pair<expression, std::vector<std::uint32_t>>> &>()));

template <typename T>
inline constexpr bool func_has_taylor_decompose_v
    = std::is_same_v<detected_t<func_taylor_decompose_t, T>,
                     std::vector<std::pair<expression, std::vector<std::uint32_t>>>::size_type>;

template <typename T>
using func_taylor_diff_dbl_t = decltype(std::declval<std::add_lvalue_reference_t<const T>>().taylor_diff_dbl(
    std::declval<llvm_state &>(), std::declval<const std::vector<std::uint32_t> &>(),
    std::declval<const std::vector<llvm::Value *> &>(), std::declval<llvm::Value *>(), std::declval<llvm::Value *>(),
    std::declval<std::uint32_t>(), std::declval<std::uint32_t>(), std::declval<std::uint32_t>(),
    std::declval<std::uint32_t>()));

template <typename T>
inline constexpr bool func_has_taylor_diff_dbl_v = std::is_same_v<detected_t<func_taylor_diff_dbl_t, T>, llvm::Value *>;

template <typename T>
using func_taylor_diff_ldbl_t = decltype(std::declval<std::add_lvalue_reference_t<const T>>().taylor_diff_ldbl(
    std::declval<llvm_state &>(), std::declval<const std::vector<std::uint32_t> &>(),
    std::declval<const std::vector<llvm::Value *> &>(), std::declval<llvm::Value *>(), std::declval<llvm::Value *>(),
    std::declval<std::uint32_t>(), std::declval<std::uint32_t>(), std::declval<std::uint32_t>(),
    std::declval<std::uint32_t>()));

template <typename T>
inline constexpr bool func_has_taylor_diff_ldbl_v
    = std::is_same_v<detected_t<func_taylor_diff_ldbl_t, T>, llvm::Value *>;

#if defined(HEYOKA_HAVE_REAL128)

template <typename T>
using func_taylor_diff_f128_t = decltype(std::declval<std::add_lvalue_reference_t<const T>>().taylor_diff_f128(
    std::declval<llvm_state &>(), std::declval<const std::vector<std::uint32_t> &>(),
    std::declval<const std::vector<llvm::Value *> &>(), std::declval<llvm::Value *>(), std::declval<llvm::Value *>(),
    std::declval<std::uint32_t>(), std::declval<std::uint32_t>(), std::declval<std::uint32_t>(),
    std::declval<std::uint32_t>()));

template <typename T>
inline constexpr bool func_has_taylor_diff_f128_v
    = std::is_same_v<detected_t<func_taylor_diff_f128_t, T>, llvm::Value *>;

#endif

template <typename T>
using func_taylor_c_diff_func_dbl_t
    = decltype(std::declval<std::add_lvalue_reference_t<const T>>().taylor_c_diff_func_dbl(
        std::declval<llvm_state &>(), std::declval<std::uint32_t>(), std::declval<std::uint32_t>()));

template <typename T>
inline constexpr bool func_has_taylor_c_diff_func_dbl_v
    = std::is_same_v<detected_t<func_taylor_c_diff_func_dbl_t, T>, llvm::Function *>;

template <typename T>
using func_taylor_c_diff_func_ldbl_t
    = decltype(std::declval<std::add_lvalue_reference_t<const T>>().taylor_c_diff_func_ldbl(
        std::declval<llvm_state &>(), std::declval<std::uint32_t>(), std::declval<std::uint32_t>()));

template <typename T>
inline constexpr bool func_has_taylor_c_diff_func_ldbl_v
    = std::is_same_v<detected_t<func_taylor_c_diff_func_ldbl_t, T>, llvm::Function *>;

#if defined(HEYOKA_HAVE_REAL128)

template <typename T>
using func_taylor_c_diff_func_f128_t
    = decltype(std::declval<std::add_lvalue_reference_t<const T>>().taylor_c_diff_func_f128(
        std::declval<llvm_state &>(), std::declval<std::uint32_t>(), std::declval<std::uint32_t>()));

template <typename T>
inline constexpr bool func_has_taylor_c_diff_func_f128_v
    = std::is_same_v<detected_t<func_taylor_c_diff_func_f128_t, T>, llvm::Function *>;

#endif

HEYOKA_DLL_PUBLIC void func_default_td_impl(func_base &,
                                            std::vector<std::pair<expression, std::vector<std::uint32_t>>> &);

HEYOKA_DLL_PUBLIC void func_default_to_stream_impl(std::ostream &, const func_base &);

template <typename T>
struct HEYOKA_DLL_PUBLIC_INLINE_CLASS func_inner final : func_inner_base {
    T m_value;

    // We just need the def ctor, delete everything else.
    func_inner() = default;
    func_inner(const func_inner &) = delete;
    func_inner(func_inner &&) = delete;
    func_inner &operator=(const func_inner &) = delete;
    func_inner &operator=(func_inner &&) = delete;

    // Constructors from T (copy and move variants).
    explicit func_inner(const T &x) : m_value(x) {}
    explicit func_inner(T &&x) : m_value(std::move(x)) {}

    // The clone function.
    std::unique_ptr<func_inner_base> clone() const final
    {
        return std::make_unique<func_inner>(m_value);
    }

    // Get the type at runtime.
    std::type_index get_type_index() const final
    {
        return typeid(T);
    }
    // Raw getters for the internal instance.
    const void *get_ptr() const final
    {
        return &m_value;
    }
    void *get_ptr() final
    {
        return &m_value;
    }

    const std::string &get_name() const final
    {
        // NOTE: make sure we are invoking the member functions
        // from func_base (these functions could have been overriden
        // in the derived class).
        return static_cast<const func_base *>(&m_value)->get_name();
    }
    void to_stream(std::ostream &os) const final
    {
        if constexpr (func_has_to_stream_v<T>) {
            m_value.to_stream(os);
        } else {
            func_default_to_stream_impl(os, static_cast<const func_base &>(m_value));
        }
    }

    const std::vector<expression> &args() const final
    {
        return static_cast<const func_base *>(&m_value)->args();
    }
    std::pair<std::vector<expression>::iterator, std::vector<expression>::iterator> get_mutable_args_it() final
    {
        return static_cast<func_base *>(&m_value)->get_mutable_args_it();
    }

    // codegen.
    llvm::Value *codegen_dbl(llvm_state &s, const std::vector<llvm::Value *> &v) const final
    {
        if constexpr (func_has_codegen_dbl_v<T>) {
            return m_value.codegen_dbl(s, v);
        } else {
            throw not_implemented_error("double codegen is not implemented for the function '" + get_name() + "'");
        }
    }
    llvm::Value *codegen_ldbl(llvm_state &s, const std::vector<llvm::Value *> &v) const final
    {
        if constexpr (func_has_codegen_ldbl_v<T>) {
            return m_value.codegen_ldbl(s, v);
        } else {
            throw not_implemented_error("long double codegen is not implemented for the function '" + get_name() + "'");
        }
    }
#if defined(HEYOKA_HAVE_REAL128)
    llvm::Value *codegen_f128(llvm_state &s, const std::vector<llvm::Value *> &v) const final
    {
        if constexpr (func_has_codegen_f128_v<T>) {
            return m_value.codegen_f128(s, v);
        } else {
            throw not_implemented_error("float128 codegen is not implemented for the function '" + get_name() + "'");
        }
    }
#endif

    // diff.
    expression diff(const std::string &) const final;

    // eval.
    double eval_dbl(const std::unordered_map<std::string, double> &m, const std::vector<double> &pars) const final
    {
        if constexpr (func_has_eval_dbl_v<T>) {
            return m_value.eval_dbl(m, pars);
        } else {
            throw not_implemented_error("double eval is not implemented for the function '" + get_name() + "'");
        }
    }
    void eval_batch_dbl(std::vector<double> &out, const std::unordered_map<std::string, std::vector<double>> &m,
                        const std::vector<double> &pars) const final
    {
        if constexpr (func_has_eval_batch_dbl_v<T>) {
            m_value.eval_batch_dbl(out, m, pars);
        } else {
            throw not_implemented_error("double batch eval is not implemented for the function '" + get_name() + "'");
        }
    }
    double eval_num_dbl(const std::vector<double> &v) const final
    {
        if constexpr (func_has_eval_num_dbl_v<T>) {
            return m_value.eval_num_dbl(v);
        } else {
            throw not_implemented_error("double numerical eval is not implemented for the function '" + get_name()
                                        + "'");
        }
    }
    double deval_num_dbl(const std::vector<double> &v, std::vector<double>::size_type i) const final
    {
        if constexpr (func_has_deval_num_dbl_v<T>) {
            return m_value.deval_num_dbl(v, i);
        } else {
            throw not_implemented_error("double numerical eval of the derivative is not implemented for the function '"
                                        + get_name() + "'");
        }
    }

    // Taylor.
    std::vector<std::pair<expression, std::vector<std::uint32_t>>>::size_type
        taylor_decompose(std::vector<std::pair<expression, std::vector<std::uint32_t>>> &)
        && final;
    llvm::Value *taylor_diff_dbl(llvm_state &s, const std::vector<std::uint32_t> &deps,
                                 const std::vector<llvm::Value *> &arr, llvm::Value *par_ptr, llvm::Value *time_ptr,
                                 std::uint32_t n_uvars, std::uint32_t order, std::uint32_t idx,
                                 std::uint32_t batch_size) const final
    {
        if constexpr (func_has_taylor_diff_dbl_v<T>) {
            return m_value.taylor_diff_dbl(s, deps, arr, par_ptr, time_ptr, n_uvars, order, idx, batch_size);
        } else {
            throw not_implemented_error("double Taylor diff is not implemented for the function '" + get_name() + "'");
        }
    }
    llvm::Value *taylor_diff_ldbl(llvm_state &s, const std::vector<std::uint32_t> &deps,
                                  const std::vector<llvm::Value *> &arr, llvm::Value *par_ptr, llvm::Value *time_ptr,
                                  std::uint32_t n_uvars, std::uint32_t order, std::uint32_t idx,
                                  std::uint32_t batch_size) const final
    {
        if constexpr (func_has_taylor_diff_ldbl_v<T>) {
            return m_value.taylor_diff_ldbl(s, deps, arr, par_ptr, time_ptr, n_uvars, order, idx, batch_size);
        } else {
            throw not_implemented_error("long double Taylor diff is not implemented for the function '" + get_name()
                                        + "'");
        }
    }
#if defined(HEYOKA_HAVE_REAL128)
    llvm::Value *taylor_diff_f128(llvm_state &s, const std::vector<std::uint32_t> &deps,
                                  const std::vector<llvm::Value *> &arr, llvm::Value *par_ptr, llvm::Value *time_ptr,
                                  std::uint32_t n_uvars, std::uint32_t order, std::uint32_t idx,
                                  std::uint32_t batch_size) const final
    {
        if constexpr (func_has_taylor_diff_f128_v<T>) {
            return m_value.taylor_diff_f128(s, deps, arr, par_ptr, time_ptr, n_uvars, order, idx, batch_size);
        } else {
            throw not_implemented_error("float128 Taylor diff is not implemented for the function '" + get_name()
                                        + "'");
        }
    }
#endif
    llvm::Function *taylor_c_diff_func_dbl(llvm_state &s, std::uint32_t n_uvars, std::uint32_t batch_size) const final
    {
        if constexpr (func_has_taylor_c_diff_func_dbl_v<T>) {
            return m_value.taylor_c_diff_func_dbl(s, n_uvars, batch_size);
        } else {
            throw not_implemented_error("double Taylor diff in compact mode is not implemented for the function '"
                                        + get_name() + "'");
        }
    }
    llvm::Function *taylor_c_diff_func_ldbl(llvm_state &s, std::uint32_t n_uvars, std::uint32_t batch_size) const final
    {
        if constexpr (func_has_taylor_c_diff_func_ldbl_v<T>) {
            return m_value.taylor_c_diff_func_ldbl(s, n_uvars, batch_size);
        } else {
            throw not_implemented_error("long double Taylor diff in compact mode is not implemented for the function '"
                                        + get_name() + "'");
        }
    }
#if defined(HEYOKA_HAVE_REAL128)
    llvm::Function *taylor_c_diff_func_f128(llvm_state &s, std::uint32_t n_uvars, std::uint32_t batch_size) const final
    {
        if constexpr (func_has_taylor_c_diff_func_f128_v<T>) {
            return m_value.taylor_c_diff_func_f128(s, n_uvars, batch_size);
        } else {
            throw not_implemented_error("float128 Taylor diff in compact mode is not implemented for the function '"
                                        + get_name() + "'");
        }
    }
#endif
};

template <typename T>
using is_func = std::conjunction<std::is_same<T, uncvref_t<T>>, std::is_default_constructible<T>,
                                 std::is_copy_constructible<T>, std::is_move_constructible<T>, std::is_destructible<T>,
                                 // https://en.cppreference.com/w/cpp/concepts/derived_from
                                 // NOTE: use add_pointer/add_cv in order to avoid
                                 // issues if invoked with problematic types (e.g., void).
                                 std::is_base_of<func_base, T>,
                                 std::is_convertible<std::add_pointer_t<std::add_cv_t<T>>, const volatile func_base *>>;

} // namespace detail

HEYOKA_DLL_PUBLIC void swap(func &, func &) noexcept;

HEYOKA_DLL_PUBLIC std::ostream &operator<<(std::ostream &, const func &);

class HEYOKA_DLL_PUBLIC func
{
    friend HEYOKA_DLL_PUBLIC void swap(func &, func &) noexcept;
    friend HEYOKA_DLL_PUBLIC std::ostream &operator<<(std::ostream &, const func &);

    // Pointer to the inner base.
    std::unique_ptr<detail::func_inner_base> m_ptr;

    // Just two small helpers to make sure that whenever we require
    // access to the pointer it actually points to something.
    const detail::func_inner_base *ptr() const;
    detail::func_inner_base *ptr();

    template <typename T>
    using generic_ctor_enabler
        = std::enable_if_t<std::conjunction_v<std::negation<std::is_same<func, detail::uncvref_t<T>>>,
                                              detail::is_func<detail::uncvref_t<T>>>,
                           int>;

public:
    template <typename T, generic_ctor_enabler<T &&> = 0>
    explicit func(T &&x) : m_ptr(std::make_unique<detail::func_inner<detail::uncvref_t<T>>>(std::forward<T>(x)))
    {
    }

    func(const func &);
    func(func &&) noexcept;

    func &operator=(const func &);
    func &operator=(func &&) noexcept;

    ~func();

    std::type_index get_type_index() const;
    const void *get_ptr() const;
    void *get_ptr();

    const std::string &get_name() const;

    const std::vector<expression> &args() const;
    std::pair<std::vector<expression>::iterator, std::vector<expression>::iterator> get_mutable_args_it();

    llvm::Value *codegen_dbl(llvm_state &, const std::vector<llvm::Value *> &) const;
    llvm::Value *codegen_ldbl(llvm_state &, const std::vector<llvm::Value *> &) const;
#if defined(HEYOKA_HAVE_REAL128)
    llvm::Value *codegen_f128(llvm_state &, const std::vector<llvm::Value *> &) const;
#endif

    expression diff(const std::string &) const;

    double eval_dbl(const std::unordered_map<std::string, double> &, const std::vector<double> &) const;
    void eval_batch_dbl(std::vector<double> &, const std::unordered_map<std::string, std::vector<double>> &,
                        const std::vector<double> &) const;
    double eval_num_dbl(const std::vector<double> &) const;
    double deval_num_dbl(const std::vector<double> &, std::vector<double>::size_type) const;

    std::vector<std::pair<expression, std::vector<std::uint32_t>>>::size_type
    taylor_decompose(std::vector<std::pair<expression, std::vector<std::uint32_t>>> &) &&;
    llvm::Value *taylor_diff_dbl(llvm_state &, const std::vector<std::uint32_t> &, const std::vector<llvm::Value *> &,
                                 llvm::Value *, llvm::Value *, std::uint32_t, std::uint32_t, std::uint32_t,
                                 std::uint32_t) const;
    llvm::Value *taylor_diff_ldbl(llvm_state &, const std::vector<std::uint32_t> &, const std::vector<llvm::Value *> &,
                                  llvm::Value *, llvm::Value *, std::uint32_t, std::uint32_t, std::uint32_t,
                                  std::uint32_t) const;
#if defined(HEYOKA_HAVE_REAL128)
    llvm::Value *taylor_diff_f128(llvm_state &, const std::vector<std::uint32_t> &, const std::vector<llvm::Value *> &,
                                  llvm::Value *, llvm::Value *, std::uint32_t, std::uint32_t, std::uint32_t,
                                  std::uint32_t) const;
#endif
    llvm::Function *taylor_c_diff_func_dbl(llvm_state &, std::uint32_t, std::uint32_t) const;
    llvm::Function *taylor_c_diff_func_ldbl(llvm_state &, std::uint32_t, std::uint32_t) const;
#if defined(HEYOKA_HAVE_REAL128)
    llvm::Function *taylor_c_diff_func_f128(llvm_state &, std::uint32_t, std::uint32_t) const;
#endif
};

HEYOKA_DLL_PUBLIC std::size_t hash(const func &);

HEYOKA_DLL_PUBLIC bool operator==(const func &, const func &);
HEYOKA_DLL_PUBLIC bool operator!=(const func &, const func &);

HEYOKA_DLL_PUBLIC std::vector<std::string> get_variables(const func &);
HEYOKA_DLL_PUBLIC void rename_variables(func &, const std::unordered_map<std::string, std::string> &);

HEYOKA_DLL_PUBLIC expression subs(const func &, const std::unordered_map<std::string, expression> &);

HEYOKA_DLL_PUBLIC expression diff(const func &, const std::string &);

HEYOKA_DLL_PUBLIC double eval_dbl(const func &, const std::unordered_map<std::string, double> &,
                                  const std::vector<double> &);
HEYOKA_DLL_PUBLIC void eval_batch_dbl(std::vector<double> &, const func &,
                                      const std::unordered_map<std::string, std::vector<double>> &,
                                      const std::vector<double> &);
HEYOKA_DLL_PUBLIC double eval_num_dbl(const func &, const std::vector<double> &);
HEYOKA_DLL_PUBLIC double deval_num_dbl(const func &, const std::vector<double> &, std::vector<double>::size_type);

HEYOKA_DLL_PUBLIC void update_connections(std::vector<std::vector<std::size_t>> &, const func &, std::size_t &);
HEYOKA_DLL_PUBLIC void update_node_values_dbl(std::vector<double> &, const func &,
                                              const std::unordered_map<std::string, double> &,
                                              const std::vector<std::vector<std::size_t>> &, std::size_t &);
HEYOKA_DLL_PUBLIC void update_grad_dbl(std::unordered_map<std::string, double> &, const func &,
                                       const std::unordered_map<std::string, double> &, const std::vector<double> &,
                                       const std::vector<std::vector<std::size_t>> &, std::size_t &, double);

namespace detail
{

// Helper to run the codegen of a function-like object with the arguments
// represented as a vector of LLVM values.
template <typename T, typename F>
inline llvm::Value *codegen_from_values(llvm_state &s, const F &f, const std::vector<llvm::Value *> &args_v)
{
    if constexpr (std::is_same_v<T, double>) {
        return f.codegen_dbl(s, args_v);
    } else if constexpr (std::is_same_v<T, long double>) {
        return f.codegen_ldbl(s, args_v);
#if defined(HEYOKA_HAVE_REAL128)
    } else if constexpr (std::is_same_v<T, mppp::real128>) {
        return f.codegen_f128(s, args_v);
#endif
    } else {
        static_assert(detail::always_false_v<T>, "Unhandled type.");
    }
}

} // namespace detail

HEYOKA_DLL_PUBLIC std::vector<std::pair<expression, std::vector<std::uint32_t>>>::size_type
taylor_decompose_in_place(func &&, std::vector<std::pair<expression, std::vector<std::uint32_t>>> &);

HEYOKA_DLL_PUBLIC llvm::Value *taylor_diff_dbl(llvm_state &, const func &, const std::vector<std::uint32_t> &,
                                               const std::vector<llvm::Value *> &, llvm::Value *, llvm::Value *,
                                               std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

HEYOKA_DLL_PUBLIC llvm::Value *taylor_diff_ldbl(llvm_state &, const func &, const std::vector<std::uint32_t> &,
                                                const std::vector<llvm::Value *> &, llvm::Value *, llvm::Value *,
                                                std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

#if defined(HEYOKA_HAVE_REAL128)

HEYOKA_DLL_PUBLIC llvm::Value *taylor_diff_f128(llvm_state &, const func &, const std::vector<std::uint32_t> &,
                                                const std::vector<llvm::Value *> &, llvm::Value *, llvm::Value *,
                                                std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

#endif

template <typename T>
inline llvm::Value *taylor_diff(llvm_state &s, const func &f, const std::vector<std::uint32_t> &deps,
                                const std::vector<llvm::Value *> &arr, llvm::Value *par_ptr, llvm::Value *time_ptr,
                                std::uint32_t n_uvars, std::uint32_t order, std::uint32_t idx, std::uint32_t batch_size)
{
    if constexpr (std::is_same_v<T, double>) {
        return taylor_diff_dbl(s, f, deps, arr, par_ptr, time_ptr, n_uvars, order, idx, batch_size);
    } else if constexpr (std::is_same_v<T, long double>) {
        return taylor_diff_ldbl(s, f, deps, arr, par_ptr, time_ptr, n_uvars, order, idx, batch_size);
#if defined(HEYOKA_HAVE_REAL128)
    } else if constexpr (std::is_same_v<T, mppp::real128>) {
        return taylor_diff_f128(s, f, deps, arr, par_ptr, time_ptr, n_uvars, order, idx, batch_size);
#endif
    } else {
        static_assert(detail::always_false_v<T>, "Unhandled type.");
    }
}

HEYOKA_DLL_PUBLIC llvm::Function *taylor_c_diff_func_dbl(llvm_state &, const func &, std::uint32_t, std::uint32_t);

HEYOKA_DLL_PUBLIC llvm::Function *taylor_c_diff_func_ldbl(llvm_state &, const func &, std::uint32_t, std::uint32_t);

#if defined(HEYOKA_HAVE_REAL128)

HEYOKA_DLL_PUBLIC llvm::Function *taylor_c_diff_func_f128(llvm_state &, const func &, std::uint32_t, std::uint32_t);

#endif

template <typename T>
inline llvm::Function *taylor_c_diff_func(llvm_state &s, const func &f, std::uint32_t n_uvars, std::uint32_t batch_size)
{
    if constexpr (std::is_same_v<T, double>) {
        return taylor_c_diff_func_dbl(s, f, n_uvars, batch_size);
    } else if constexpr (std::is_same_v<T, long double>) {
        return taylor_c_diff_func_ldbl(s, f, n_uvars, batch_size);
#if defined(HEYOKA_HAVE_REAL128)
    } else if constexpr (std::is_same_v<T, mppp::real128>) {
        return taylor_c_diff_func_f128(s, f, n_uvars, batch_size);
#endif
    } else {
        static_assert(detail::always_false_v<T>, "Unhandled type.");
    }
}

} // namespace heyoka

#endif
