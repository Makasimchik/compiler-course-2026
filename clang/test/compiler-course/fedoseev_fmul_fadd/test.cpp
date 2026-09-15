; RUN: opt -load-pass-plugin=%builddir/lib/fmadplugin.so -passes=decompose-fmuladd -S %s | FileCheck %s --check-prefix=CHECK --implicit-check-not="llvm.fmuladd"
; REQUIRES: plugin

define float @test_f32(float %a, float %b, float %c) {
  %res = call float @llvm.fmuladd.f32(float %a, float %b, float %c)
  ret float %res
}

define double @test_f64(double %a, double %b, double %c) {
  %res = call double @llvm.fmuladd.f64(double %a, double %b, double %c)
  ret double %res
}

define half @test_f16(half %a, half %b, half %c) {
  %res = call half @llvm.fmuladd.f16(half %a, half %b, half %c)
  ret half %res
}

define <4 x float> @test_vec4(<4 x float> %a, <4 x float> %b, <4 x float> %c) {
  %res = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %a, <4 x float> %b, <4 x float> %c)
  ret <4 x float> %res
}

define <2 x double> @test_vec2f64(<2 x double> %a, <2 x double> %b, <2 x double> %c) {
  %res = call <2 x double> @llvm.fmuladd.v2f64(<2 x double> %a, <2 x double> %b, <2 x double> %c)
  ret <2 x double> %res
}

define float @test_fast(float %a, float %b, float %c) {
  %res = call fast float @llvm.fmuladd.f32(float %a, float %b, float %c)
  ret float %res
}

define float @test_nnan_ninf(float %a, float %b, float %c) {
  %res = call nnan ninf float @llvm.fmuladd.f32(float %a, float %b, float %c)
  ret float %res
}

define float @test_contract(float %a, float %b, float %c) {
  %res = call contract float @llvm.fmuladd.f32(float %a, float %b, float %c)
  ret float %res
}

define float @test_multi_use(float %a, float %b, float %c) {
  %fma = call float @llvm.fmuladd.f32(float %a, float %b, float %c)
  %add1 = fadd float %fma, 1.0
  %add2 = fadd float %fma, 2.0
  %r = fadd float %add1, %add2
  ret float %r
}

define float @test_two_fmas(float %a, float %b, float %c, float %d, float %e, float %f) {
  %fma1 = call float @llvm.fmuladd.f32(float %a, float %b, float %c)
  %fma2 = call float @llvm.fmuladd.f32(float %d, float %e, float %f)
  %sum = fadd float %fma1, %fma2
  ret float %sum
}

declare float @llvm.fmuladd.f32(float, float, float)
declare double @llvm.fmuladd.f64(double, double, double)
declare half @llvm.fmuladd.f16(half, half, half)
declare <4 x float> @llvm.fmuladd.v4f32(<4 x float>, <4 x float>, <4 x float>)
declare <2 x double> @llvm.fmuladd.v2f64(<2 x double>, <2 x double>, <2 x double>)

; CHECK-LABEL: define float @test_f32
; CHECK: %fmul = fmul float %a, %b
; CHECK: %fadd = fadd float %fmul, %c
; CHECK-NEXT: ret float %fadd

; CHECK-LABEL: define double @test_f64
; CHECK: %fmul = fmul double %a, %b
; CHECK: %fadd = fadd double %fmul, %c
; CHECK-NEXT: ret double %fadd

; CHECK-LABEL: define half @test_f16
; CHECK: %fmul = fmul half %a, %b
; CHECK: %fadd = fadd half %fmul, %c
; CHECK-NEXT: ret half %fadd

; CHECK-LABEL: define <4 x float> @test_vec4
; CHECK: %fmul = fmul <4 x float> %a, %b
; CHECK: %fadd = fadd <4 x float> %fmul, %c
; CHECK-NEXT: ret <4 x float> %fadd

; CHECK-LABEL: define <2 x double> @test_vec2f64
; CHECK: %fmul = fmul <2 x double> %a, %b
; CHECK: %fadd = fadd <2 x double> %fmul, %c
; CHECK-NEXT: ret <2 x double> %fadd

; CHECK-LABEL: define float @test_fast
; CHECK: %fmul = fmul fast float %a, %b
; CHECK: %fadd = fadd fast float %fmul, %c
; CHECK-NEXT: ret float %fadd

; CHECK-LABEL: define float @test_nnan_ninf
; CHECK: %fmul = fmul nnan ninf float %a, %b
; CHECK: %fadd = fadd nnan ninf float %fmul, %c
; CHECK-NEXT: ret float %fadd

; CHECK-LABEL: define float @test_contract
; CHECK: %fmul = fmul contract float %a, %b
; CHECK: %fadd = fadd contract float %fmul, %c
; CHECK-NEXT: ret float %fadd

; CHECK-LABEL: define float @test_multi_use
; CHECK: %fmul = fmul float %a, %b
; CHECK: %fadd = fadd float %fmul, %c
; CHECK: %add1 = fadd float %fadd, 1.0
; CHECK: %add2 = fadd float %fadd, 2.0
; CHECK: %r = fadd float %add1, %add2
; CHECK-NEXT: ret float %r

; CHECK-LABEL: define float @test_two_fmas
; CHECK: %fmul = fmul float %a, %b
; CHECK: %fadd = fadd float %fmul, %c
; CHECK: %fmul1 = fmul float %d, %e
; CHECK: %fadd2 = fadd float %fmul1, %f
; CHECK: %sum = fadd float %fadd, %fadd2
; CHECK-NEXT: ret float %sum