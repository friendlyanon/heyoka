// Copyright 2020 Francesco Biscani (bluescarni@gmail.com), Dario Izzo (dario.izzo@gmail.com)
//
// This file is part of the heyoka library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <llvm/Config/llvm-config.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Transforms/Vectorize.h>

#include <heyoka/detail/llvm_helpers.hpp>
#include <heyoka/detail/string_conv.hpp>
#include <heyoka/expression.hpp>
#include <heyoka/llvm_state.hpp>
#include <heyoka/number.hpp>
#include <heyoka/taylor.hpp>
#include <heyoka/variable.hpp>

namespace heyoka
{

namespace detail
{

namespace
{

std::once_flag nt_inited;

} // namespace

} // namespace detail

// Implementation of the jit class.
class llvm_state::jit
{
    llvm::orc::ExecutionSession m_es;
    llvm::orc::RTDyldObjectLinkingLayer m_object_layer;
    std::unique_ptr<llvm::orc::IRCompileLayer> m_compile_layer;
    std::unique_ptr<llvm::DataLayout> m_dl;
    // NOTE: it seems like in LLVM 11 this class was moved
    // from llvm/ExecutionEngine/Orc/Core.h to
    // llvm/ExecutionEngine/Orc/Mangling.h.
    std::unique_ptr<llvm::orc::MangleAndInterner> m_mangle;
    llvm::orc::ThreadSafeContext m_ctx;
#if LLVM_VERSION_MAJOR == 10
    llvm::orc::JITDylib &m_main_jd;
#endif

public:
    jit()
        : m_object_layer(m_es, []() { return std::make_unique<llvm::SectionMemoryManager>(); }),
          m_ctx(std::make_unique<llvm::LLVMContext>())
#if LLVM_VERSION_MAJOR == 10
          ,
          m_main_jd(m_es.createJITDylib("<main>"))
#endif
    {
        // NOTE: the native target initialization needs to be done only once
        std::call_once(detail::nt_inited, []() {
            llvm::InitializeNativeTarget();
            llvm::InitializeNativeTargetAsmPrinter();
            llvm::InitializeNativeTargetAsmParser();
        });

        auto jtmb = llvm::orc::JITTargetMachineBuilder::detectHost();
        if (!jtmb) {
            throw std::invalid_argument("Error creating a JITTargetMachineBuilder for the host system");
        }

        auto dlout = jtmb->getDefaultDataLayoutForTarget();
        if (!dlout) {
            throw std::invalid_argument("Error fetching the default data layout for the host system");
        }

        m_compile_layer = std::make_unique<llvm::orc::IRCompileLayer>(
            m_es, m_object_layer,
#if LLVM_VERSION_MAJOR == 10
            std::make_unique<llvm::orc::ConcurrentIRCompiler>(std::move(*jtmb))
#else
            llvm::orc::ConcurrentIRCompiler(std::move(*jtmb))
#endif
        );

        m_dl = std::make_unique<llvm::DataLayout>(std::move(*dlout));

        m_mangle = std::make_unique<llvm::orc::MangleAndInterner>(m_es, *m_dl);

        auto dlsg = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(m_dl->getGlobalPrefix());
        if (!dlsg) {
            throw std::invalid_argument("Could not create the dynamic library search generator");
        }

#if LLVM_VERSION_MAJOR == 10
        m_main_jd.addGenerator(std::move(*dlsg));
#else
        m_es.getMainJITDylib().setGenerator(std::move(*dlsg));
#endif
    }

    jit(const jit &) = delete;
    jit(jit &&) = delete;
    jit &operator=(const jit &) = delete;
    jit &operator=(jit &&) = delete;

    ~jit() = default;

    // Accessors.
    llvm::LLVMContext &get_context()
    {
        return *m_ctx.getContext();
    }
    const llvm::LLVMContext &get_context() const
    {
        return *m_ctx.getContext();
    }
    const llvm::DataLayout &get_data_layout() const
    {
        return *m_dl;
    }

