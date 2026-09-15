// RUN: mlir-opt %s -load-pass-plugin=%mlir_lib_dir/nikitin_a_lab4_MLIR%shlibext --pass-pipeline="builtin.module(nikitin_a_lab4_MLIR)" | FileCheck %s

// Test 1: Basic loop with constant bounds and step 1
func.func @test_basic_loop_step1() {
  // CHECK-LABEL: func.func @test_basic_loop_step1
  // CHECK: affine.for %{{.*}} = 0 to 10 {
  // CHECK-NEXT: } {trip_count = 10 : i64}
  affine.for %i = 0 to 10 {
    affine.yield
  }
  return
}

// Test 2: Loop with step > 1
func.func @test_loop_step2() {
  // CHECK-LABEL: func.func @test_loop_step2
  // CHECK: affine.for %{{.*}} = 0 to 10 step 2 {
  // CHECK-NEXT: } {trip_count = 5 : i64}
  affine.for %i = 0 to 10 step 2 {
    affine.yield
  }
  return
}

// Test 3: Loop with non-zero lower bound
func.func @test_nonzero_lower_bound() {
  // CHECK-LABEL: func.func @test_nonzero_lower_bound
  // CHECK: affine.for %{{.*}} = 5 to 20 {
  // CHECK-NEXT: } {trip_count = 15 : i64}
  affine.for %i = 5 to 20 {
    affine.yield
  }
  return
}

// Test 4: Loop with zero iterations
func.func @test_zero_iterations() {
  // CHECK-LABEL: func.func @test_zero_iterations
  // CHECK: affine.for %{{.*}} = 5 to 5 {
  // CHECK-NEXT: } {trip_count = 0 : i64}
  affine.for %i = 5 to 5 {
    affine.yield
  }
  return
}

// Test 5: Loop with negative range
func.func @test_negative_range() {
  // CHECK-LABEL: func.func @test_negative_range
  // CHECK: affine.for %{{.*}} = 10 to 5 {
  // CHECK-NEXT: } {trip_count = 0 : i64}
  affine.for %i = 10 to 5 {
    affine.yield
  }
  return
}

// Test 6: Loop with uneven step
func.func @test_uneven_step() {
  // CHECK-LABEL: func.func @test_uneven_step
  // CHECK: affine.for %{{.*}} = 0 to 10 step 3 {
  // CHECK-NEXT: } {trip_count = 4 : i64}
  affine.for %i = 0 to 10 step 3 {
    affine.yield
  }
  return
}

// Test 7: Loop with step greater than range
func.func @test_step_greater_than_range() {
  // CHECK-LABEL: func.func @test_step_greater_than_range
  // CHECK: affine.for %{{.*}} = 0 to 10 step 20 {
  // CHECK-NEXT: } {trip_count = 1 : i64}
  affine.for %i = 0 to 10 step 20 {
    affine.yield
  }
  return
}

// Test 8: Dynamic lower bound (no attribute)
func.func @test_dynamic_lower_bound(%arg0: index) {
  // CHECK-LABEL: func.func @test_dynamic_lower_bound
  // CHECK: affine.for %{{.*}} = %arg0 to 10 {
  // CHECK-NEXT: }
  // CHECK-NOT: trip_count
  affine.for %i = %arg0 to 10 {
    affine.yield
  }
  return
}

// Test 9: Dynamic upper bound (no attribute)
func.func @test_dynamic_upper_bound(%arg0: index) {
  // CHECK-LABEL: func.func @test_dynamic_upper_bound
  // CHECK: affine.for %{{.*}} = 0 to %arg0 {
  // CHECK-NEXT: }
  // CHECK-NOT: trip_count
  affine.for %i = 0 to %arg0 {
    affine.yield
  }
  return
}

// Test 10: Nested loops
func.func @test_nested_loops() {
  // CHECK-LABEL: func.func @test_nested_loops
  // CHECK: affine.for %{{.*}} = 0 to 5 {
  // CHECK-NEXT: affine.for %{{.*}} = 0 to 3 {
  // CHECK-NEXT: } {trip_count = 3 : i64}
  // CHECK-NEXT: } {trip_count = 5 : i64}
  affine.for %i = 0 to 5 {
    affine.for %j = 0 to 3 {
      affine.yield
    }
    affine.yield
  }
  return
}