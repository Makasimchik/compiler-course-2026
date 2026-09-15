; RUN: opt -load-pass-plugin %llvmshlibdir/titaev_m_lab2_LLVM_IR%pluginext \
; RUN: -passes=replace-add -S %s | FileCheck %s

; CHECK-LABEL: define i32 @add(
; CHECK: %result = add i32 %a, %b
; CHECK: ret i32 %result
define i32 @add(i32 %a, i32 %b) {
  %result = add i32 %a, %b
  ret i32 %result
}

; CHECK-LABEL: define i32 @foo(
; CHECK: %sum = call i32 @add(i32 %x, i32 %y)
; CHECK: ret i32 %sum
define i32 @foo(i32 %x, i32 %y) {
  %sum = add i32 %x, %y
  ret i32 %sum
}

; CHECK-LABEL: define i64 @wrong_type(
; CHECK: %sum = add i64 %x, %y
; CHECK: ret i64 %sum
define i64 @wrong_type(i64 %x, i64 %y) {
  %sum = add i64 %x, %y
  ret i64 %sum
}

; CHECK-LABEL: define i32 @sub_example(
; CHECK: %res = sub i32 %x, %y
define i32 @sub_example(i32 %x, i32 %y) {
  %res = sub i32 %x, %y
  ret i32 %res
}