    void add_module(std::unique_ptr<llvm::Module> &&m)
    {
        auto handle = m_compile_layer->add(
#if LLVM_VERSION_MAJOR == 10
            m_main_jd,
#else
            m_es.getMainJITDylib(),
#endif
            llvm::orc::ThreadSafeModule(std::move(m), m_ctx));

        if (handle) {
            throw std::invalid_argument("The function for adding a module to the jit failed");
        }
    }

    // Symbol lookup.
    llvm::Expected<llvm::JITEvaluatedSymbol> lookup(const std::string &name)
    {
        return m_es.lookup(
#if LLVM_VERSION_MAJOR == 10
            {&m_main_jd},
#else
            {&m_es.getMainJITDylib()},
#endif
            (*m_mangle)(name));
    }
};

llvm_state::llvm_state(const std::string &name, unsigned opt_level)
    : m_jitter(std::make_unique<jit>()), m_opt_level(opt_level)
{
    static_assert(std::is_same_v<llvm::IRBuilder<>, decltype(m_builder)::element_type>,
                  "Inconsistent llvm::IRBuilder<> type.");

    // Create the module.
    m_module = std::make_unique<llvm::Module>(name, context());
    m_module->setDataLayout(m_jitter->get_data_layout());

    // Create a new builder for the module.
    m_builder = std::make_unique<llvm::IRBuilder<>>(context());

    // Set a couple of flags for faster math at the
    // price of potential change of semantics.
    llvm::FastMathFlags fmf;
    fmf.setFast();
    m_builder->setFastMathFlags(fmf);

    // Create the optimization passes.
    if (m_opt_level > 0u) {
        // Create the function pass manager.
        m_fpm = std::make_unique<llvm::legacy::FunctionPassManager>(m_module.get());
        m_fpm->add(llvm::createPromoteMemoryToRegisterPass());
        m_fpm->add(llvm::createInstructionCombiningPass());
        m_fpm->add(llvm::createReassociatePass());
        m_fpm->add(llvm::createGVNPass());
        m_fpm->add(llvm::createCFGSimplificationPass());
        m_fpm->add(llvm::createLoopVectorizePass());
        m_fpm->add(llvm::createSLPVectorizerPass());
        m_fpm->add(llvm::createLoadStoreVectorizerPass());
        m_fpm->add(llvm::createLoopUnrollPass());
        m_fpm->doInitialization();

        // The module-level optimizer. See:
        // https://stackoverflow.com/questions/48300510/llvm-api-optimisation-run
        m_pm = std::make_unique<llvm::legacy::PassManager>();
        llvm::PassManagerBuilder pm_builder;
        // See here for the defaults:
        // https://llvm.org/doxygen/PassManagerBuilder_8cpp_source.html
        pm_builder.OptLevel = m_opt_level;
        pm_builder.VerifyInput = true;
        pm_builder.VerifyOutput = true;
        pm_builder.Inliner = llvm::createFunctionInliningPass();
        if (m_opt_level >= 3u) {
            pm_builder.SLPVectorize = true;
            pm_builder.MergeFunctions = true;
        }
        pm_builder.populateModulePassManager(*m_pm);
        pm_builder.populateFunctionPassManager(*m_fpm);
    }
}

llvm_state::~llvm_state() = default;

llvm::Module &llvm_state::module()
{
    check_uncompiled(__func__);
    return *m_module;
}

llvm::IRBuilder<> &llvm_state::builder()
{
    return *m_builder;
}

llvm::LLVMContext &llvm_state::context()
{
    return m_jitter->get_context();
}

bool &llvm_state::verify()
{
    return m_verify;
}

std::unordered_map<std::string, llvm::Value *> &llvm_state::named_values()
{
    return m_named_values;
}

const llvm::Module &llvm_state::module() const
{
    check_uncompiled(__func__);
    return *m_module;
}

const llvm::IRBuilder<> &llvm_state::builder() const
{
    return *m_builder;
}

const llvm::LLVMContext &llvm_state::context() const
{
    return m_jitter->get_context();
}

const bool &llvm_state::verify() const
{
    return m_verify;
}

const std::unordered_map<std::string, llvm::Value *> &llvm_state::named_values() const
{
    return m_named_values;
}

void llvm_state::check_uncompiled(const char *f) const
{
    if (!m_module) {
        throw std::invalid_argument(std::string{"The function '"} + f
                                    + "' can be invoked only if the module has not been compiled yet");
    }
}

void llvm_state::check_compiled(const char *f) const
{
    if (m_module) {
        throw std::invalid_argument(std::string{"The function '"} + f
                                    + "' can be invoked only after the module has been compiled");
    }
}

void llvm_state::check_add_name(const std::string &name) const
{
    assert(m_module);
    if (m_module->getNamedValue(name) != nullptr) {
        throw std::invalid_argument("The name '" + name + "' already exists in the module");
    }
}

void llvm_state::verify_function_impl(llvm::Function *f)
{
    assert(f != nullptr);

    std::string err_report;
    llvm::raw_string_ostream ostr(err_report);
    if (llvm::verifyFunction(*f, &ostr) && m_verify) {
        // Remove function before throwing.
        f->eraseFromParent();
        throw std::invalid_argument("Function verification failed. The full error message:\n" + ostr.str());
    }
}

void llvm_state::verify_function(const std::string &name)
{
    check_uncompiled(__func__);

    // Lookup the function in the module.
    auto f = m_module->getFunction(name);
    if (f == nullptr) {
        throw std::invalid_argument("The function '" + name + "' does not exist in the module");
    }

    // Run the actual check.
    verify_function_impl(f);
}

void llvm_state::compile()
{
    check_uncompiled(__func__);

    m_jitter->add_module(std::move(m_module));
}

template <typename T>
void llvm_state::add_varargs_expression(const std::string &name, const expression &e,
                                        const std::vector<std::string> &vars)
{
    // Prepare the function prototype. First the function arguments.
    std::vector<llvm::Type *> fargs(vars.size(), detail::to_llvm_type<T>(context()));
    // Then the return type.
    auto *ft = llvm::FunctionType::get(detail::to_llvm_type<T>(context()), fargs, false);
    assert(ft != nullptr);

    // Now create the function.
    auto *f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name, m_module.get());
    assert(f != nullptr);
    // Set names for all arguments.
    decltype(vars.size()) idx = 0;
    for (auto &arg : f->args()) {
        arg.setName(vars[idx++]);
    }

