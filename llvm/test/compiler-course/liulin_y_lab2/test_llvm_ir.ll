; RUN: opt -load-pass-plugin %llvmshlibdir/liulin_y_lab2_LLVM_IR%pluginext \
; RUN: -passes=load-store-elim -S %s | FileCheck %s

; Тест 1: store → load по тому же указателю (значение из переменной)
define i32 @_test1_store_then_load_var(i32 %x, i32* %ptr) {
; CHECK-LABEL: @_test1_store_then_load_var
; CHECK-NEXT:  store i32 %x, ptr %ptr, align 4
; CHECK-NEXT:  ret i32 %x
; CHECK-NOT:   load i32
  store i32 %x, i32* %ptr, align 4
  %val = load i32, i32* %ptr, align 4
  ret i32 %val
}

; Тест 2: store → load по тому же указателю (константное значение)
define i32 @_test2_store_then_load_const(i32* %ptr) {
; CHECK-LABEL: @_test2_store_then_load_const
; CHECK-NEXT:  store i32 42, ptr %ptr, align 4
; CHECK-NEXT:  ret i32 42
; CHECK-NOT:   load i32
  store i32 42, i32* %ptr, align 4
  %val = load i32, i32* %ptr, align 4
  ret i32 %val
}

; Тест 3: Два последовательных store по одному указателю, затем load
define i32 @_test3_store_store_load(i32* %ptr) {
; CHECK-LABEL: @_test3_store_store_load
; CHECK-NEXT:  store i32 20, ptr %ptr, align 4
; CHECK-NEXT:  ret i32 20
; CHECK-NOT:   store i32 10
; CHECK-NOT:   load i32
  store i32 10, i32* %ptr, align 4
  store i32 20, i32* %ptr, align 4
  %val = load i32, i32* %ptr, align 4
  ret i32 %val
}

; Тест 4: store → load → арифметическая операция
define i32 @_test4_load_used_in_arithmetic(i32* %ptr) {
; CHECK-LABEL: @_test4_load_used_in_arithmetic
; CHECK-NEXT:  store i32 5, ptr %ptr, align 4
; CHECK-NEXT:  %add = add i32 5, 3
; CHECK-NEXT:  ret i32 %add
; CHECK-NOT:   load i32
  store i32 5, i32* %ptr, align 4
  %val = load i32, i32* %ptr, align 4
  %add = add i32 %val, 3
  ret i32 %add
}

; Тест 5: Несколько load подряд после store
define i32 @_test5_multiple_loads_after_store(i32* %ptr) {
; CHECK-LABEL: @_test5_multiple_loads_after_store
; CHECK-NEXT:  store i32 7, ptr %ptr, align 4
; CHECK-NEXT:  %add = add i32 7, 7
; CHECK-NEXT:  ret i32 %add
; CHECK-NOT:   load i32
  store i32 7, i32* %ptr, align 4
  %val1 = load i32, i32* %ptr, align 4
  %val2 = load i32, i32* %ptr, align 4
  %add = add i32 %val1, %val2
  ret i32 %add
}

; Тест 6: store в разные указатели, затем load из первого
define i32 @_test6_different_pointers(i32* %ptrA, i32* %ptrB) {
; CHECK-LABEL: @_test6_different_pointers
; CHECK-NEXT:  store i32 100, ptr %ptrA, align 4
; CHECK-NEXT:  store i32 200, ptr %ptrB, align 4
; CHECK-NEXT:  ret i32 100
; CHECK-NOT:   load i32, ptr %ptrA
  store i32 100, i32* %ptrA, align 4
  store i32 200, i32* %ptrB, align 4
  %val = load i32, i32* %ptrA, align 4
  ret i32 %val
}

; Тест 7: Работа с типами float
define float @_test7_float_type(float %f, float* %ptr) {
; CHECK-LABEL: @_test7_float_type
; CHECK-NEXT:  store float %f, ptr %ptr, align 4
; CHECK-NEXT:  ret float %f
; CHECK-NOT:   load float
  store float %f, float* %ptr, align 4
  %val = load float, float* %ptr, align 4
  ret float %val
}

