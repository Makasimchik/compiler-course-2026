; RUN: opt -load-pass-plugin %llvmshlibdir/DecomposeRemPass%pluginext \
; RUN: -passes=decompose-rem -S %s | FileCheck %s

; CHECK-LABEL: @test_srem
; CHECK-NOT: srem
; CHECK: %sdiv_tmp = sdiv i32 %a, %b
; CHECK: %mul_tmp = mul i32 %sdiv_tmp, %b
; CHECK: %srem_decomposed = sub i32 %a, %mul_tmp
define i32 @test_srem(i32 %a, i32 %b) {
entry:
  %result = srem i32 %a, %b
  ret i32 %result
}

; CHECK-LABEL: @test_urem
; CHECK-NOT: urem
; CHECK: %udiv_tmp = udiv i32 %a, %b
; CHECK: %mul_tmp = mul i32 %udiv_tmp, %b
; CHECK: %urem_decomposed = sub i32 %a, %mul_tmp
define i32 @test_urem(i32 %a, i32 %b) {
entry:
  %result = urem i32 %a, %b
  ret i32 %result
}

; CHECK-LABEL: @test_frem
; CHECK-NOT: frem
; CHECK: %fdiv_tmp = fdiv float %a, %b
; CHECK: %trunc_tmp = call float @llvm.trunc.f32(float %fdiv_tmp)
; CHECK: %fmul_tmp = fmul float %trunc_tmp, %b
; CHECK: %frem_decomposed = fsub float %a, %fmul_tmp
define float @test_frem(float %a, float %b) {
entry:
  %result = frem float %a, %b
  ret float %result
}

; CHECK-LABEL: @test_frem_double
; CHECK-NOT: frem
; CHECK: %fdiv_tmp = fdiv double %a, %b
; CHECK: %trunc_tmp = call double @llvm.trunc.f64(double %fdiv_tmp)
; CHECK: %fmul_tmp = fmul double %trunc_tmp, %b
; CHECK: %frem_decomposed = fsub double %a, %fmul_tmp
define double @test_frem_double(double %a, double %b) {
entry:
  %result = frem double %a, %b
  ret double %result
}

; CHECK-LABEL: @test_multiple_rem
; CHECK-NOT: srem
; CHECK: %sdiv_tmp = sdiv i32 %a, %b
; CHECK: %mul_tmp = mul i32 %sdiv_tmp, %b
; CHECK: %srem_decomposed = sub i32 %a, %mul_tmp
define i32 @test_multiple_rem(i32 %a, i32 %b, i32 %c) {
entry:
  %r1 = srem i32 %a, %b
  %r2 = srem i32 %r1, %c
  ret i32 %r2
}

; CHECK-LABEL: @test_srem_i64
; CHECK-NOT: srem
; CHECK: %sdiv_tmp = sdiv i64 %a, %b
; CHECK: %mul_tmp = mul i64 %sdiv_tmp, %b
; CHECK: %srem_decomposed = sub i64 %a, %mul_tmp
define i64 @test_srem_i64(i64 %a, i64 %b) {
entry:
  %result = srem i64 %a, %b
  ret i64 %result
}

; CHECK-LABEL: @test_urem_i64
; CHECK-NOT: urem
; CHECK: %udiv_tmp = udiv i64 %a, %b
; CHECK: %mul_tmp = mul i64 %udiv_tmp, %b
; CHECK: %urem_decomposed = sub i64 %a, %mul_tmp
define i64 @test_urem_i64(i64 %a, i64 %b) {
entry:
  %result = urem i64 %a, %b
  ret i64 %result
}

; CHECK-LABEL: @test_no_rem
; CHECK: add i32 %a, %b
define i32 @test_no_rem(i32 %a, i32 %b) {
entry:
  %result = add i32 %a, %b
  ret i32 %result
}