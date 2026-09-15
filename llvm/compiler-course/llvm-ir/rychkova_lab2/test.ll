; RUN: opt -load-pass-plugin %llvmshlibdir/rychkova_lab2_LLVM_IR%pluginext \
; RUN: -passes=div2shift -S %s | FileCheck %s

define i32 @test_mul_2(i32 %a) {
; CHECK-LABEL: @test_mul_2
; CHECK-NEXT: %mul2shl = shl i32 %a, 1
; CHECK-NEXT: ret i32 %mul2shl
  %res = mul i32 %a, 2
  ret i32 %res
}

define i32 @test_mul_4(i32 %a) {
; CHECK-LABEL: @test_mul_4
; CHECK-NEXT: %mul2shl = shl i32 %a, 2
; CHECK-NEXT: ret i32 %mul2shl
  %res = mul i32 %a, 4
  ret i32 %res
}

define i32 @test_mul_8(i32 %a) {
; CHECK-LABEL: @test_mul_8
; CHECK-NEXT: %mul2shl = shl i32 %a, 3
; CHECK-NEXT: ret i32 %mul2shl
  %res = mul i32 %a, 8
  ret i32 %res
}

define i32 @test_mul_16(i32 %a) {
; CHECK-LABEL: @test_mul_16
; CHECK-NEXT: %mul2shl = shl i32 %a, 4
; CHECK-NEXT: ret i32 %mul2shl
  %res = mul i32 %a, 16
  ret i32 %res
}

define i32 @test_mul_32(i32 %a) {
; CHECK-LABEL: @test_mul_32
; CHECK-NEXT: %mul2shl = shl i32 %a, 5
; CHECK-NEXT: ret i32 %mul2shl
  %res = mul i32 %a, 32
  ret i32 %res
}

define i32 @test_mul_64(i32 %a) {
; CHECK-LABEL: @test_mul_64
; CHECK-NEXT: %mul2shl = shl i32 %a, 6
; CHECK-NEXT: ret i32 %mul2shl
  %res = mul i32 %a, 64
  ret i32 %res
}

define i32 @test_mul_not_pow2(i32 %a) {
; CHECK-LABEL: @test_mul_not_pow2
; CHECK-NEXT: %res = mul i32 %a, 6
; CHECK-NEXT: ret i32 %res
  %res = mul i32 %a, 6
  ret i32 %res
}

define i32 @test_mul_1(i32 %a) {
; CHECK-LABEL: @test_mul_1
; CHECK-NEXT: %res = mul i32 %a, 1
; CHECK-NEXT: ret i32 %res
  %res = mul i32 %a, 1
  ret i32 %res
}

define i32 @test_mul_0(i32 %a) {
; CHECK-LABEL: @test_mul_0
; CHECK-NEXT: %res = mul i32 %a, 0
; CHECK-NEXT: ret i32 %res
  %res = mul i32 %a, 0
  ret i32 %res
}

define i32 @test_udiv_2(i32 %a) {
; CHECK-LABEL: @test_udiv_2
; CHECK-NEXT: %udiv2shr = lshr i32 %a, 1
; CHECK-NEXT: ret i32 %udiv2shr
  %res = udiv i32 %a, 2
  ret i32 %res
}

define i32 @test_udiv_4(i32 %a) {
; CHECK-LABEL: @test_udiv_4
; CHECK-NEXT: %udiv2shr = lshr i32 %a, 2
; CHECK-NEXT: ret i32 %udiv2shr
  %res = udiv i32 %a, 4
  ret i32 %res
}

define i32 @test_udiv_8(i32 %a) {
; CHECK-LABEL: @test_udiv_8
; CHECK-NEXT: %udiv2shr = lshr i32 %a, 3
; CHECK-NEXT: ret i32 %udiv2shr
  %res = udiv i32 %a, 8
  ret i32 %res
}

define i32 @test_udiv_not_pow2(i32 %a) {
; CHECK-LABEL: @test_udiv_not_pow2
; CHECK-NEXT: %res = udiv i32 %a, 6
; CHECK-NEXT: ret i32 %res
  %res = udiv i32 %a, 6
  ret i32 %res
}

define i32 @test_sdiv_4_positive(i32 %a) {
; CHECK-LABEL: @test_sdiv_4_positive
; CHECK: %sdiv2ashr = select
; CHECK-NEXT: ret i32 %sdiv2ashr
  %res = sdiv i32 %a, 4
  ret i32 %res
}

define i64 @test_mul_i64(i64 %a) {
; CHECK-LABEL: @test_mul_i64
; CHECK-NEXT: %mul2shl = shl i64 %a, 3
; CHECK-NEXT: ret i64 %mul2shl
  %res = mul i64 %a, 8
  ret i64 %res
}

define i16 @test_mul_i16(i16 %a) {
; CHECK-LABEL: @test_mul_i16
; CHECK-NEXT: %mul2shl = shl i16 %a, 2
; CHECK-NEXT: ret i16 %mul2shl
  %res = mul i16 %a, 4
  ret i16 %res
}

define i32 @test_multiple_ops(i32 %a, i32 %b) {
; CHECK-LABEL: @test_multiple_ops
; CHECK-NEXT: %mul2shl = shl i32 %a, 1
; CHECK-NEXT: %udiv2shr = lshr i32 %b, 3
; CHECK-NEXT: %add = add i32 %mul2shl, %udiv2shr
; CHECK-NEXT: ret i32 %add
  %mul = mul i32 %a, 2
  %div = udiv i32 %b, 8
  %res = add i32 %mul, %div
  ret i32 %res
}