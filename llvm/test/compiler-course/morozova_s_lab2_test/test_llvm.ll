; RUN: opt -load-pass-plugin=%llvmshlibdir/ReplacePowi%shlibext -passes="replace-powi" -S %s | FileCheck %s

define float @pow0_float(float %x) {
; CHECK-LABEL: @pow0_float
; CHECK-NEXT: ret float 1.000000e+00
  %call = call float @llvm.powi.f32.i32(float %x, i32 0)
  ret float %call
}

define float @pow1_float(float %x) {
; CHECK-LABEL: @pow1_float
; CHECK-NEXT: ret float %x
  %call = call float @llvm.powi.f32.i32(float %x, i32 1)
  ret float %call
}

define float @pow2_float(float %x) {
; CHECK-LABEL: @pow2_float
; CHECK: fmul float %x, %x
; CHECK-NEXT: ret float
  %call = call float @llvm.powi.f32.i32(float %x, i32 2)
  ret float %call
}

define float @pow3_float(float %x) {
; CHECK-LABEL: @pow3_float
; CHECK: fmul float %x, %x
; CHECK-NEXT: fmul float {{.*}}, %x
; CHECK-NEXT: ret float
  %call = call float @llvm.powi.f32.i32(float %x, i32 3)
  ret float %call
}

define float @pow4_float(float %x) {
; CHECK-LABEL: @pow4_float
; CHECK: fmul float %x, %x
; CHECK-NEXT: fmul float {{.*}}, {{.*}}
; CHECK-NEXT: ret float
  %call = call float @llvm.powi.f32.i32(float %x, i32 4)
  ret float %call
}

define double @pow0_double(double %x) {
; CHECK-LABEL: @pow0_double
; CHECK-NEXT: ret double 1.000000e+00
  %call = call double @llvm.powi.f64.i32(double %x, i32 0)
  ret double %call
}

define double @pow1_double(double %x) {
; CHECK-LABEL: @pow1_double
; CHECK-NEXT: ret double %x
  %call = call double @llvm.powi.f64.i32(double %x, i32 1)
  ret double %call
}

define double @pow2_double(double %x) {
; CHECK-LABEL: @pow2_double
; CHECK: fmul double %x, %x
; CHECK-NEXT: ret double
  %call = call double @llvm.powi.f64.i32(double %x, i32 2)
  ret double %call
}

define double @pow3_double(double %x) {
; CHECK-LABEL: @pow3_double
; CHECK: fmul double %x, %x
; CHECK-NEXT: fmul double {{.*}}, %x
; CHECK-NEXT: ret double
  %call = call double @llvm.powi.f64.i32(double %x, i32 3)
  ret double %call
}

define double @pow4_double(double %x) {
; CHECK-LABEL: @pow4_double
; CHECK: fmul double %x, %x
; CHECK-NEXT: fmul double {{.*}}, {{.*}}
; CHECK-NEXT: ret double
  %call = call double @llvm.powi.f64.i32(double %x, i32 4)
  ret double %call
}

define <4 x float> @pow2_vector(<4 x float> %x) {
; CHECK-LABEL: @pow2_vector
; CHECK: fmul <4 x float> %x, %x
; CHECK-NEXT: ret <4 x float>
  %call = call <4 x float> @llvm.powi.v4f32.i32(<4 x float> %x, i32 2)
  ret <4 x float> %call
}

define <4 x float> @pow3_vector(<4 x float> %x) {
; CHECK-LABEL: @pow3_vector
; CHECK: fmul <4 x float> %x, %x
; CHECK-NEXT: fmul <4 x float> {{.*}}, %x
; CHECK-NEXT: ret <4 x float>
  %call = call <4 x float> @llvm.powi.v4f32.i32(<4 x float> %x, i32 3)
  ret <4 x float> %call
}

define <4 x float> @pow4_vector(<4 x float> %x) {
; CHECK-LABEL: @pow4_vector
; CHECK: fmul <4 x float> %x, %x
; CHECK-NEXT: fmul <4 x float> {{.*}}, {{.*}}
; CHECK-NEXT: ret <4 x float>
  %call = call <4 x float> @llvm.powi.v4f32.i32(<4 x float> %x, i32 4)
  ret <4 x float> %call
}

define float @negative_power_skip(float %x) {
; CHECK-LABEL: @negative_power_skip
; CHECK: call float @llvm.powi.f32.i32(float %x, i32 -2)
; CHECK-NEXT: ret float
  %call = call float @llvm.powi.f32.i32(float %x, i32 -2)
  ret float %call
}

define float @power_high_skip(float %x) {
; CHECK-LABEL: @power_high_skip
; CHECK: call float @llvm.powi.f32.i32(float %x, i32 10)
; CHECK-NEXT: ret float
  %call = call float @llvm.powi.f32.i32(float %x, i32 10)
  ret float %call
}

define float @nonconst_power_skip(float %x, i32 %n) {
; CHECK-LABEL: @nonconst_power_skip
; CHECK: call float @llvm.powi.f32.i32(float %x, i32 %n)
; CHECK-NEXT: ret float
  %call = call float @llvm.powi.f32.i32(float %x, i32 %n)
  ret float %call
}

define float @multiple_calls(float %x) {
; CHECK-LABEL: @multiple_calls
; CHECK: fadd float 1.000000e+00, %x
; CHECK: fadd float {{.*}}, {{.*}}
; CHECK: fadd float {{.*}}, {{.*}}
; CHECK: fadd float {{.*}}, {{.*}}
  %c0 = call float @llvm.powi.f32.i32(float %x, i32 0)
  %c1 = call float @llvm.powi.f32.i32(float %x, i32 1)
  %c2 = call float @llvm.powi.f32.i32(float %x, i32 2)
  %c3 = call float @llvm.powi.f32.i32(float %x, i32 3)
  %c4 = call float @llvm.powi.f32.i32(float %x, i32 4)
  %s1 = fadd float %c0, %c1
  %s2 = fadd float %s1, %c2
  %s3 = fadd float %s2, %c3
  %s4 = fadd float %s3, %c4
  ret float %s4
}

declare float @llvm.powi.f32.i32(float, i32)
declare double @llvm.powi.f64.i32(double, i32)
declare <4 x float> @llvm.powi.v4f32.i32(<4 x float>, i32)
