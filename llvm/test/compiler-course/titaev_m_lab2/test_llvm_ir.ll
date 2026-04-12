; RUN: opt -load-pass-plugin %llvmshlibdir/RemainderExpansionPlugin%pluginext \
; RUN: -passes=expand-rem -S %s | FileCheck %s

; CHECK-LABEL: @test_frem
; CHECK-NOT: frem
; CHECK: %rem.fdiv = fdiv float %a, %b
; CHECK: %rem.ftrunc = call float @llvm.trunc.f32(float %rem.fdiv)
; CHECK: %rem.fmul = fmul float %rem.ftrunc, %b
; CHECK: %rem.fres = fsub float %a, %rem.fmul
; CHECK: ret float %rem.fres
define float @test_frem(float %a, float %b) {
  %res = frem float %a, %b
  ret float %res
}

; CHECK-LABEL: @test_srem
; CHECK-NOT: srem
; CHECK: %rem.sdiv = sdiv i32 %a, %b
; CHECK: %rem.smul = mul i32 %rem.sdiv, %b
; CHECK: %rem.sres = sub i32 %a, %rem.smul
; CHECK: ret i32 %rem.sres
define i32 @test_srem(i32 %a, i32 %b) {
  %res = srem i32 %a, %b
  ret i32 %res
}

; CHECK-LABEL: @test_urem
; CHECK-NOT: urem
; CHECK: %rem.udiv = udiv i64 %a, %b
; CHECK: %rem.umul = mul i64 %rem.udiv, %b
; CHECK: %rem.ures = sub i64 %a, %rem.umul
; CHECK: ret i64 %rem.ures
define i64 @test_urem(i64 %a, i64 %b) {
  %res = urem i64 %a, %b
  ret i64 %res
}