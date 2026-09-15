; RUN: opt -load-pass-plugin %llvmshlibdir/nikitin_a_lab2_LLVM_IR%pluginext -passes=icmp-swap -S %s | FileCheck %s

; ============================================================================
; Тест 1: Базовый случай sgt -> sle + xor
; ============================================================================
; CHECK-LABEL: @test_sgt_basic
; CHECK: %[[CMP:.+]] = icmp sle i32 %a, %b
; CHECK-NEXT: %[[NOT:.+]] = xor i1 %[[CMP]], true
; CHECK-NEXT: ret i1 %[[NOT]]
define i1 @test_sgt_basic(i32 %a, i32 %b) {
  %cmp = icmp sgt i32 %a, %b
  ret i1 %cmp
}

; ============================================================================
; Тест 2: Базовый случай sge -> slt + xor
; ============================================================================
; CHECK-LABEL: @test_sge_basic
; CHECK: %[[CMP:.+]] = icmp slt i32 %a, %b
; CHECK-NEXT: %[[NOT:.+]] = xor i1 %[[CMP]], true
; CHECK-NEXT: ret i1 %[[NOT]]
define i1 @test_sge_basic(i32 %a, i32 %b) {
  %cmp = icmp sge i32 %a, %b
  ret i1 %cmp
}

; ============================================================================
; Тест 3: sgt в составе сложного выражения (zext + add)
; ============================================================================
; CHECK-LABEL: @test_sgt_complex
; CHECK: %[[CMP:.+]] = icmp sle i32 %a, %b
; CHECK-NEXT: %[[NOT:.+]] = xor i1 %[[CMP]], true
; CHECK-NEXT: %ext = zext i1 %[[NOT]] to i32
; CHECK-NEXT: %res = add i32 %ext, 5
; CHECK-NEXT: ret i32 %res
define i32 @test_sgt_complex(i32 %a, i32 %b) {
  %cmp = icmp sgt i32 %a, %b
  %ext = zext i1 %cmp to i32
  %res = add i32 %ext, 5
  ret i32 %res
}

; ============================================================================
; Тест 4: sge в составе br (условный переход)
; ============================================================================
; CHECK-LABEL: @test_sge_br
; CHECK: %[[CMP:.+]] = icmp slt i32 %a, %b
; CHECK-NEXT: %[[NOT:.+]] = xor i1 %[[CMP]], true
; CHECK-NEXT: br i1 %[[NOT]], label %then, label %else
define void @test_sge_br(i32 %a, i32 %b) {
  %cmp = icmp sge i32 %a, %b
  br i1 %cmp, label %then, label %else
then:
  ret void
else:
  ret void
}

; ============================================================================
; Тест 5: Несколько сравнений в одной функции (sgt и sge)
; ============================================================================
; CHECK-LABEL: @test_multiple
; CHECK: %[[CMP1:.+]] = icmp sle i32 %a, %b
; CHECK-NEXT: %[[NOT1:.+]] = xor i1 %[[CMP1]], true
; CHECK: %[[CMP2:.+]] = icmp slt i32 %c, %d
; CHECK-NEXT: %[[NOT2:.+]] = xor i1 %[[CMP2]], true
; CHECK: %and = and i1 %[[NOT1]], %[[NOT2]]
; CHECK-NEXT: ret i1 %and
define i1 @test_multiple(i32 %a, i32 %b, i32 %c, i32 %d) {
  %cmp1 = icmp sgt i32 %a, %b
  %cmp2 = icmp sge i32 %c, %d
  %and = and i1 %cmp1, %cmp2
  ret i1 %and
}

