; RUN: opt -load-pass-plugin=%llvmshlibdir/kutergin_a_lab2_LLVM_IR%shlibext -passes=powi-to-mul -S %s | FileCheck %s

define float @test_pow0(float %x) {
; CHECK-LABEL: @test_pow0
; CHECK-NEXT: ret float 1.000000e+00
  %1 = call float @llvm.powi.f32.i32(float %x, i32 0)
  ret float %1
}

define float @test_pow1(float %x) {
; CHECK-LABEL: @test_pow1
; CHECK-NEXT: ret float %x
  %1 = call float @llvm.powi.f32.i32(float %x, i32 1)
  ret float %1
}

define float @test_pow2(float %x) {
; CHECK-LABEL: @test_pow2
; CHECK-NEXT: %powi.sq = fmul float %x, %x
; CHECK-NEXT: ret float %powi.sq
  %1 = call float @llvm.powi.f32.i32(float %x, i32 2)
  ret float %1
}

define double @test_pow3(double %x) {
; CHECK-LABEL: @test_pow3
; CHECK-NEXT: %powi.sq = fmul double %x, %x
; CHECK-NEXT: %powi.cub = fmul double %powi.sq, %x
; CHECK-NEXT: ret double %powi.cub
  %1 = call double @llvm.powi.f64.i32(double %x, i32 3)
  ret double %1
}

define float @test_pow4(float %x) {
; CHECK-LABEL: @test_pow4
; CHECK-NEXT: %powi.sq = fmul float %x, %x
; CHECK-NEXT: %powi.quad = fmul float %powi.sq, %powi.sq
; CHECK-NEXT: ret float %powi.quad
  %1 = call float @llvm.powi.f32.i32(float %x, i32 4)
  ret float %1
}

define float @test_pow5(float %x) {
; CHECK-LABEL: @test_pow5
; CHECK-NEXT: %1 = call float @llvm.powi.f32.i32(float %x, i32 5)
; CHECK-NEXT: ret float %1
  %1 = call float @llvm.powi.f32.i32(float %x, i32 5)
  ret float %1
}

declare float @llvm.powi.f32.i32(float, i32)
declare double @llvm.powi.f64.i32(double, i32)