    // Create a new basic block to start insertion into.
    auto *bb = llvm::BasicBlock::Create(context(), "entry", f);
    assert(bb != nullptr);
    m_builder->SetInsertPoint(bb);

    // Record the function arguments in the m_named_values map.
    m_named_values.clear();
    for (auto &arg : f->args()) {
        m_named_values[arg.getName()] = &arg;
    }

    // Run the codegen on the expression.
    auto *ret_val = detail::invoke_codegen<T>(*this, e);
    assert(ret_val != nullptr);

    // Finish off the function.
    m_builder->CreateRet(ret_val);

    // NOTE: it seems like the module-level
    // optimizer is able to figure out on its
    // own at least some useful attributes for
    // functions. Additional attributes
    // (e.g., speculatable, willreturn)
    // will also depend on the attributes
    // of function calls contained in the expression,
    // so it may be tricky to "prove" that they
    // can be added safely.

    // Verify it.
    verify_function_impl(f);

    // Add the function to m_sig_map.
    std::vector<std::type_index> sig_args(vars.size(), std::type_index(typeid(T)));
    auto sig = std::pair{std::type_index(typeid(T)), std::move(sig_args)};
    [[maybe_unused]] const auto eret = m_sig_map.emplace(name, std::move(sig));
    assert(eret.second);
}

void llvm_state::add_dbl(const std::string &name, const expression &e)
{
    check_uncompiled(__func__);
    check_add_name(name);

    // Fetch the sorted list of variables in the expression.
    const auto vars = get_variables(e);

    add_varargs_expression<double>(name, e, vars);

    // Run the optimization pass.
    if (m_opt_level > 0u) {
        m_pm->run(*m_module);
    }
}

