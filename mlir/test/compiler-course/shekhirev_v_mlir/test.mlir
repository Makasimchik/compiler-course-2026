// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/shekhirev_v_mlir_MLIR.so \
// RUN:   --pass-pipeline="builtin.module(shekhirev_v_max_depth_MLIR)" %s | FileCheck %s

// CHECK-LABEL: func.func @test_flat_code
// CHECK-SAME:  max_block_depth = 0 : i64
func.func @test_flat_code(%a: i32, %b: i32) -> i32 {
  %res = arith.muli %a, %b : i32
  return %res : i32
}

// CHECK-LABEL: func.func @test_single_loop
// CHECK-SAME:  max_block_depth = 1 : i64
func.func @test_single_loop(%start: index, %end: index, %step: index) {
  scf.for %idx = %start to %end step %step {
  }
  return
}

// CHECK-LABEL: func.func @test_siblings
// CHECK-SAME:  max_block_depth = 1 : i64
func.func @test_siblings(%start: index, %end: index, %step: index) {
  scf.for %i = %start to %end step %step {}
  scf.for %j = %start to %end step %step {}
  return
}

// CHECK-LABEL: func.func @test_nested_loops
// CHECK-SAME:  max_block_depth = 2 : i64
func.func @test_nested_loops(%start: index, %end: index, %step: index) {
  scf.for %i = %start to %end step %step {
    scf.for %j = %start to %end step %step {
    }
  }
  return
}

// CHECK-LABEL: func.func @test_asymmetric_regions
// CHECK-SAME:  max_block_depth = 2 : i64
func.func @test_asymmetric_regions(%flag: i1, %start: index, %end: index, %step: index) {
  scf.if %flag {
  } else {
    scf.for %i = %start to %end step %step {
    }
  }
  return
}

// CHECK-LABEL: func.func @test_while_op
// CHECK-SAME:  max_block_depth = 2 : i64
func.func @test_while_op(%start_val: i32, %max_val: i32, %flag: i1) {
  %res = scf.while (%iter = %start_val) : (i32) -> i32 {
    %check = arith.cmpi slt, %iter, %max_val : i32
    scf.condition(%check) %iter : i32
  } do {
  ^bb0(%iter: i32):
    scf.if %flag {}
    scf.yield %iter : i32
  }
  return
}

// CHECK-LABEL: func.func @test_mixed_dialects
// CHECK-SAME:  max_block_depth = 3 : i64
func.func @test_mixed_dialects(%start: index, %end: index, %step: index) {
  affine.for %i = 0 to 10 {
    scf.for %j = %start to %end step %step {
      affine.if affine_set<(d0) : (d0 - 2 >= 0)>(%i) {
      }
    }
  }
  return
}