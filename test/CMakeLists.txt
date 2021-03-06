# Find the xtensor bits.
find_package(xtensor REQUIRED CONFIG)
find_package(xtensor-blas REQUIRED CONFIG)

add_library(heyoka_test STATIC catch_main.cpp)
target_compile_options(heyoka_test PRIVATE
  "$<$<CONFIG:Debug>:${HEYOKA_CXX_FLAGS_DEBUG}>"
  "$<$<CONFIG:Release>:${HEYOKA_CXX_FLAGS_RELEASE}>"
  "$<$<CONFIG:RelWithDebInfo>:${HEYOKA_CXX_FLAGS_RELEASE}>"
  "$<$<CONFIG:MinSizeRel>:${HEYOKA_CXX_FLAGS_RELEASE}>"
)

# Setup the C++ standard.
target_compile_features(heyoka_test PRIVATE cxx_std_17)
set_property(TARGET heyoka_test PROPERTY CXX_EXTENSIONS NO)

function(ADD_HEYOKA_TESTCASE arg1)
  add_executable(${arg1} ${arg1}.cpp)
  target_link_libraries(${arg1} PRIVATE heyoka_test heyoka xtensor xtensor-blas)
  target_compile_definitions(${arg1} PRIVATE XTENSOR_USE_FLENS_BLAS)
  target_compile_options(${arg1} PRIVATE
    "$<$<CONFIG:Debug>:${HEYOKA_CXX_FLAGS_DEBUG}>"
    "$<$<CONFIG:Release>:${HEYOKA_CXX_FLAGS_RELEASE}>"
    "$<$<CONFIG:RelWithDebInfo>:${HEYOKA_CXX_FLAGS_RELEASE}>"
    "$<$<CONFIG:MinSizeRel>:${HEYOKA_CXX_FLAGS_RELEASE}>"
  )
  # Setup the C++ standard.
  target_compile_features(${arg1} PRIVATE cxx_std_17)
  set_property(TARGET ${arg1} PROPERTY CXX_EXTENSIONS NO)
  add_test(${arg1} ${arg1})
endfunction()

ADD_HEYOKA_TESTCASE(llvm_state)
ADD_HEYOKA_TESTCASE(expression)
ADD_HEYOKA_TESTCASE(math_functions)
ADD_HEYOKA_TESTCASE(func)
ADD_HEYOKA_TESTCASE(gp)
ADD_HEYOKA_TESTCASE(taylor_adaptive)
ADD_HEYOKA_TESTCASE(taylor_div)
ADD_HEYOKA_TESTCASE(taylor_erf)
ADD_HEYOKA_TESTCASE(taylor_exp)
ADD_HEYOKA_TESTCASE(taylor_log)
ADD_HEYOKA_TESTCASE(taylor_mul)
ADD_HEYOKA_TESTCASE(taylor_addsub)
ADD_HEYOKA_TESTCASE(taylor_pow)
ADD_HEYOKA_TESTCASE(taylor_sqrt)
ADD_HEYOKA_TESTCASE(taylor_square)
ADD_HEYOKA_TESTCASE(taylor_sincos)
ADD_HEYOKA_TESTCASE(taylor_const_sys)
ADD_HEYOKA_TESTCASE(taylor_no_decomp_sys)
ADD_HEYOKA_TESTCASE(taylor_tan)
ADD_HEYOKA_TESTCASE(taylor_time)
ADD_HEYOKA_TESTCASE(taylor_asin)
ADD_HEYOKA_TESTCASE(taylor_acos)
ADD_HEYOKA_TESTCASE(taylor_atan)
ADD_HEYOKA_TESTCASE(taylor_sigmoid)
ADD_HEYOKA_TESTCASE(taylor_sinhcosh)
ADD_HEYOKA_TESTCASE(taylor_tanh)
ADD_HEYOKA_TESTCASE(taylor_asinh)
ADD_HEYOKA_TESTCASE(taylor_acosh)
ADD_HEYOKA_TESTCASE(taylor_atanh)
ADD_HEYOKA_TESTCASE(two_body)
ADD_HEYOKA_TESTCASE(two_body_batch)
ADD_HEYOKA_TESTCASE(e3bp)
ADD_HEYOKA_TESTCASE(fast_math_compare)
ADD_HEYOKA_TESTCASE(back_and_forth)
ADD_HEYOKA_TESTCASE(nbody)
ADD_HEYOKA_TESTCASE(outer_ss)
ADD_HEYOKA_TESTCASE(custom_step)
ADD_HEYOKA_TESTCASE(one_body)
ADD_HEYOKA_TESTCASE(number)
ADD_HEYOKA_TESTCASE(pow)
ADD_HEYOKA_TESTCASE(taylor_tc)
