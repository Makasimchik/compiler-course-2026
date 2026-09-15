// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/rozenberg_a_lab4_MLIR%shlibext --pass-pipeline="builtin.module(TripCountAnnotationPass_MLIR)" %s | FileCheck %s

module {
  // CHECK-LABEL: static_loop
  // CHECK: {trip_count = 100 : i64}

  func.func @static_loop(%arg0: i32) {
    %c0_i32 = arith.constant 0 : i32
    %0 = affine.for %arg1 = 0 to 100 iter_args(%arg2 = %arg0) -> (i32) {
      %1 = arith.index_cast %arg1 : index to i32
      %2 = arith.addi %arg2, %1 : i32
      affine.yield %2 : i32
    }
    return
  }
  func.func @step_loop(%arg0: i32) {
    // CHECK-LABEL: step_loop
    // CHECK: {trip_count = 50 : i64}

    %0 = affine.for %arg1 = 0 to 100 step 2 iter_args(%arg2 = %arg0) -> (i32) {
      %1 = arith.index_cast %arg1 : index to i32
      %2 = arith.addi %arg2, %1 : i32
      affine.yield %2 : i32
    }
    return
  }
  func.func @dynamic_loop(%arg0: i32, %arg1: index) {
    // CHECK-LABEL: dynamic_loop
    // CHECK-NOT: trip_count

    %0 = affine.for %arg2 = 0 to %arg1 iter_args(%arg3 = %arg0) -> (i32) {
      %1 = arith.index_cast %arg2 : index to i32
      %2 = arith.addi %arg3, %1 : i32
      affine.yield %2 : i32
    }
    return
  }
  func.func @nested_loops(%arg0: i32) {
    // CHECK-LABEL: nested_loops
    // CHECK: {trip_count = 20 : i64}
    // CHECK: {trip_count = 10 : i64}

    %0 = affine.for %arg1 = 0 to 10 iter_args(%arg2 = %arg0) -> (i32) {
      %1 = affine.for %arg3 = 0 to 20 iter_args(%arg4 = %arg2) -> (i32) {
        %2 = arith.index_cast %arg1 : index to i32
        %3 = arith.index_cast %arg3 : index to i32
        %4 = arith.muli %2, %3 : i32
        %5 = arith.addi %arg4, %4 : i32
        affine.yield %5 : i32
      }
      affine.yield %1 : i32
    }
    return
  }
}