void llvm_state::add_ldbl(const std::string &name, const expression &e)
{
    check_uncompiled(__func__);
    check_add_name(name);

    // Fetch the sorted list of variables in the expression.
    const auto vars = get_variables(e);

    add_varargs_expression<long double>(name, e, vars);

    // Run the optimization pass.
    if (m_opt_level > 0u) {
        m_pm->run(*m_module);
    }
}

// NOTE: this function will lookup symbol names,
// so it does not necessarily return a function
// pointer (could be, e.g., a global variable).
std::uintptr_t llvm_state::jit_lookup(const std::string &name)
{
    check_compiled(__func__);

    auto sym = m_jitter->lookup(name);
    if (!sym) {
        throw std::invalid_argument("Could not find the symbol '" + name + "' in the compiled module");
    }

    return static_cast<std::uintptr_t>((*sym).getAddress());
}

std::string llvm_state::dump() const
{
    check_uncompiled(__func__);

    std::string out;
    llvm::raw_string_ostream ostr(out);
    m_module->print(ostr, nullptr);
    return ostr.str();
}

std::string llvm_state::dump_function(const std::string &name) const
{
    check_uncompiled(__func__);

    if (auto f = m_module->getFunction(name)) {
        std::string out;
        llvm::raw_string_ostream ostr(out);
        f->print(ostr);
        return ostr.str();
    } else {
        throw std::invalid_argument("Could not locate the function called '" + name + "'");
    }
}

llvm_state::ts_dbl_t llvm_state::fetch_taylor_stepper_dbl(const std::string &name)
{
    check_compiled(__func__);

    return fetch_taylor_stepper_impl<double>(name);
}

llvm_state::ts_ldbl_t llvm_state::fetch_taylor_stepper_ldbl(const std::string &name)
{
    check_compiled(__func__);

    return fetch_taylor_stepper_impl<long double>(name);
}

// Create the function to implement the n-th order normalised derivative of a
// state variable in a Taylor system. n_uvars is the total number of
// u variables in the decomposition, var is the u variable which is equal to
// the first derivative of the state variable.
template <typename T>
auto llvm_state::taylor_add_sv_diff(const std::string &fname, std::uint32_t n_uvars, const variable &var)
{
    check_add_name(fname);

    // Extract the index of the u variable.
    const auto u_idx = detail::uname_to_index(var.name());

    // Prepare the main function prototype. The arguments are:
    // - const float pointer to the derivatives array,
    // - 32-bit integer (order of the derivative).
    std::vector<llvm::Type *> fargs{llvm::PointerType::getUnqual(detail::to_llvm_type<T>(context())),
                                    m_builder->getInt32Ty()};

    // The function will return the n-th derivative.
    auto *ft = llvm::FunctionType::get(detail::to_llvm_type<T>(context()), fargs, false);
    assert(ft != nullptr);

    // Now create the function. Don't need to call it from outside,
    // thus internal linkage.
    auto *f = llvm::Function::Create(ft, llvm::Function::InternalLinkage, fname, m_module.get());
    assert(f != nullptr);

    // Setup the function arugments.
    auto arg_it = f->args().begin();
    arg_it->setName("diff_ptr");
    arg_it->addAttr(llvm::Attribute::ReadOnly);
    arg_it->addAttr(llvm::Attribute::NoCapture);
    auto diff_ptr = arg_it;

    (++arg_it)->setName("order");
    auto order = arg_it;

    // Create a new basic block to start insertion into.
    auto *bb = llvm::BasicBlock::Create(context(), "entry", f);
    assert(bb != nullptr);
    m_builder->SetInsertPoint(bb);

    // Fetch from diff_ptr the pointer to the u variable
    // at u_idx. The index is (order - 1) * n_uvars + u_idx.
    auto in_ptr = m_builder->CreateInBoundsGEP(
        diff_ptr,
        {m_builder->CreateAdd(
            m_builder->CreateMul(m_builder->getInt32(n_uvars), m_builder->CreateSub(order, m_builder->getInt32(1))),
            m_builder->getInt32(u_idx))},
        "diff_ptr");

    // Load the value from in_ptr.
    auto diff_load = m_builder->CreateLoad(in_ptr, "diff_load");

    // We have to divide the derivative by order
    // to get the normalised derivative of the state variable.
    // NOTE: precompute in the main function the 1/n factors?
    auto ret = m_builder->CreateFDiv(diff_load, m_builder->CreateUIToFP(order, detail::to_llvm_type<T>(context())));

    m_builder->CreateRet(ret);

    // Verify it.
    verify_function_impl(f);

    // NOTE: no need to add the function
    // signature to m_sig_map, as this
    // is just an internal function.

    return f;
}

