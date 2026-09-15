; RUN: opt -load-pass-plugin %llvmshlibdir/leonova_a_lab_2_LLVM_IR%pluginext \
; RUN: -passes=pow-of-2-to-shift -S %s | FileCheck %s

define i32 @mul_pow2(i32 %x) {
; CHECK-LABEL: @mul_pow2
; CHECK: shl i32 %x, 3
  %1 = mul i32 %x, 8
  ret i32 %1
}

define i32 @mul_pow2_neg(i32 %x) {
; CHECK-LABEL: @mul_pow2_neg
; CHECK: mul i32 %x, -8
  %1 = mul i32 %x, -8
  ret i32 %1
}

define i32 @udiv_pow2(i32 %x) {
; CHECK-LABEL: @udiv_pow2
; CHECK: lshr i32 %x, 2
  %1 = udiv i32 %x, 4
  ret i32 %1
}

define i32 @sdiv_pow2_pos(i32 %x) {
; CHECK-LABEL: @sdiv_pow2_pos
; CHECK: add i32 %x, %{{.*}}
; CHECK: ashr i32 %{{.*}}, 1
  %1 = sdiv i32 %x, 2
  ret i32 %1
}

define i32 @sdiv_pow2_neg(i32 %x) {
; CHECK-LABEL: @sdiv_pow2_neg
; CHECK: sdiv i32 %x, -2
  %1 = sdiv i32 %x, -2
  ret i32 %1
}

define i32 @mul_const_left(i32 %x) {
; CHECK-LABEL: @mul_const_left
; CHECK: shl i32 %x, 4
  %1 = mul i32 16, %x
  ret i32 %1
}

define i32 @mul_const_left_neg(i32 %x) {
; CHECK-LABEL: @mul_const_left_neg
; CHECK: mul i32 -16, %x
  %1 = mul i32 -16, %x
  ret i32 %1
}

define i32 @mul_not_pow2(i32 %x) {
; CHECK-LABEL: @mul_not_pow2
; CHECK: mul i32 %x, 6
  %1 = mul i32 %x, 6
  ret i32 %1
}

define i32 @mul_no_const(i32 %x, i32 %y) {
; CHECK-LABEL: @mul_no_const
; CHECK: mul i32 %x, %y
  %1 = mul i32 %x, %y
  ret i32 %1
}

define float @fmul_pow2(float %x) {
; CHECK-LABEL: @fmul_pow2
; CHECK: fmul float %x, 8.000000e+00
  %1 = fmul float %x, 8.0
  ret float %1
}

define i32 @multiple(i32 %x) {
; CHECK-LABEL: @multiple
; CHECK: shl i32 %x, 1
; CHECK: lshr i32 %x, 3
  %1 = mul i32 %x, 2
  %2 = udiv i32 %x, 8
  %3 = add i32 %1, %2
  ret i32 %3
}

define i32 @mul_by_one(i32 %x) {
; CHECK-LABEL: @mul_by_one
; CHECK: shl i32 %x, 0
  %1 = mul i32 %x, 1
  ret i32 %1
}

define i32 @udiv_by_one(i32 %x) {
; CHECK-LABEL: @udiv_by_one
; CHECK: lshr i32 %x, 0
  %1 = udiv i32 %x, 1
  ret i32 %1
}

define i64 @mul_i64(i64 %x) {
; CHECK-LABEL: @mul_i64
; CHECK: shl i64 %x, 5
  %1 = mul i64 %x, 32
  ret i64 %1
}

define i32 @mixed_ops(i32 %x) {
; CHECK-LABEL: @mixed_ops
; CHECK: add i32
; CHECK: shl i32 %x, 2
  %1 = add i32 %x, 5
  %2 = mul i32 %x, 4
  ret i32 %2
}

define i32 @mul_by_zero(i32 %x) {
; CHECK-LABEL: @mul_by_zero
; CHECK: mul i32 %x, 0
  %1 = mul i32 %x, 0
  ret i32 %1
}