; ============================================================================
; Тест 6: Другие signed предикаты не трогаем
; ============================================================================
; CHECK-LABEL: @test_other_predicates
; CHECK: %cmp_eq = icmp eq i32 %a, %b
; CHECK: %cmp_ne = icmp ne i32 %a, %b
; CHECK: %cmp_slt = icmp slt i32 %a, %b
; CHECK: %cmp_sle = icmp sle i32 %a, %b
; CHECK-NOT: xor i1
define i1 @test_other_predicates(i32 %a, i32 %b) {
  %cmp_eq = icmp eq i32 %a, %b
  %cmp_ne = icmp ne i32 %a, %b
  %cmp_slt = icmp slt i32 %a, %b
  %cmp_sle = icmp sle i32 %a, %b
  %res = and i1 %cmp_eq, %cmp_ne
  %res2 = and i1 %res, %cmp_slt
  %res3 = and i1 %res2, %cmp_sle
  ret i1 %res3
}

; ============================================================================
; Тест 7: Использование в select
; ============================================================================
; CHECK-LABEL: @test_sgt_select
; CHECK: %[[CMP:.+]] = icmp sle i32 %a, %b
; CHECK-NEXT: %[[NOT:.+]] = xor i1 %[[CMP]], true
; CHECK-NEXT: %sel = select i1 %[[NOT]], i32 1, i32 0
; CHECK-NEXT: ret i32 %sel
define i32 @test_sgt_select(i32 %a, i32 %b) {
  %cmp = icmp sgt i32 %a, %b
  %sel = select i1 %cmp, i32 1, i32 0
  ret i32 %sel
}

; ============================================================================
; Тест 8: Операции с константами
; ============================================================================
; CHECK-LABEL: @test_constants
; CHECK: %[[CMP:.+]] = icmp sle i32 %a, 42
; CHECK-NEXT: %[[NOT:.+]] = xor i1 %[[CMP]], true
; CHECK-NEXT: ret i1 %[[NOT]]
define i1 @test_constants(i32 %a) {
  %cmp = icmp sgt i32 %a, 42
  ret i1 %cmp
}

; ============================================================================
; Тест 9: Сравнение с нулём
; ============================================================================
; CHECK-LABEL: @test_zero
; CHECK: %[[CMP:.+]] = icmp sle i32 %a, 0
; CHECK-NEXT: %[[NOT:.+]] = xor i1 %[[CMP]], true
; CHECK-NEXT: ret i1 %[[NOT]]
define i1 @test_zero(i32 %a) {
  %cmp = icmp sgt i32 %a, 0
  ret i1 %cmp
}

; ============================================================================
; Тест 10: sge с константами
; ============================================================================
; CHECK-LABEL: @test_sge_const
; CHECK: %[[CMP:.+]] = icmp slt i32 %a, 100
; CHECK-NEXT: %[[NOT:.+]] = xor i1 %[[CMP]], true
; CHECK-NEXT: ret i1 %[[NOT]]
define i1 @test_sge_const(i32 %a) {
  %cmp = icmp sge i32 %a, 100
  ret i1 %cmp
}

; ============================================================================
; Тест 11: Векторные типы
; ============================================================================
; CHECK-LABEL: @test_vector
; CHECK: %[[CMP:.+]] = icmp sle <2 x i32> %a, %b
; CHECK-NEXT: %[[NOT:.+]] = xor <2 x i1> %[[CMP]], splat (i1 true)
; CHECK-NEXT: ret <2 x i1> %[[NOT]]
define <2 x i1> @test_vector(<2 x i32> %a, <2 x i32> %b) {
  %cmp = icmp sgt <2 x i32> %a, %b
  ret <2 x i1> %cmp
}

; ============================================================================
; Тест 12: Сравнение с i64
; ============================================================================
; CHECK-LABEL: @test_i64
; CHECK: %[[CMP:.+]] = icmp sle i64 %a, %b
; CHECK-NEXT: %[[NOT:.+]] = xor i1 %[[CMP]], true
; CHECK-NEXT: ret i1 %[[NOT]]
define i1 @test_i64(i64 %a, i64 %b) {
  %cmp = icmp sgt i64 %a, %b
  ret i1 %cmp
}