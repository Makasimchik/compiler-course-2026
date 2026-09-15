 // RUN: mlir-opt --load-pass-plugin=%mlir_lib_dir/romanov_a_max_depth_MLIR%shlibext --pass-pipeline="builtin.module(func.func(MaxDepthCountPass))" %s | FileCheck %s

// CHECK: func.func @dec{{.*}}max_depth = 0{{.*}}
func.func @dec(%i: i32) -> i32 {
  %c1 = arith.constant 1 : i32
  %result = arith.subi %i, %c1 : i32
  func.return %result : i32
}

// CHECK: func.func @one_if{{.*}}max_depth = 1{{.*}}
func.func @one_if() -> i32 {
  %b = arith.constant 5 : i32
  %a = arith.constant true
  %result = scf.if %a -> i32 {
    %c2 = arith.constant 2 : i32
    %doubled = arith.muli %b, %c2 : i32
    scf.yield %doubled : i32
  } else {
    scf.yield %b : i32
  }

  func.return %result : i32
}

// CHECK: func.func @two_ifs(){{.*}}max_depth = 1{{.*}}
func.func @two_ifs() {
  %b = arith.constant 5 : i32
  %a = arith.constant true
  %cond1 = arith.cmpi eq, %a, %a : i1
  scf.if %cond1 {
    %c2 = arith.constant 2 : i32
    %doubled = arith.muli %b, %c2 : i32
  }

  %cond2 = arith.cmpi eq, %a, %a : i1
  scf.if %cond2 {
    %c3 = arith.constant 3 : i32
    %tripled = arith.muli %b, %c3 : i32
  }

  func.return
}

// CHECK: func.func @nested_ifs(){{.*}}max_depth = 2{{.*}}
func.func @nested_ifs() -> i32 {
  %b = arith.constant 5 : i32
  %a = arith.constant true
  %new_b = scf.if %a -> i32 {
    %c2 = arith.constant 2 : i32
    %doubled = arith.muli %b, %c2 : i32
    %inner = scf.if %a -> i32 {
      %c3 = arith.constant 3 : i32
      %tripled = arith.muli %doubled, %c3 : i32
      scf.yield %tripled : i32
    } else {
      scf.yield %doubled : i32
    }
    scf.yield %inner : i32
  } else {
    scf.yield %b : i32
  }
  func.return %new_b : i32
}

// CHECK: func.func @sum_range{{.*}}max_depth = 1{{.*}}
func.func @sum_range(%first: i32, %last: i32) -> i32 {
  %c0 = arith.constant 0 : i32
  %c1 = arith.constant 1 : i32
  %step = arith.constant 1 : index

  %first_idx = arith.index_cast %first : i32 to index
  %last_idx = arith.index_cast %last : i32 to index
  %last_plus_1 = arith.addi %last_idx, %step : index

  %sum = scf.for %i = %first_idx to %last_plus_1 step %step iter_args(%acc = %c0) -> i32 {
    %i_i32 = arith.index_cast %i : index to i32
    %new_acc = arith.addi %acc, %i_i32 : i32
    scf.yield %new_acc : i32
  }

  func.return %sum : i32
}

// CHECK: func.func @nested_for{{.*}}max_depth = 2{{.*}}
func.func @nested_for(%l: i32, %r: i32) {
  %step = arith.constant 1 : index
  %l_idx = arith.index_cast %l : i32 to index
  %r_idx = arith.index_cast %r : i32 to index
  %r_plus_1 = arith.addi %r_idx, %step : index

  scf.for %i = %l_idx to %r_plus_1 step %step {
    scf.for %j = %l_idx to %r_plus_1 step %step {
      %i_i32 = arith.index_cast %i : index to i32
      %j_i32 = arith.index_cast %j : index to i32
      %sum = arith.addi %i_i32, %j_i32 : i32
    }
  }

  func.return
}

// CHECK: func.func @while_loop{{.*}}max_depth = 1{{.*}}
func.func @while_loop(%n: i32) -> i32 {
  %c0 = arith.constant 0 : i32
  %c2 = arith.constant 2 : i32

  %result = scf.while (%x = %n) : (i32) -> i32 {
    %cond = arith.cmpi sgt, %x, %c0 : i32
    scf.condition(%cond) %x : i32
  } do {
  ^bb0(%x: i32):
    %next = arith.divsi %x, %c2 : i32
    scf.yield %next : i32
  }
  func.return %result : i32
}

// CHECK: func.func @affine_for{{.*}}max_depth = 1{{.*}}
func.func @affine_for() {
  affine.for %i = 0 to 10 {
    %i_i32 = arith.index_cast %i : index to i32
    %doubled = arith.addi %i_i32, %i_i32 : i32
  }
  func.return
}

// CHECK: func.func @nested_affine{{.*}}max_depth = 2{{.*}}
func.func @nested_affine() {
  affine.for %i = 0 to 10 {
    affine.for %j = 0 to 10 {
      %i_i32 = arith.index_cast %i : index to i32
      %j_i32 = arith.index_cast %j : index to i32
      %sum = arith.addi %i_i32, %j_i32 : i32
    }
  }
  func.return
}

// CHECK: func.func @depth_three{{.*}}max_depth = 3{{.*}}
func.func @depth_three(%cond: i1) {
  %lb = arith.constant 0 : index
  %ub = arith.constant 10 : index
  %step = arith.constant 1 : index

  scf.for %i = %lb to %ub step %step {
    scf.if %cond {
      scf.for %j = %lb to %ub step %step {
        %j_i32 = arith.index_cast %j : index to i32
        %doubled = arith.addi %j_i32, %j_i32 : i32
      }
    }
  }
  func.return
}

// CHECK: func.func @affine_nested{{.*}}max_depth = 2{{.*}}
func.func @affine_nested(%n: index) {
  affine.if affine_set<()[s0] : (s0 - 5 >= 0)>()[%n] {
    affine.for %j = 0 to %n {
      %j_i32 = arith.index_cast %j : index to i32
      %doubled = arith.addi %j_i32, %j_i32 : i32
    }
  }
  func.return
}

// CHECK: func.func @transparent_region{{.*}}max_depth = 2{{.*}}
func.func @transparent_region(%cond: i1) -> i32 {
  %result = scf.if %cond -> i32 {
    %inner = scf.execute_region -> i32 {
      %lb = arith.constant 0 : index
      %ub = arith.constant 10 : index
      %step = arith.constant 1 : index
      %c0 = arith.constant 0 : i32
      %sum = scf.for %i = %lb to %ub step %step iter_args(%acc = %c0) -> i32 {
        %i_i32 = arith.index_cast %i : index to i32
        %new_acc = arith.addi %acc, %i_i32 : i32
        scf.yield %new_acc : i32
      }
      scf.yield %sum : i32
    }
    scf.yield %inner : i32
  } else {
    %zero = arith.constant 0 : i32
    scf.yield %zero : i32
  }
  func.return %result : i32
}