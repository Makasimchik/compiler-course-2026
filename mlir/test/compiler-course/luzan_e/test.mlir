 // RUN: mlir-opt --load-pass-plugin=%mlir_lib_dir/luzan_e_depth_MLIR%shlibext --pass-pipeline="builtin.module(func.func(luzanemaxdepth))" %s | FileCheck %s

// CHECK: func.func @f{{.*}}max_block_depth = 0{{.*}}
func.func @f(%i: i32) -> i32 {
  %result = arith.muli %i, %i : i32
  func.return %result : i32
}

// CHECK:   func.func @simple(){{.*}}max_block_depth = 1{{.*}}
func.func @simple() {
  %a    = arith.constant 10 : i32
  %five = arith.constant 5  : i32
  %cond = arith.cmpi sgt, %a, %five : i32
  scf.if %cond {
    %0 = func.call @f(%a) : (i32) -> i32
  }
  func.return
}

// CHECK: func.func @ifelse{{.*}}max_block_depth = 1{{.*}}
func.func @ifelse() {
  %b    = arith.constant 3 : i32
  %five = arith.constant 5 : i32
  %two  = arith.constant 2 : i32
  %cond = arith.cmpi sgt, %b, %five : i32
  scf.if %cond {
    %0 = func.call @f(%b) : (i32) -> i32
  } else {
    %b2 = arith.muli %b, %two : i32
    %0  = func.call @f(%b2) : (i32) -> i32
  }
  func.return
}

// CHECK: func.func @nested_ifelse{{.*}}max_block_depth = 2{{.*}}
func.func @nested_ifelse() {
  %c    = arith.constant 0 : i32
  %zero = arith.constant 0 : i32
  %cond1 = arith.cmpi sgt, %c, %zero : i32
  scf.if %cond1 {
    %0 = func.call @f(%c) : (i32) -> i32
  } else {
    %cond2 = arith.cmpi slt, %c, %zero : i32
    scf.if %cond2 {
      %0 = func.call @f(%c) : (i32) -> i32
    } else {
      %0 = func.call @f(%c) : (i32) -> i32
    }
  }
  func.return
}

// CHECK: func.func @simple_for{{.*}}max_block_depth = 1{{.*}}
func.func @simple_for() {
  %lb   = arith.constant 1 : index
  %ub   = arith.constant 4 : index
  %step = arith.constant 1 : index
  scf.for %i = %lb to %ub step %step {
    %i_i32 = arith.index_cast %i : index to i32
    %0 = func.call @f(%i_i32) : (i32) -> i32
  }
  func.return
}

// CHECK: func.func @nested_for{{.*}}max_block_depth = 2{{.*}}
func.func @nested_for() {
  %lb   = arith.constant 1 : index
  %ub   = arith.constant 4 : index
  %step = arith.constant 1 : index
  scf.for %i = %lb to %ub step %step {
    scf.for %j = %lb to %ub step %step {
      %i_i32 = arith.index_cast %i : index to i32
      %0 = func.call @f(%i_i32) : (i32) -> i32
    }
  }
  func.return
}

// CHECK: func.func @simple_while{{.*}}max_block_depth = 1{{.*}}
func.func @simple_while() {
  %d_init = arith.constant 3 : i32
  %zero   = arith.constant 0 : i32
  scf.while (%d = %d_init) : (i32) -> (i32) {
    %cond = arith.cmpi sgt, %d, %zero : i32
    scf.condition(%cond) %d : i32
  } do {
    ^bb0(%d_body : i32):
    %0 = func.call @f(%d_body) : (i32) -> i32
    scf.yield %d_body : i32
  }
  func.return
}

// CHECK: func.func @if_in_loop{{.*}}max_block_depth = 2{{.*}}
func.func @if_in_loop() {
  %lb      = arith.constant 1 : index
  %ub      = arith.constant 6 : index
  %step    = arith.constant 1 : index
  %two     = arith.constant 2 : i32
  %zero_i32 = arith.constant 0 : i32
  scf.for %i = %lb to %ub step %step {
    %i_i32 = arith.index_cast %i : index to i32
    %rem   = arith.remsi %i_i32, %two : i32
    %cond  = arith.cmpi eq, %rem, %zero_i32 : i32
    scf.if %cond {
      %0 = func.call @f(%i_i32) : (i32) -> i32
    }
  }
  func.return
}

// CHECK: func.func @nested3_for{{.*}}max_block_depth = 3{{.*}}
func.func @nested3_for() {
  %lb   = arith.constant 1 : index
  %ub   = arith.constant 4 : index
  %step = arith.constant 1 : index
  scf.for %i = %lb to %ub step %step {
    scf.for %j = %lb to %ub step %step {
      scf.for %k = %lb to %ub step %step {
        %i_i32 = arith.index_cast %i : index to i32
        %0 = func.call @f(%i_i32) : (i32) -> i32
      }
    }
  }
  func.return
}

// CHECK: func.func @affine_simple_for{{.*}}max_block_depth = 1{{.*}}
func.func @affine_simple_for() {
  affine.for %i = 0 to 10 {
    %i_i32 = arith.index_cast %i : index to i32
    %0 = func.call @f(%i_i32) : (i32) -> i32
  }
  func.return
}

// CHECK: func.func @affine_nested_for{{.*}}max_block_depth = 2{{.*}}
func.func @affine_nested_for() {
  affine.for %i = 0 to 10 {
    affine.for %j = 0 to 10 {
      %i_i32 = arith.index_cast %i : index to i32
      %0 = func.call @f(%i_i32) : (i32) -> i32
    }
  }
  func.return
}

// CHECK: func.func @affine_simple_if{{.*}}max_block_depth = 1{{.*}}
func.func @affine_simple_if(%arg0: index) {
  affine.if affine_set<(d0) : (d0 >= 0)>(%arg0) {
    %arg0_i32 = arith.index_cast %arg0 : index to i32
    %0 = func.call @f(%arg0_i32) : (i32) -> i32
  } else {
    %arg0_i32 = arith.index_cast %arg0 : index to i32
    %0 = func.call @f(%arg0_i32) : (i32) -> i32
  }
  func.return
}
