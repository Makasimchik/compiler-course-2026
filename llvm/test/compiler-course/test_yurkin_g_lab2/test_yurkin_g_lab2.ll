; RUN: opt -load-pass-plugin %llvmshlibdir/yurkin_g_lab2_LLVM_IR%pluginext -passes=yurkin_g_lab2 -S %s | FileCheck %s

; Declarations for trunc intrinsics used in FP replacement
declare float @llvm.trunc.f32(float)
declare <2 x float> @llvm.trunc.v2f32(<2 x float>)

; CHECK-LABEL: @test_frem
; CHECK-NOT: frem
; CHECK: %frem.div = fdiv float %a, %b
; CHECK: %frem.trunc = call float @llvm.trunc.f32(float %frem.div)
; CHECK: %frem.mul = fmul float %frem.trunc, %b
; CHECK: %frem.res = fsub float %a, %frem.mul
; CHECK: ret float %frem.res
define float @test_frem(float %a, float %b) {
entry:
  %res = frem float %a, %b
  ret float %res
}

; CHECK-LABEL: @test_srem
; CHECK-NOT: srem
; CHECK: %srem.div = sdiv i32 %a, %b
; CHECK: %srem.mul = mul i32 %srem.div, %b
; CHECK: %srem.res = sub i32 %a, %srem.mul
; CHECK: ret i32 %srem.res
define i32 @test_srem(i32 %a, i32 %b) {
entry:
  %res = srem i32 %a, %b
  ret i32 %res
}

; CHECK-LABEL: @test_urem
; CHECK-NOT: urem
; CHECK: %urem.div = udiv i64 %a, %b
; CHECK: %urem.mul = mul i64 %urem.div, %b
; CHECK: %urem.res = sub i64 %a, %urem.mul
; CHECK: ret i64 %urem.res
define i64 @test_urem(i64 %a, i64 %b) {
entry:
  %res = urem i64 %a, %b
  ret i64 %res
}

; CHECK-LABEL: @test_vector
; CHECK-NOT: frem
; CHECK: %frem.div = fdiv <2 x float> %a, %b
; CHECK: %frem.trunc = call <2 x float> @llvm.trunc.v2f32(<2 x float> %frem.div)
; CHECK: %frem.mul = fmul <2 x float> %frem.trunc, %b
; CHECK: %frem.res = fsub <2 x float> %a, %frem.mul
; CHECK: ret <2 x float> %frem.res
define <2 x float> @test_vector(<2 x float> %a, <2 x float> %b) {
entry:
  %res = frem <2 x float> %a, %b
  ret <2 x float> %res
}