; Тест 8: volatile store → volatile load (не оптимизируются)
define i32 @_test8_volatile_store_load(i32* %ptr) {
; CHECK-LABEL: @_test8_volatile_store_load
; CHECK:       store volatile i32 42, ptr %ptr, align 4
; CHECK-NEXT:  %val = load volatile i32, ptr %ptr, align 4
; CHECK-NEXT:  ret i32 %val
; CHECK-NOT:   ret i32 42
  store volatile i32 42, i32* %ptr, align 4
  %val = load volatile i32, i32* %ptr, align 4
  ret i32 %val
}

; Тест 9: store → вызов функции (сброс состояния) → load
declare void @foo(ptr) ; функция может изменять память
define i32 @_test9_call_breaks_optimization(i32* %ptr) {
; CHECK-LABEL: @_test9_call_breaks_optimization
; CHECK:       store i32 99, ptr %ptr, align 4
; CHECK-NEXT:  call void @foo(ptr %ptr)
; CHECK-NEXT:  %val = load i32, ptr %ptr, align 4
; CHECK-NEXT:  ret i32 %val
; CHECK-NOT:   ret i32 99
  store i32 99, i32* %ptr, align 4
  call void @foo(i32* %ptr)
  %val = load i32, i32* %ptr, align 4
  ret i32 %val
}

; Тест 10: atomic load / atomic store
define i32 @_test10_atomic_operations(i32* %ptr) {
; CHECK-LABEL: @_test10_atomic_operations
; CHECK:       store atomic i32 5, ptr %ptr seq_cst, align 4
; CHECK-NEXT:  %val = load atomic i32, ptr %ptr seq_cst, align 4
; CHECK-NEXT:  ret i32 %val
; CHECK-NOT:   ret i32 5
  store atomic i32 5, i32* %ptr seq_cst, align 4
  %val = load atomic i32, i32* %ptr seq_cst, align 4
  ret i32 %val
}

; Тест 11: store по i32* и последующий load по float* (типы не совпадают)
define float @_test11_type_mismatch(i32* %iptr, float* %fptr) {
; CHECK-LABEL: @_test11_type_mismatch
; CHECK:       store i32 42, ptr %iptr, align 4
; CHECK-NEXT:  %val = load float, ptr %fptr, align 4
; CHECK-NEXT:  ret float %val
; Не должно быть подстановки константы 42
  store i32 42, i32* %iptr, align 4
  %val = load float, float* %fptr, align 4
  ret float %val
}

; Тест 12: Инструкция с побочными эффектами между store и load
; Используем `fence` как пример барьерной инструкции
define i32 @_test12_fence_barrier(i32* %ptr) {
; CHECK-LABEL: @_test12_fence_barrier
; CHECK:       store i32 77, ptr %ptr, align 4
; CHECK-NEXT:  fence seq_cst
; CHECK-NEXT:  %val = load i32, ptr %ptr, align 4
; CHECK-NEXT:  ret i32 %val
; CHECK-NOT:   ret i32 77
  store i32 77, i32* %ptr, align 4
  fence seq_cst
  %val = load i32, i32* %ptr, align 4
  ret i32 %val
}

; Тест 13: смешанные указатели с возможным алиасингом
declare void @may_write(ptr)
define i32 @_test13_unknown_call(i32* %ptr) {
; CHECK-LABEL: @_test13_unknown_call
; CHECK:       store i32 88, ptr %ptr, align 4
; CHECK-NEXT:  call void @may_write(ptr %ptr)
; CHECK-NEXT:  %val = load i32, ptr %ptr, align 4
; CHECK-NEXT:  ret i32 %val
  store i32 88, i32* %ptr, align 4
  call void @may_write(i32* %ptr)
  %val = load i32, i32* %ptr, align 4
  ret i32 %val
}