; RUN: split-file %s %t
; RUN: opt -load-pass-plugin %llvmshlibdir/titaev_m_lab2_LLVM_IR%pluginext \
; RUN: -passes=replace-binops -S %t/transform.ll | FileCheck %s --check-prefix=TRANSFORM
; RUN: opt -load-pass-plugin %llvmshlibdir/titaev_m_lab2_LLVM_IR%pluginext \
; RUN: -passes=replace-binops -S %t/no-transform.ll | FileCheck %s --check-prefix=NO-TRANSFORM


;--- transform.ll

define i32 @add(i32 %a, i32 %b) {
  %result = add i32 %a, %b
  ret i32 %result
}

define i32 @sub(i32 %a, i32 %b) {
  %result = sub i32 %a, %b
  ret i32 %result
}

define i32 @mul(i32 %a, i32 %b) {
  %result = mul i32 %a, %b
  ret i32 %result
}

; TRANSFORM-LABEL: define i32 @foo(
; TRANSFORM-NEXT: %sum = call i32 @add(i32 %x, i32 %y)
; TRANSFORM-NEXT: %difference = call i32 @sub(i32 %sum, i32 %y)
; TRANSFORM-NEXT: %product = call i32 @mul(i32 %difference, i32 %y)
; TRANSFORM-NEXT: ret i32 %product
define i32 @foo(i32 %x, i32 %y) {
  %sum = add i32 %x, %y
  %difference = sub i32 %sum, %y
  %product = mul i32 %difference, %y
  ret i32 %product
}


;--- no-transform.ll

define i64 @add(i64 %a, i64 %b) {
  %result = add i64 %a, %b
  ret i64 %result
}

; NO-TRANSFORM-LABEL: define i32 @foo(
; NO-TRANSFORM-NEXT: %sum = add i32 %x, %y
; NO-TRANSFORM-NEXT: %difference = sub i32 %sum, %y
; NO-TRANSFORM-NEXT: ret i32 %difference
define i32 @foo(i32 %x, i32 %y) {
  %sum = add i32 %x, %y
  %difference = sub i32 %sum, %y
  ret i32 %difference
}