// Same as above, but for the special case in which the derivative
// of a state variable is not equal to a u variable, but to
// a constant (e.g., x' = 1).
template <typename T>
auto llvm_state::taylor_add_sv_diff(const std::string &fname, std::uint32_t, const number &num)
{
    check_add_name(fname);

    // Prepare the main function prototype. The arguments are:
    // - const float pointer to the derivatives array,
    // - 32-bit integer (order of the derivative).
    std::vector<llvm::Type *> fargs{llvm::PointerType::getUnqual(detail::to_llvm_type<T>(context())),
                                    m_builder->getInt32Ty()};

    // The function will return the n-th derivative.
    auto *ft = llvm::FunctionType::get(detail::to_llvm_type<T>(context()), fargs, false);
    assert(ft != nullptr);

    // Now create the function. Don't need to call it from outside,
    // thus internal linkage.
    auto *f = llvm::Function::Create(ft, llvm::Function::InternalLinkage, fname, m_module.get());
    assert(f != nullptr);

    // Setup the function arugments.
    // NOTE: the fist argument will be unused.
    auto arg_it = f->args().begin();
    arg_it->setName("diff_ptr");
    arg_it->addAttr(llvm::Attribute::ReadOnly);
    arg_it->addAttr(llvm::Attribute::NoCapture);

    (++arg_it)->setName("order");
    auto order = arg_it;

    // Create a new basic block to start insertion into.
    auto *bb = llvm::BasicBlock::Create(context(), "entry", f);
    assert(bb != nullptr);
    m_builder->SetInsertPoint(bb);

    // If the first-order derivative is being requested,
    // we return the constant in num. Otherwise, we return
    // zero.
    // Create the comparison instruction.
    auto cmp_inst = m_builder->CreateICmpEQ(order, m_builder->getInt32(1), "order_cmp");
    assert(cmp_inst != nullptr);

    // Create blocks for the then and else cases.  Insert the 'then' block at the
    // end of the function.
    auto then_bb = llvm::BasicBlock::Create(context(), "then", f);
    assert(then_bb != nullptr);
    auto else_bb = llvm::BasicBlock::Create(context(), "else");
    assert(else_bb != nullptr);
    auto merge_bb = llvm::BasicBlock::Create(context(), "ifcont");
    assert(merge_bb != nullptr);

    auto branch_inst = m_builder->CreateCondBr(cmp_inst, then_bb, else_bb);
    assert(branch_inst != nullptr);

    // Emit then value.
    m_builder->SetInsertPoint(then_bb);
    auto then_value = detail::invoke_codegen<T>(*this, num);

    auto lab = m_builder->CreateBr(merge_bb);
    assert(lab != nullptr);

    // Codegen of 'then' can change the current block, update then_bb for the PHI.
    then_bb = m_builder->GetInsertBlock();

    // Emit else block.
    f->getBasicBlockList().push_back(else_bb);
    m_builder->SetInsertPoint(else_bb);
    auto else_value = detail::invoke_codegen<T>(*this, number(0.));

    lab = m_builder->CreateBr(merge_bb);
    assert(lab != nullptr);

    // Codegen of 'else' can change the current block, update else_bb for the PHI.
    else_bb = m_builder->GetInsertBlock();

    // Emit merge block.
    f->getBasicBlockList().push_back(merge_bb);
    m_builder->SetInsertPoint(merge_bb);
    auto PN = m_builder->CreatePHI(detail::to_llvm_type<T>(context()), 2, "iftmp");
    assert(PN != nullptr);

    PN->addIncoming(then_value, then_bb);
    PN->addIncoming(else_value, else_bb);

    m_builder->CreateRet(PN);

    // Verify it.
    verify_function_impl(f);

    // NOTE: no need to add the function
    // signature to m_sig_map, as this
    // is just an internal function.

    return f;
}

