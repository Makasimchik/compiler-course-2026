; RUN: opt -load-pass-plugin %llvmshlibdir/volkov_a_lab2_LLVM_IR%pluginext \
; RUN:     -passes=volkov-powi-opt -S %s | FileCheck %s

declare float @llvm.powi.f32.i32(float, i32)
declare double @llvm.powi.f64.i32(double, i32)
declare fp128 @llvm.powi.f128.i32(fp128, i32)
declare ppc_fp128 @llvm.powi.ppcf128.i32(ppc_fp128, i32)
declare <2 x double> @llvm.powi.v2f64.i32(<2 x double>, i32)
declare <4 x float> @llvm.powi.v4f32.i32(<4 x float>, i32)
declare <8 x float> @llvm.powi.v8f32.i32(<8 x float>, i32)

; скипаем неподдерживаемые кейсы (отрицательные, >4, не константы)
define float @check_skip_pow5(float %a) {
; CHECK-LABEL: @check_skip_pow5(
; CHECK-NEXT:    [[RES:%.*]] = call float @llvm.powi.f32.i32(float %a, i32 5)
; CHECK-NEXT:    ret float [[RES]]
  %out = call float @llvm.powi.f32.i32(float %a, i32 5)
  ret float %out
}

define double @check_skip_negative(double %a) {
; CHECK-LABEL: @check_skip_negative(
; CHECK-NEXT:    [[RES:%.*]] = call double @llvm.powi.f64.i32(double %a, i32 -2)
; CHECK-NEXT:    ret double [[RES]]
  %out = call double @llvm.powi.f64.i32(double %a, i32 -2)
  ret double %out
}

define float @check_skip_variable(float %a, i32 %p) {
; CHECK-LABEL: @check_skip_variable(
; CHECK-NEXT:    [[RES:%.*]] = call float @llvm.powi.f32.i32(float %a, i32 %p)
; CHECK-NEXT:    ret float [[RES]]
  %out = call float @llvm.powi.f32.i32(float %a, i32 %p)
  ret float %out
}

; скалярные тесты
define float @check_f32_pow0(float %a) {
; CHECK-LABEL: @check_f32_pow0(
; CHECK-NEXT:    ret float 1.000000e+00
  %out = call float @llvm.powi.f32.i32(float %a, i32 0)
  ret float %out
}

define fp128 @check_f128_pow1(fp128 %a) {
; CHECK-LABEL: @check_f128_pow1(
; CHECK-NEXT:    ret fp128 %a
  %out = call fp128 @llvm.powi.f128.i32(fp128 %a, i32 1)
  ret fp128 %out
}

define double @check_f64_pow2(double %a) {
; CHECK-LABEL: @check_f64_pow2(
; CHECK-NEXT:    [[M1:%.*]] = fmul double %a, %a
; CHECK-NEXT:    ret double [[M1]]
  %out = call double @llvm.powi.f64.i32(double %a, i32 2)
  ret double %out
}

define float @check_f32_pow3(float %a) {
; CHECK-LABEL: @check_f32_pow3(
; CHECK-NEXT:    [[M1:%.*]] = fmul float %a, %a
; CHECK-NEXT:    [[M2:%.*]] = fmul float [[M1]], %a
; CHECK-NEXT:    ret float [[M2]]
  %out = call float @llvm.powi.f32.i32(float %a, i32 3)
  ret float %out
}

define ppc_fp128 @check_ppc_pow4(ppc_fp128 %a) {
; CHECK-LABEL: @check_ppc_pow4(
; CHECK-NEXT:    [[M1:%.*]] = fmul ppc_fp128 %a, %a
; CHECK-NEXT:    [[M2:%.*]] = fmul ppc_fp128 [[M1]], [[M1]]
; CHECK-NEXT:    ret ppc_fp128 [[M2]]
  %out = call ppc_fp128 @llvm.powi.ppcf128.i32(ppc_fp128 %a, i32 4)
  ret ppc_fp128 %out
}

; векторные тесты
define <2 x double> @check_v2f64_pow0(<2 x double> %v) {
; CHECK-LABEL: @check_v2f64_pow0(
; CHECK-NEXT:    ret <2 x double> splat (double 1.000000e+00)
  %out = call <2 x double> @llvm.powi.v2f64.i32(<2 x double> %v, i32 0)
  ret <2 x double> %out
}

define <8 x float> @check_v8f32_pow1(<8 x float> %v) {
; CHECK-LABEL: @check_v8f32_pow1(
; CHECK-NEXT:    ret <8 x float> %v
  %out = call <8 x float> @llvm.powi.v8f32.i32(<8 x float> %v, i32 1)
  ret <8 x float> %out
}

define <4 x float> @check_v4f32_pow2(<4 x float> %v) {
; CHECK-LABEL: @check_v4f32_pow2(
; CHECK-NEXT:    [[M1:%.*]] = fmul <4 x float> %v, %v
; CHECK-NEXT:    ret <4 x float> [[M1]]
  %out = call <4 x float> @llvm.powi.v4f32.i32(<4 x float> %v, i32 2)
  ret <4 x float> %out
}

define <8 x float> @check_v8f32_pow3(<8 x float> %v) {
; CHECK-LABEL: @check_v8f32_pow3(
; CHECK-NEXT:    [[M1:%.*]] = fmul <8 x float> %v, %v
; CHECK-NEXT:    [[M2:%.*]] = fmul <8 x float> [[M1]], %v
; CHECK-NEXT:    ret <8 x float> [[M2]]
  %out = call <8 x float> @llvm.powi.v8f32.i32(<8 x float> %v, i32 3)
  ret <8 x float> %out
}

define <2 x double> @check_v2f64_pow4(<2 x double> %v) {
; CHECK-LABEL: @check_v2f64_pow4(
; CHECK-NEXT:    [[M1:%.*]] = fmul <2 x double> %v, %v
; CHECK-NEXT:    [[M2:%.*]] = fmul <2 x double> [[M1]], [[M1]]
; CHECK-NEXT:    ret <2 x double> [[M2]]
  %out = call <2 x double> @llvm.powi.v2f64.i32(<2 x double> %v, i32 4)
  ret <2 x double> %out
}
