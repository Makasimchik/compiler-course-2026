; RUN: opt -load-pass-plugin %llvmshlibdir/chaschin_v_replace_Pass%pluginext \
; RUN: -passes=resuct-shifts -S %s | FileCheck %s

define i32 @test_mul_16(i32 %val) {
; CHECK-LABEL: @test_mul_16
; CHECK: shl i32 %val, 4
  %res = mul i32 %val, 16
  ret i32 %res
}

define i32 @test_mul_negative(i32 %val) {
; CHECK-LABEL: @test_mul_negative
; CHECK: mul i32 %val, -16
  %res = mul i32 %val, -16
  ret i32 %res
}

define i32 @test_unsigned_div_64(i32 %val) {
; CHECK-LABEL: @test_unsigned_div_64
; CHECK: lshr i32 %val, 6
  %res = udiv i32 %val, 64
  ret i32 %res
}

define i32 @test_signed_div_4(i32 %val) {
; CHECK-LABEL: @test_signed_div_4
; CHECK: add i32
; CHECK: ashr i32
  %res = sdiv i32 %val, 4
  ret i32 %res
}

define i32 @test_signed_div_neg(i32 %val) {
; CHECK-LABEL: @test_signed_div_neg
; CHECK: sdiv i32 %val, -4
  %res = sdiv i32 %val, -4
  ret i32 %res
}

define i32 @test_mul_commutative(i32 %val) {
; CHECK-LABEL: @test_mul_commutative
; CHECK: shl i32 %val, 5
  %res = mul i32 32, %val
  ret i32 %res
}

define i32 @test_mul_non_power2(i32 %val) {
; CHECK-LABEL: @test_mul_non_power2
; CHECK: mul i32 %val, 10
  %res = mul i32 %val, 10
  ret i32 %res
}

define i32 @test_mul_vars(i32 %v1, i32 %v2) {
; CHECK-LABEL: @test_mul_vars
; CHECK: mul i32 %v1, %v2
  %res = mul i32 %v1, %v2
  ret i32 %res
}

define float @test_float_mul(float %val) {
; CHECK-LABEL: @test_float_mul
; CHECK: fmul float %val, 4.000000e+00
  %res = fmul float %val, 4.0
  ret float %res
}

define i64 @test_mul_i64(i64 %val) {
; CHECK-LABEL: @test_mul_i64
; CHECK: shl i64 %val, 7
  %res = mul i64 %val, 128
  ret i64 %res
}

define i32 @test_mul_zero(i32 %val) {
; CHECK-LABEL: @test_mul_zero
; CHECK: mul i32 %val, 0
  %res = mul i32 %val, 0
  ret i32 %res
}
