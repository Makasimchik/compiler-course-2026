; RUN: opt -load-pass-plugin %llvmshlibdir/titaev_m_lab2_LLVM_IR%pluginext \
; RUN: -passes=titaev-m-expand-rem -S %s | FileCheck %s

; CHECK-LABEL: @test_frem
; CHECK-NOT: frem
; CHECK: %titaev.f.div = fdiv float %a, %b
; CHECK: %titaev.f.trunc = call float @llvm.trunc.f32(float %titaev.f.div)
; CHECK: %titaev.f.mul = fmul float %titaev.f.trunc, %b
; CHECK: %titaev.f.res = fsub float %a, %titaev.f.mul
define float @test_frem(float %a, float %b) {
  %res = frem float %a, %b
  ret float %res
}

; CHECK-LABEL: @test_srem
; CHECK-NOT: srem
; CHECK: %titaev.s.div = sdiv i32 %a, %b
; CHECK: %titaev.s.mul = mul i32 %titaev.s.div, %b
; CHECK: %titaev.s.res = sub i32 %a, %titaev.s.mul
define i32 @test_srem(i32 %a, i32 %b) {
  %res = srem i32 %a, %b
  ret i32 %res
}

; CHECK-LABEL: @test_urem
; CHECK-NOT: urem
; CHECK: %titaev.u.div = udiv i64 %a, %b
; CHECK: %titaev.u.mul = mul i64 %titaev.u.div, %b
; CHECK: %titaev.u.res = sub i64 %a, %titaev.u.mul
define i64 @test_urem(i64 %a, i64 %b) {
  %res = urem i64 %a, %b
  ret i64 %res
}

; CHECK-LABEL: @test_vector
; CHECK-NOT: frem
; CHECK: %titaev.f.div = fdiv <2 x float> %a, %b
; CHECK: %titaev.f.trunc = call <2 x float> @llvm.trunc.v2f32(<2 x float> %titaev.f.div)
; CHECK: %titaev.f.mul = fmul <2 x float> %titaev.f.trunc, %b
; CHECK: %titaev.f.res = fsub <2 x float> %a, %titaev.f.mul
define <2 x float> @test_vector(<2 x float> %a, <2 x float> %b) {
  %res = frem <2 x float> %a, %b
  ret <2 x float> %res
}