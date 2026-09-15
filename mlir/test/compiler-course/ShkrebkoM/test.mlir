// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/ShkrebkoM_MLIR%shlibext \
// RUN:   --pass-pipeline="builtin.module(shkrebko-trip-count)" %s | FileCheck %s
// ----------------------------------------------------------------------------
// Test 1: Simple loop with step = 1
// CHECK-LABEL: func.func @simple_step1
// CHECK: affine.for %{{.*}} = 0 to 5 {
// CHECK: } {trip_count = 5 : i64}
func.func @simple_step1() {
  affine.for %i = 0 to 5 {
    %0 = arith.constant 1 : i32
  }
  return
}

// ----------------------------------------------------------------------------
// Test 2: Loop with step = 2, lower = 1, upper = 10 -> (10-1+2-1)/2 = 5
// CHECK-LABEL: func.func @step2_lower1_upper10
// CHECK: affine.for %{{.*}} = 1 to 10 step 2 {
// CHECK: } {trip_count = 5 : i64}
func.func @step2_lower1_upper10() {
  affine.for %i = 1 to 10 step 2 {
    %0 = arith.constant 1 : i32
  }
  return
}

// ----------------------------------------------------------------------------
// Test 3: Negative lower bound, step = 3
// lower = -10, upper = 0 -> iterations = ceil((0 - (-10))/3) = ceil(10/3)=4
// CHECK-LABEL: func.func @negative_lower_bound_step3
// CHECK: affine.for %{{.*}} = -10 to 0 step 3 {
// CHECK: } {trip_count = 4 : i64}
func.func @negative_lower_bound_step3() {
  affine.for %i = -10 to 0 step 3 {
    %0 = arith.constant 1 : i32
  }
  return
}

// ----------------------------------------------------------------------------
// Test 4: Zero iterations because lower >= upper (lower = 10, upper = 5)
// CHECK-LABEL: func.func @lower_greater_than_upper
// CHECK: affine.for %{{.*}} = 10 to 5 {
// CHECK: } {trip_count = 0 : i64}
func.func @lower_greater_than_upper() {
  affine.for %i = 10 to 5 {
    %0 = arith.constant 1 : i32
  }
  return
}

// ----------------------------------------------------------------------------
// Test 5: Zero iterations because lower == upper
// CHECK-LABEL: func.func @equal_bounds
// CHECK: affine.for %{{.*}} = 3 to 3 {
// CHECK: } {trip_count = 0 : i64}
func.func @equal_bounds() {
  affine.for %i = 3 to 3 {
    %0 = arith.constant 1 : i32
  }
  return
}

// ----------------------------------------------------------------------------
// Test 6: Step larger than the range -> one iteration (ceil(2/5)=1)
// CHECK-LABEL: func.func @step_larger_than_range
// CHECK: affine.for %{{.*}} = 0 to 2 step 5 {
// CHECK: } {trip_count = 1 : i64}
func.func @step_larger_than_range() {
  affine.for %i = 0 to 2 step 5 {
    %0 = arith.constant 1 : i32
  }
  return
}

// ----------------------------------------------------------------------------
// Test 7: Nested loops with different steps
// Outer: 0 to 4 step 1 -> 4 iterations
// Inner: 1 to 7 step 3 -> ceil((7-1)/3)=ceil(6/3)=2 iterations
// CHECK-LABEL: func.func @nested_loops
// CHECK: affine.for %{{.*}} = 0 to 4 {
// CHECK:   affine.for %{{.*}} = 1 to 7 step 3 {
// CHECK:   } {trip_count = 2 : i64}
// CHECK: } {trip_count = 4 : i64}
func.func @nested_loops(%arg0: memref<10x10xi32>) {
  affine.for %i = 0 to 4 {
    affine.for %j = 1 to 7 step 3 {
      %0 = affine.load %arg0[%i, %j] : memref<10x10xi32>
    }
  }
  return
}

// ----------------------------------------------------------------------------
// Test 8: Dynamic upper bound -> no trip_count attribute
// CHECK-LABEL: func.func @dynamic_upper_bound
// CHECK: affine.for %{{.*}} = 0 to %{{.*}} {
// CHECK-NOT: trip_count
// CHECK: return
func.func @dynamic_upper_bound(%N: index) {
  affine.for %i = 0 to %N {
    %0 = arith.constant 1 : i32
  }
  return
}

// ----------------------------------------------------------------------------
// Test 9: Dynamic lower bound -> no trip_count
// CHECK-LABEL: func.func @dynamic_lower_bound
// CHECK: affine.for %{{.*}} = %{{.*}} to 10 {
// CHECK-NOT: trip_count
// CHECK: return
func.func @dynamic_lower_bound(%M: index) {
  affine.for %i = %M to 10 {
    %0 = arith.constant 1 : i32
  }
  return
}

// ----------------------------------------------------------------------------
// Test 10: Loop already has a trip_count attribute -> preserved unchanged
// CHECK-LABEL: func.func @existing_trip_count
// CHECK: affine.for %{{.*}} = 2 to 8 step 2 {
// CHECK: } {trip_count = 99 : i64}
func.func @existing_trip_count() {
  affine.for %i = 2 to 8 step 2 {
    %0 = arith.constant 1 : i32
  } {trip_count = 99 : i64}
  return
}

// ----------------------------------------------------------------------------
// Test 11: Loop with memref and load/store (realistic kernel)
// CHECK-LABEL: func.func @vector_add
// CHECK: affine.for %{{.*}} = 0 to 128 {
// CHECK: } {trip_count = 128 : i64}
func.func @vector_add(%A: memref<128xi32>, %B: memref<128xi32>, %C: memref<128xi32>) {
  affine.for %i = 0 to 128 {
    %a = affine.load %A[%i] : memref<128xi32>
    %b = affine.load %B[%i] : memref<128xi32>
    %c = arith.addi %a, %b : i32
    affine.store %c, %C[%i] : memref<128xi32>
  }
  return
}