// Helper to create the functions for the computation
// of the derivatives of the u variables.
template <typename T>
auto llvm_state::taylor_add_uvars_diff(const std::string &name, const std::vector<expression> &dc,
                                       std::uint32_t n_uvars, std::uint32_t n_eq)
{
    // We begin with the state variables.
    // We will also identify the state variables whose derivatives
    // are constants and record them.
    std::unordered_map<std::uint32_t, number> cd_uvars;
    // We will store pointers to the created functions
    // for later use.
    std::vector<llvm::Function *> u_diff_funcs;

    // NOTE: the derivatives of the state variables
    // are at the end of the decomposition vector.
    for (std::uint32_t i = n_uvars; i < dc.size(); ++i) {
        const auto &ex = dc[i];
        const auto u_idx = static_cast<std::uint32_t>(i - n_uvars);
        const auto fname = name + ".diff." + detail::li_to_string(u_idx);

        std::visit(
            [this, &u_diff_funcs, &cd_uvars, &fname, n_uvars, u_idx](const auto &v) {
                using type = detail::uncvref_t<decltype(v)>;

                // NOTE: the only possibilities which make sense
                // here for type are number or variable, and thus
                // I think we don't need this to be a general-purpose
                // customisation point.
                if constexpr (std::is_same_v<type, number>) {
                    // ex is a number. Add its index to the list
                    // of constant-derivative state variables.
                    cd_uvars.emplace(u_idx, v);
                    u_diff_funcs.emplace_back(this->taylor_add_sv_diff<T>(fname, n_uvars, v));
                } else if constexpr (std::is_same_v<type, variable>) {
                    // ex is a variable.
                    u_diff_funcs.emplace_back(this->taylor_add_sv_diff<T>(fname, n_uvars, v));
                } else {
                    assert(false);
                }
            },
            ex.value());
    }

    // Now the derivatives of the other u variables.
    for (auto i = n_eq; i < n_uvars; ++i) {
        detail::invoke_taylor_diff<T>(*this, dc[i], i, name + ".diff." + detail::li_to_string(i), n_uvars, cd_uvars);
    }
}

template <typename T>
void llvm_state::add_taylor_stepper_impl(const std::string &name, std::vector<expression> sys, std::uint32_t max_order)
{
    check_uncompiled(__func__);
    check_add_name(name);

    if (max_order == 0u) {
        throw std::invalid_argument("The maximum order cannot be zero");
    }

    // Record the number of equations/variables.
    const auto n_eq = sys.size();

    // Decompose the system of equations.
    const auto dc = taylor_decompose(std::move(sys));

    // Compute the number of u variables.
    assert(dc.size() > n_eq);
    const auto n_uvars = dc.size() - n_eq;

    // Overflow checking. We want to make sure we can do all computations
    // using uint32_t.
    if (n_eq > std::numeric_limits<std::uint32_t>::max() || n_uvars > std::numeric_limits<std::uint32_t>::max()
        || n_uvars > std::numeric_limits<std::uint32_t>::max() / max_order) {
        throw std::overflow_error(
            "An overflow condition was detected in the number of variables while adding a Taylor stepper");
    }

    // Create the functions for the computation of the derivatives
    // of the u variables.
    taylor_add_uvars_diff<T>(name, dc, static_cast<std::uint32_t>(n_uvars), static_cast<std::uint32_t>(n_eq));

    // TODO main function: verify, add to signature map.

    // Run the optimization pass.
    if (m_opt_level > 0u) {
        m_pm->run(*m_module);
    }
}

void llvm_state::add_taylor_stepper_dbl(const std::string &name, std::vector<expression> sys, std::uint32_t max_order)
{
    add_taylor_stepper_impl<double>(name, std::move(sys), max_order);
}

void llvm_state::add_taylor_stepper_ldbl(const std::string &name, std::vector<expression> sys, std::uint32_t max_order)
{
    add_taylor_stepper_impl<long double>(name, std::move(sys), max_order);
}

} // namespace heyoka
