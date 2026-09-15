; RUN: opt -load-pass-plugin %llvmshlibdir/pikhotskiy_r_lab2_LLVM_IR%pluginext \
; RUN:     -passes=pikhotskiy-powi-to-mul -S %s | FileCheck %s

declare float @llvm.powi.f32.i16(float, i16)
declare float @llvm.powi.f32.i32(float, i32)
declare double @llvm.powi.f64.i64(double, i64)
declare <2 x float> @llvm.powi.v2f32.i32(<2 x float>, i32)

define float @pow0(float %x) {
; CHECK-LABEL: @pow0
; CHECK-NEXT:    ret float 1.000000e+00
  %r = call float @llvm.powi.f32.i32(float %x, i32 0)
  ret float %r
}

define float @pow1(float %x) {
; CHECK-LABEL: @pow1
; CHECK-NEXT:    ret float %x
  %r = call float @llvm.powi.f32.i32(float %x, i32 1)
  ret float %r
}

define float @pow2_i16(float %x) {
; CHECK-LABEL: @pow2_i16
; CHECK-NEXT:    [[M:%.*]] = fmul float %x, %x
; CHECK-NEXT:    ret float [[M]]
  %r = call float @llvm.powi.f32.i16(float %x, i16 2)
  ret float %r
}

define float @pow3(float %x) {
; CHECK-LABEL: @pow3
; CHECK-NEXT:    [[M1:%.*]] = fmul float %x, %x
; CHECK-NEXT:    [[M2:%.*]] = fmul float [[M1]], %x
; CHECK-NEXT:    ret float [[M2]]
  %r = call float @llvm.powi.f32.i32(float %x, i32 3)
  ret float %r
}

define float @pow4(float %x) {
; CHECK-LABEL: @pow4
; CHECK-NEXT:    [[M1:%.*]] = fmul float %x, %x
; CHECK-NEXT:    [[M2:%.*]] = fmul float [[M1]], [[M1]]
; CHECK-NEXT:    ret float [[M2]]
  %r = call float @llvm.powi.f32.i32(float %x, i32 4)
  ret float %r
}

define double @pow2_i64(double %x) {
; CHECK-LABEL: @pow2_i64
; CHECK-NEXT:    [[M:%.*]] = fmul double %x, %x
; CHECK-NEXT:    ret double [[M]]
  %r = call double @llvm.powi.f64.i64(double %x, i64 2)
  ret double %r
}

define float @pow5_keep(float %x) {
; CHECK-LABEL: @pow5_keep
; CHECK-NEXT:    [[R:%.*]] = call float @llvm.powi.f32.i32(float %x, i32 5)
; CHECK-NEXT:    ret float [[R]]
  %r = call float @llvm.powi.f32.i32(float %x, i32 5)
  ret float %r
}

define float @pow_neg_keep(float %x) {
; CHECK-LABEL: @pow_neg_keep
; CHECK-NEXT:    [[R:%.*]] = call float @llvm.powi.f32.i32(float %x, i32 -1)
; CHECK-NEXT:    ret float [[R]]
  %r = call float @llvm.powi.f32.i32(float %x, i32 -1)
  ret float %r
}

define float @pow_var_keep(float %x, i32 %n) {
; CHECK-LABEL: @pow_var_keep
; CHECK-NEXT:    [[R:%.*]] = call float @llvm.powi.f32.i32(float %x, i32 %n)
; CHECK-NEXT:    ret float [[R]]
  %r = call float @llvm.powi.f32.i32(float %x, i32 %n)
  ret float %r
}

define <2 x float> @pow0_vec(<2 x float> %v) {
; CHECK-LABEL: @pow0_vec
; CHECK-NEXT:    ret <2 x float> splat (float 1.000000e+00)
  %r = call <2 x float> @llvm.powi.v2f32.i32(<2 x float> %v, i32 0)
  ret <2 x float> %r
}

