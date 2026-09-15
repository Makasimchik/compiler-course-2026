// RUN: mlir-opt %s -load-pass-plugin=%mlir_lib_dir/kondakov_v_affine_for_MLIR%shlibext --pass-pipeline="builtin.module(trip_count)" | FileCheck %s

func.func @test_simple() {
  // CHECK-LABEL: func.func @test_simple
  // CHECK: affine.for %{{.*}} = 0 to 10 step 2 {
  // CHECK-NEXT: "test.op"
  // CHECK-NEXT: } {trip_count = 5 : index}
  affine.for %i = 0 to 10 step 2 {
    "test.op"() : () -> ()
  }
  return
}

func.func @test_ceil_rounding() {
  // CHECK-LABEL: func.func @test_ceil_rounding
  // CHECK: affine.for %{{.*}} = 0 to 10 step 3 {
  // CHECK-NEXT: "test.op"
  // CHECK-NEXT: } {trip_count = 4 : index}
  affine.for %i = 0 to 10 step 3 {
    "test.op"() : () -> ()
  }
  return
}

func.func @test_empty() {
  // CHECK-LABEL: func.func @test_empty
  // CHECK: affine.for %{{.*}} = 5 to 5 {
  // CHECK-NEXT: "test.op"
  // CHECK-NEXT: } {trip_count = 0 : index}
  affine.for %i = 5 to 5 step 1 {
    "test.op"() : () -> ()
  }
  return
}

func.func @test_negative() {
  // CHECK-LABEL: func.func @test_negative
  // CHECK: affine.for %{{.*}} = -10 to -5 {
  // CHECK-NEXT: "test.op"
  // CHECK-NEXT: } {trip_count = 5 : index}
  affine.for %i = -10 to -5 step 1 {
    "test.op"() : () -> ()
  }
  return
}

func.func @test_symbolic_upper(%N: index) {
  // CHECK-LABEL: func.func @test_symbolic_upper
  // CHECK-NOT: trip_count
  affine.for %i = 0 to %N step 1 {
    "test.op"() : () -> ()
  }
  return
}

func.func @test_symbolic_lower(%start: index) {
  // CHECK-LABEL: func.func @test_symbolic_lower
  // CHECK-NOT: trip_count
  affine.for %i = %start to 100 step 1 {
    "test.op"() : () -> ()
  }
  return
}

func.func @test_both_symbolic(%lo: index, %hi: index) {
  // CHECK-LABEL: func.func @test_both_symbolic
  // CHECK-NOT: trip_count
  affine.for %i = %lo to %hi step 2 {
    "test.op"() : () -> ()
  }
  return
}

func.func @test_large_step() {
  // CHECK-LABEL: func.func @test_large_step
  // CHECK: affine.for %{{.*}} = 0 to 100 step 50 {
  // CHECK-NEXT: "test.op"
  // CHECK-NEXT: } {trip_count = 2 : index}
  affine.for %i = 0 to 100 step 50 {
    "test.op"() : () -> ()
  }
  return
}

func.func @test_step_larger_than_dist() {
  // CHECK-LABEL: func.func @test_step_larger_than_dist
  // CHECK: affine.for %{{.*}} = 0 to 5 step 10 {
  // CHECK-NEXT: "test.op"
  // CHECK-NEXT: } {trip_count = 1 : index}
  affine.for %i = 0 to 5 step 10 {
    "test.op"() : () -> ()
  }
  return
}

func.func @test_nested() {
  // CHECK-LABEL: func.func @test_nested
  // CHECK: affine.for %{{.*}} = 0 to 3 {
  affine.for %i = 0 to 3 step 1 {
    // CHECK: affine.for %{{.*}} = 0 to 4 step 2 {
    affine.for %j = 0 to 4 step 2 {
      // CHECK: "test.op"
      "test.op"() : () -> ()
    }
    // CHECK: } {trip_count = 2 : index}
  }
  // CHECK: } {trip_count = 3 : index}
  return
}

func.func @test_mixed(%N: index) {
  // CHECK-LABEL: func.func @test_mixed
  // CHECK: affine.for %{{.*}} = 0 to 10 {
  affine.for %i = 0 to 10 step 1 {
    "test.op"() : () -> ()
  }
  // CHECK: } {trip_count = 10 : index}
  // CHECK-NOT: trip_count
  affine.for %j = 0 to %N step 1 {
    "test.op"() : () -> ()
  }
  return
}

func.func @test_reverse_bounds() {
  // CHECK-LABEL: func.func @test_reverse_bounds
  // CHECK: affine.for %{{.*}} = 10 to 5 {
  affine.for %i = 10 to 5 step 1 {
    "test.op"() : () -> ()
  }
  // CHECK: } {trip_count = 0 : index}
  return
}

func.func @test_single_iter() {
  // CHECK-LABEL: func.func @test_single_iter
  // CHECK: affine.for %{{.*}} = 0 to 1 {
  affine.for %i = 0 to 1 step 1 {
    "test.op"() : () -> ()
  }
  // CHECK: } {trip_count = 1 : index}
  return
}
