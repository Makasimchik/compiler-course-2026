; RUN: opt -load-pass-plugin %llvmshlibdir/titaev_m_lab2_LLVM_IR%pluginext \
; RUN: -passes=decompose-remainder -S %s | FileCheck %s

; Тест 1: Проверка знакового деления на 16-битном целом числе
; CHECK-LABEL: @process_signed_i16
; CHECK-NEXT: [[DIV:%[a-z0-9.]+]] = sdiv i16 %val1, %val2
; CHECK-NEXT: [[MUL:%[a-z0-9.]+]] = mul i16 [[DIV]], %val2
; CHECK-NEXT: [[RES:%[a-z0-9.]+]] = sub i16 %val1, [[MUL]]
; CHECK-NEXT: ret i16 [[RES]]
define i16 @process_signed_i16(i16 %val1, i16 %val2) {
  %rem = srem i16 %val1, %val2
  ret i16 %rem
}

; Тест 2: Проверка беззнакового деления на 64-битном числе
; CHECK-LABEL: @process_unsigned_i64
; CHECK: [[UDIV:%.+]] = udiv i64 %input_a, %input_b
; CHECK: [[UMUL:%.+]] = mul i64 [[UDIV]], %input_b
; CHECK: [[URES:%.+]] = sub i64 %input_a, [[UMUL]]
define i64 @process_unsigned_i64(i64 %input_a, i64 %input_b) {
  %result = urem i64 %input_a, %input_b
  ret i64 %result
}

; Тест 3: Работа с плавающей точкой двойной точности (double)
; CHECK-LABEL: @frem_double_precision
; CHECK: [[FDIV:%.+]] = fdiv double %a, %b
; CHECK: [[TRUNC:%.+]] = call double @llvm.trunc.f64(double [[FDIV]])
; CHECK: [[FMUL:%.+]] = fmul double [[TRUNC]], %b
; CHECK: [[FREM:%.+]] = fsub double %a, [[FMUL]]
define double @frem_double_precision(double %a, double %b) {
  %res = frem double %a, %b
  ret double %res
}

; Тест 4: Работа с векторами другого размера (<4 x float>)
; CHECK-LABEL: @vector_simd_4x
; CHECK: [[VDIV:%.+]] = fdiv <4 x float> %v1, %v2
; CHECK: [[VTRUNC:%.+]] = call <4 x float> @llvm.trunc.v4f32(<4 x float> [[VDIV]])
; CHECK: [[VMUL:%.+]] = fmul <4 x float> [[VTRUNC]], %v2
; CHECK: [[VRES:%.+]] = fsub <4 x float> %v1, [[VMUL]]
define <4 x float> @vector_simd_4x(<4 x float> %v1, <4 x float> %v2) {
  %vrem = frem <4 x float> %v1, %v2
  ret <4 x float> %vrem
}

; Тест 5: Комбинированный случай
; CHECK-LABEL: @mixed_math
; CHECK: %sum = fadd float %x, 1.0
; CHECK: [[FDIV:%.+]] = fdiv float %sum, %y
; CHECK: [[FREM:%.+]] = fsub float %sum,
define float @mixed_math(float %x, float %y) {
  %sum = fadd float %x, 1.0
  %res = frem float %sum, %y
  ret float %res
}