; RUN: opt -load-pass-plugin %llvmshlibdir/konstantinov_s_lab2_LLVM_IR%pluginext \
; RUN: -passes=expandfma -S %s | FileCheck %s

; CHECK-LABEL: @test_no_intr_replace
; CHECK-NOT: fmuladd
; CHECK-NEXT: %o = fadd float %a, %b
; CHECK-NEXT: ret float %o
define float @test_no_intr_replace(float %a, float %b) {
  %o = fadd float %a, %b
  ret float %o
}

; CHECK-LABEL: @test_single_intr
; CHECK-NEXT: %mul_part = fmul float %a, %b
; CHECK-NEXT: %add_part = fadd float %mul_part, %c
; CHECK-NEXT: ret float %add_part
define float @test_single_intr(float %a, float %b, float %c) {
  %result = call float @llvm.fmuladd.f32(float %a, float %b, float %c)
  ret float %result
}

; CHECK-LABEL: @test_intr_removed
; CHECK-NOT: fmuladd
define float @test_intr_removed(float %a, float %b, float %c) {
  %result = call float @llvm.fmuladd.f32(float %a, float %b, float %c)
  ret float %result
}

; CHECK-LABEL: @test_two_intr
; CHECK-NOT: fmuladd
; CHECK-NEXT: %mul_part = fmul float %a, %b
; CHECK-NEXT: %add_part = fadd float %mul_part, %c
; CHECK-NEXT: %mul_part1 = fmul float %add_part, %b
; CHECK-NEXT: %add_part2 = fadd float %mul_part1, %c
define float @test_two_intr(float %a, float %b, float %c) {
  %d = call float @llvm.fmuladd.f32(float %a, float %b, float %c)
  %e = call float @llvm.fmuladd.f32(float %d, float %b, float %c)
  ret float %e
}

; CHECK-LABEL: @test_three_intr
; CHECK-NOT: fmuladd
; CHECK-NEXT: %mul_part = fmul float %a, %b
; CHECK-NEXT: %add_part = fadd float %mul_part, %c
; CHECK-NEXT: %mul_part1 = fmul float %add_part, %b
; CHECK-NEXT: %add_part2 = fadd float %mul_part1, %c
; CHECK-NEXT: %mul_part3 = fmul float %add_part, %add_part2
; CHECK-NEXT: %add_part4 = fadd float %mul_part3, %c
define float @test_three_intr(float %a, float %b, float %c) {
  %d = call float @llvm.fmuladd.f32(float %a, float %b, float %c)
  %e = call float @llvm.fmuladd.f32(float %d, float %b, float %c)
  %f = call float @llvm.fmuladd.f32(float %d, float %e, float %c)
  ret float %e
}

; CHECK-LABEL: @test_fastmathflag
; CHECK-NOT: fmuladd
; CHECK-NEXT: %mul_part = fmul fast float %a, %b
; CHECK-NEXT: %add_part = fadd fast float %mul_part, %c
define float @test_fastmathflag(float %a, float %b, float %c) {
  %result = call fast float @llvm.fmuladd.f32(float %a, float %b, float %c)
  ret float %result
}

; CHECK-LABEL: @test_double
; CHECK-NOT: fmuladd
; CHECK-NEXT: %mul_part = fmul double %a, %b
; CHECK-NEXT: %add_part = fadd double %mul_part, %c
define double @test_double(double %a, double %b, double %c) {
  %result = call double @llvm.fmuladd.f64(double %a, double %b, double %c)
  ret double %result
}

; CHECK-LABEL: @test_8floats(
; CHECK-NOT: fmuladd
; CHECK-NEXT:  %mul_part = fmul <8 x float> %a, %b
; CHECK-NEXT:  %add_part = fadd <8 x float> %mul_part, %c
; CHECK-NEXT:  ret <8 x float> %add_part
define <8 x float> @test_8floats(<8 x float> %a, <8 x float> %b, <8 x float> %c) {
  %result = call <8 x float> @llvm.fmuladd.v8f32(<8 x float> %a, <8 x float> %b, <8 x float> %c)
  ret <8 x float> %result
}