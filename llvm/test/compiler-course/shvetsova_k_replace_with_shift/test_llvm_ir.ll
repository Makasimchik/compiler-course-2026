; RUN: opt -load-pass-plugin %llvmshlibdir/shvetsova_k_replace_with_shift_LLVM_IR%pluginext\
; RUN: -passes=replaceWithShiftPass -S %s | FileCheck %s

; --- Умножение ---

;--- Умножение: Константа справа (val * 8) ---
; CHECK-LABEL: @test_mul_right
define i32 @test_mul_right(i32 %val) {
  ; CHECK: %leftShift = shl i32 %val, 3
  %res = mul i32 %val, 8
  ret i32 %res
}

; --- Умножение: Константа слева (16 * val) ---
; CHECK-LABEL: @test_mul_left
define i32 @test_mul_left(i32 %val) {
  ; CHECK: %leftShift = shl i32 %val, 4
  %res = mul i32 16, %val
  ret i32 %res
}

; CHECK-LABEL: @test_mul_i32
define i32 @test_mul_i32(i32 %val) {
  ; CHECK: %leftShift = shl i32 %val, 3
  %res = mul i32 %val, 8
  ret i32 %res
}

; --- Знаковое деление (SDiv)---


; CHECK-LABEL: @test_sdiv_i32
define i32 @test_sdiv_i32(i32 %val) {
  ; CHECK: %isNeg = icmp slt i32 %val, 0
  ; CHECK-NEXT: %bias = select i1 %isNeg, i32 3, i32 0
  ; CHECK-NEXT: %adjustedVal = add i32 %val, %bias
  ; CHECK-NEXT: %arithShiftRight = ashr i32 %adjustedVal, 2
  %res = sdiv i32 %val, 4
  ret i32 %res
}

; CHECK-LABEL: @test_sdiv_i8
define i8 @test_sdiv_i8(i8 %val) {
  ; CHECK: %isNeg = icmp slt i8 %val, 0
  ; CHECK-NEXT: %bias = select i1 %isNeg, i8 1, i8 0
  ; CHECK-NEXT: %adjustedVal = add i8 %val, %bias
  ; CHECK-NEXT: %arithShiftRight = ashr i8 %adjustedVal, 1
  %res = sdiv i8 %val, 2
  ret i8 %res
}

; CHECK-LABEL: @test_sdiv_i64
define i64 @test_sdiv_i64(i64 %val) {
  ; CHECK: %isNeg = icmp slt i64 %val, 0
  ; CHECK-NEXT: %bias = select i1 %isNeg, i64 15, i64 0
  ; CHECK-NEXT: %adjustedVal = add i64 %val, %bias
  ; CHECK-NEXT: %arithShiftRight = ashr i64 %adjustedVal, 4
  %res = sdiv i64 %val, 16
  ret i64 %res
}

; --- Специальные случаи ---

; CHECK-LABEL: @test_sdiv_exact
define i32 @test_sdiv_exact(i32 %val) {
  ; CHECK-NOT: select
  ; CHECK: %arithShiftRight = ashr i32 %val, 2
  %res = sdiv exact i32 %val, 4
  ret i32 %res
}

; CHECK-LABEL: @test_udiv_i16
define i16 @test_udiv_i16(i16 %val) {
  ; CHECK: %logicalShiftRight = lshr i16 %val, 3
  %res = udiv i16 %val, 8
  ret i16 %res
}

; --- Отрицательные тесты ---

; Делитель отрицательный (-4). isPowerOf2() должен вернуть false
; CHECK-LABEL: @test_sdiv_neg_divisor
define i32 @test_sdiv_neg_divisor(i32 %val) {
  ; CHECK: %res = sdiv i32 %val, -4
  ; CHECK-NOT: ashr
  %res = sdiv i32 %val, -4
  ret i32 %res
}

; Делитель не степень двойки (7)
; CHECK-LABEL: @test_mul_not_power_of_2
define i32 @test_mul_not_power_of_2(i32 %val) {
  ; CHECK: %res = mul i32 %val, 7
  %res = mul i32 %val, 7
  ret i32 %res
}

; Умножение на 1
; CHECK-LABEL: @test_mul_one
define i32 @test_mul_one(i32 %val) {
  ; CHECK: %leftShift = shl i32 %val, 0
  %res = mul i32 %val, 1
  ret i32 %res
}