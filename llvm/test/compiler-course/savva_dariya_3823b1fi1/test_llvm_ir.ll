; RUN: opt -load-pass-plugin %llvmshlibdir/SavvaMulDivToShiftPass_Savva_Dariya_FIIT1_LLVM_IR%pluginext \
; RUN: -passes=savva-mul-div-to-shift-pass -S %s | FileCheck %s


; ТЕСТ 1: Умножение на степень двойки (8)
; CHECK-LABEL: define i32 @test_mul
; CHECK: %savva.shl = shl i32 %a, 3
; CHECK-NEXT: ret i32 %savva.shl
define i32 @test_mul(i32 %a) {
  %res = mul i32 %a, 8
  ret i32 %res
}

; ТЕСТ 2: Умножение с константой слева (16) — уникальная особенность вашего кода!
; CHECK-LABEL: define i32 @test_mul_comm
; CHECK: %savva.shl = shl i32 %a, 4
; CHECK-NEXT: ret i32 %savva.shl
define i32 @test_mul_comm(i32 %a) {
  %res = mul i32 16, %a
  ret i32 %res
}

; ТЕСТ 3: Умножение на отрицательную степень двойки (НЕ оптимизируется)
; CHECK-LABEL: define i32 @test_mul_negative
; CHECK: %res = mul i32 %a, -8
; CHECK-NOT: savva.shl
define i32 @test_mul_negative(i32 %a) {
  %res = mul i32 %a, -8
  ret i32 %res
}

; ТЕСТ 4: Беззнаковое деление (UDiv) на 4
; CHECK-LABEL: define i32 @test_udiv
; CHECK: %savva.lshr = lshr i32 %a, 2
; CHECK-NEXT: ret i32 %savva.lshr
define i32 @test_udiv(i32 %a) {
  %res = udiv i32 %a, 4
  ret i32 %res
}

; ТЕСТ 5: Знаковое деление (SDiv) на 2 с коррекцией
; CHECK-LABEL: define i32 @test_sdiv
; CHECK: %savva.isneg = icmp slt i32 %a, 0
; CHECK: %savva.corr = select i1 %savva.isneg, i32 1, i32 0
; CHECK: %savva.adjusted = add i32 %a, %savva.corr
; CHECK: %savva.ashr = ashr i32 %savva.adjusted, 1
; CHECK-NEXT: ret i32 %savva.ashr
define i32 @test_sdiv(i32 %a) {
  %res = sdiv i32 %a, 2
  ret i32 %res
}


; ТЕСТ 6: Знаковое деление на отрицательную степень двойки (НЕ оптимизируется)
; CHECK-LABEL: define i32 @test_sdiv_negative
; CHECK: %res = sdiv i32 %a, -4
; CHECK-NOT: savva.ashr
define i32 @test_sdiv_negative(i32 %a) {
  %res = sdiv i32 %a, -4
  ret i32 %res
}


; ТЕСТ 7: не степень двойки — умножение
; CHECK-LABEL: define i32 @test_no_opt_mul
; CHECK: %res = mul i32 %a, 7
; CHECK-NOT: savva.shl
define i32 @test_no_opt_mul(i32 %a) {
  %res = mul i32 %a, 7
  ret i32 %res
}

; ТЕСТ 8: не степень двойки — деление
; CHECK-LABEL: define i32 @test_no_opt_sdiv
; CHECK: %res = sdiv i32 %a, 3
; CHECK-NOT: savva.ashr
define i32 @test_no_opt_sdiv(i32 %a) {
  %res = sdiv i32 %a, 3
  ret i32 %res
}

; ТЕСТ 9: Тип i8 (проверка работы с малыми типами)
; CHECK-LABEL: define i8 @test_i8
; CHECK: %savva.shl = shl i8 %a, 2
define i8 @test_i8(i8 %a) {
  %res = mul i8 %a, 4
  ret i8 %res
}

; ТЕСТ 10: Тип i64 (проверка APInt для больших типов)
; CHECK-LABEL: define i64 @test_i64
; CHECK: %savva.lshr = lshr i64 %a, 10
define i64 @test_i64(i64 %a) {
  %res = udiv i64 %a, 1024
  ret i64 %res
}

; ТЕСТ 11: Тип i16
; CHECK-LABEL: define i16 @test_i16
; CHECK: %savva.ashr = ashr i16 %savva.adjusted, 3
define i16 @test_i16(i16 %a) {
  %res = sdiv i16 %a, 8
  ret i16 %res
}