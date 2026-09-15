; RUN: opt -load-pass-plugin %llvmshlibdir/eremin_v_lab_2_LLVM_IR%pluginext \
; RUN:     -passes="eremin_v_lab_2_frem_decompose" -S %s | FileCheck %s


define float @test_frem_float(float %a, float %b) {
; CHECK-LABEL: @test_frem_float(
; CHECK-NOT:        = frem
; CHECK:       %frem.div = fdiv float %a, %b
; CHECK-NEXT:  %frem.trunc = fptosi float %frem.div to i64
; CHECK-NEXT:  %frem.trunc.fp = sitofp i64 %frem.trunc to float
; CHECK-NEXT:  %frem.mul = fmul float %frem.trunc.fp, %b
; CHECK-NEXT:  %frem.sub = fsub float %a, %frem.mul
; CHECK-NEXT:  ret float %frem.sub
  %r = frem float %a, %b
  ret float %r
}


define double @test_frem_double(double %a, double %b) {
; CHECK-LABEL: @test_frem_double(
; CHECK-NOT:        = frem
; CHECK:       %frem.div = fdiv double %a, %b
; CHECK-NEXT:  %frem.trunc = fptosi double %frem.div to i64
; CHECK-NEXT:  %frem.trunc.fp = sitofp i64 %frem.trunc to double
; CHECK-NEXT:  %frem.mul = fmul double %frem.trunc.fp, %b
; CHECK-NEXT:  %frem.sub = fsub double %a, %frem.mul
; CHECK-NEXT:  ret double %frem.sub
  %r = frem double %a, %b
  ret double %r
}

define float @test_frem_fast(float %a, float %b) {
; CHECK-LABEL: @test_frem_fast(
; CHECK-NOT:        = frem
; CHECK:       %frem.div = fdiv fast float %a, %b
; CHECK-NEXT:  %frem.trunc = fptosi float %frem.div to i64
; CHECK-NEXT:  %frem.trunc.fp = sitofp i64 %frem.trunc to float
; CHECK-NEXT:  %frem.mul = fmul fast float %frem.trunc.fp, %b
; CHECK-NEXT:  %frem.sub = fsub fast float %a, %frem.mul
; CHECK-NEXT:  ret float %frem.sub
  %r = frem fast float %a, %b
  ret float %r
}


define i32 @test_srem_i32(i32 %a, i32 %b) {
; CHECK-LABEL: @test_srem_i32(
; CHECK-NOT:        = srem
; CHECK:       %srem.div = sdiv i32 %a, %b
; CHECK-NEXT:  %srem.mul = mul i32 %srem.div, %b
; CHECK-NEXT:  %srem.sub = sub i32 %a, %srem.mul
; CHECK-NEXT:  ret i32 %srem.sub
  %r = srem i32 %a, %b
  ret i32 %r
}


define i64 @test_srem_i64(i64 %a, i64 %b) {
; CHECK-LABEL: @test_srem_i64(
; CHECK-NOT:        = srem
; CHECK:       %srem.div = sdiv i64 %a, %b
; CHECK-NEXT:  %srem.mul = mul i64 %srem.div, %b
; CHECK-NEXT:  %srem.sub = sub i64 %a, %srem.mul
; CHECK-NEXT:  ret i64 %srem.sub
  %r = srem i64 %a, %b
  ret i64 %r
}


define i32 @test_urem_i32(i32 %a, i32 %b) {
; CHECK-LABEL: @test_urem_i32(
; CHECK-NOT:        = urem
; CHECK:       %urem.div = udiv i32 %a, %b
; CHECK-NEXT:  %urem.mul = mul i32 %urem.div, %b
; CHECK-NEXT:  %urem.sub = sub i32 %a, %urem.mul
; CHECK-NEXT:  ret i32 %urem.sub
  %r = urem i32 %a, %b
  ret i32 %r
}


define i64 @test_urem_i64(i64 %a, i64 %b) {
; CHECK-LABEL: @test_urem_i64(
; CHECK-NOT:        = urem
; CHECK:       %urem.div = udiv i64 %a, %b
; CHECK-NEXT:  %urem.mul = mul i64 %urem.div, %b
; CHECK-NEXT:  %urem.sub = sub i64 %a, %urem.mul
; CHECK-NEXT:  ret i64 %urem.sub
  %r = urem i64 %a, %b
  ret i64 %r
}


define i32 @test_multiple_rem(i32 %a, i32 %b, i32 %c) {
; CHECK-LABEL: @test_multiple_rem(
; CHECK-NOT:        = srem
; CHECK:       %srem.div = sdiv i32 %a, %b
; CHECK-NEXT:  %srem.mul = mul i32 %srem.div, %b
; CHECK-NEXT:  %srem.sub = sub i32 %a, %srem.mul
; CHECK-NEXT:  %srem.div1 = sdiv i32 %srem.sub, %c
; CHECK-NEXT:  %srem.mul2 = mul i32 %srem.div1, %c
; CHECK-NEXT:  %srem.sub3 = sub i32 %srem.sub, %srem.mul2
; CHECK-NEXT:  ret i32 %srem.sub3
  %r1 = srem i32 %a, %b
  %r2 = srem i32 %r1, %c
  ret i32 %r2
}


define i32 @test_no_rem(i32 %a, i32 %b) {
; CHECK-LABEL: @test_no_rem(
; CHECK:       %r = add i32 %a, %b
; CHECK-NEXT:  ret i32 %r
  %r = add i32 %a, %b
  ret i32 %r
}


define <4 x float> @test_frem_vec4(<4 x float> %a, <4 x float> %b) {
; CHECK-LABEL: @test_frem_vec4(
; CHECK-NOT:        = frem
; CHECK:       %frem.div = fdiv <4 x float> %a, %b
; CHECK-NEXT:  %frem.trunc = fptosi <4 x float> %frem.div to <4 x i64>
; CHECK-NEXT:  %frem.trunc.fp = sitofp <4 x i64> %frem.trunc to <4 x float>
; CHECK-NEXT:  %frem.mul = fmul <4 x float> %frem.trunc.fp, %b
; CHECK-NEXT:  %frem.sub = fsub <4 x float> %a, %frem.mul
; CHECK-NEXT:  ret <4 x float> %frem.sub
  %r = frem <4 x float> %a, %b
  ret <4 x float> %r
}
