; RUN: opt -load-pass-plugin %llvmshlibdir/agafonov_i_lab2_LLVM_IR%pluginext \
; RUN: -passes=icmp-invert -S %s | FileCheck %s

; CHECK-LABEL: @test_sgt
; CHECK-NEXT: %cmp.rev = icmp sle i32 %a, %b
; CHECK-NEXT: %cmp.not = xor i1 %cmp.rev, true
; CHECK-NEXT: ret i1 %cmp.not
define i1 @test_sgt(i32 %a, i32 %b) {
  %cmp = icmp sgt i32 %a, %b
  ret i1 %cmp
}

; CHECK-LABEL: @test_sge
; CHECK-NEXT: %cmp.rev = icmp slt i32 %a, %b
; CHECK-NEXT: %cmp.not = xor i1 %cmp.rev, true
; CHECK-NEXT: ret i1 %cmp.not
define i1 @test_sge(i32 %a, i32 %b) {
  %cmp = icmp sge i32 %a, %b
  ret i1 %cmp
}

; CHECK-LABEL: @test_ugt
; CHECK-NEXT: %cmp.rev = icmp ule i32 %a, %b
; CHECK-NEXT: %cmp.not = xor i1 %cmp.rev, true
; CHECK-NEXT: ret i1 %cmp.not
define i1 @test_ugt(i32 %a, i32 %b) {
  %cmp = icmp ugt i32 %a, %b
  ret i1 %cmp
}

; CHECK-LABEL: @test_uge
; CHECK-NEXT: %cmp.rev = icmp ult i32 %a, %b
; CHECK-NEXT: %cmp.not = xor i1 %cmp.rev, true
; CHECK-NEXT: ret i1 %cmp.not
define i1 @test_uge(i32 %a, i32 %b) {
  %cmp = icmp uge i32 %a, %b
  ret i1 %cmp
}

; CHECK-LABEL: @test_eq
; CHECK-NEXT: %cmp = icmp eq i32 %a, %b
; CHECK-NEXT: ret i1 %cmp
define i1 @test_eq(i32 %a, i32 %b) {
  %cmp = icmp eq i32 %a, %b
  ret i1 %cmp
}