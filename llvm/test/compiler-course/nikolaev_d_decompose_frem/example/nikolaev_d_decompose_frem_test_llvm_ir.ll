; RUN: opt -load-pass-plugin %llvmshlibdir/nikolaev_d_decompose_frem_LLVM_IR%pluginext \
; RUN: -passes=decompose-rem -S %s | FileCheck %s

; CHECK-LABEL: @_Z6isEveni
; CHECK:      %sdiv_tmp = sdiv i32 %3, 2
; CHECK-NEXT: %mul_tmp = mul i32 %sdiv_tmp, 2
; CHECK-NEXT: %sub_tmp = sub i32 %3, %mul_tmp
; CHECK-NEXT: %4 = icmp eq i32 %sub_tmp, 0
define dso_local noundef zeroext i1 @_Z6isEveni(i32 noundef %0) {
  %2 = alloca i32, align 4
  store i32 %0, ptr %2, align 4
  %3 = load i32, ptr %2, align 4
  %4 = srem i32 %3, 2
  %5 = icmp eq i32 %4, 0
  ret i1 %5
}

; CHECK-LABEL: @test_urem
; CHECK:      %udiv_tmp = udiv i32 %a, %b
; CHECK-NEXT: %mul_tmp = mul i32 %udiv_tmp, %b
; CHECK-NEXT: %sub_tmp = sub i32 %a, %mul_tmp
; CHECK-NEXT: ret i32 %sub_tmp
define i32 @test_urem(i32 %a, i32 %b) {
  %res = urem i32 %a, %b
  ret i32 %res
}

; CHECK-LABEL: @test_frem
; CHECK:      %fdiv_tmp = fdiv float %a, %b
; CHECK-NEXT: %trunc_tmp = call float @llvm.trunc.f32(float %fdiv_tmp)
; CHECK-NEXT: %fmul_tmp = fmul float %trunc_tmp, %b
; CHECK-NEXT: %fsub_tmp = fsub float %a, %fmul_tmp
; CHECK-NEXT: ret float %fsub_tmp
define float @test_frem(float %a, float %b) {
  %res = frem float %a, %b
  ret float %res
}

; CHECK-LABEL: @test_vec_srem
; CHECK:      %sdiv_tmp = sdiv <4 x i32> %a, %b
; CHECK-NEXT: %mul_tmp = mul <4 x i32> %sdiv_tmp, %b
; CHECK-NEXT: %sub_tmp = sub <4 x i32> %a, %mul_tmp
; CHECK-NEXT: ret <4 x i32> %sub_tmp
define <4 x i32> @test_vec_srem(<4 x i32> %a, <4 x i32> %b) {
  %res = srem <4 x i32> %a, %b
  ret <4 x i32> %res
}

; CHECK-LABEL: @test_vec_frem
; CHECK:      %fdiv_tmp = fdiv <4 x float> %a, %b
; CHECK-NEXT: %trunc_tmp = call <4 x float> @llvm.trunc.v4f32(<4 x float> %fdiv_tmp)
; CHECK-NEXT: %fmul_tmp = fmul <4 x float> %trunc_tmp, %b
; CHECK-NEXT: %fsub_tmp = fsub <4 x float> %a, %fmul_tmp
; CHECK-NEXT: ret <4 x float> %fsub_tmp
define <4 x float> @test_vec_frem(<4 x float> %a, <4 x float> %b) {
  %res = frem <4 x float> %a, %b
  ret <4 x float> %res
}