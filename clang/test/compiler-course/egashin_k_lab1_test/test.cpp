// RUN: %clang_cc1 -load %llvmshlibdir/egashin_k_lab1_ClangAST%pluginext -plugin egashin_k_const_plugin -fsyntax-only %s 2>&1 | FileCheck %s

void mutate_value(int *value);
void inspect_value(const int *value);
void mutate_ref(int &value);
void mutate_pointer_ref(int *&value);

// CHECK-LABEL: void readonly_pointer{{[(][)]}}
// CHECK: const int{{ *\* *}}const ptr {{=}} &value;
void readonly_pointer() {
  int value = 10;
  int* ptr = &value;
  int read = *ptr;
  (void)read;
}

// CHECK-LABEL: void pointer_moves{{[(][)]}}
// CHECK: const int{{ *\* *}}ptr {{=}} values;
void pointer_moves() {
  int values[2] = {1, 2};
  int* ptr = values;
  ++ptr;
}

// CHECK-LABEL: void pointee_changes{{[(][)]}}
// CHECK: int{{ *\* *}}const ptr {{=}} &value;
void pointee_changes() {
  int value = 10;
  int* ptr = &value;
  *ptr = 30;
}

// CHECK-LABEL: void pointer_and_pointee_change{{[(][)]}}
// CHECK: int{{ *\* *}}ptr {{=}} &first;
void pointer_and_pointee_change() {
  int first = 1;
  int second = 2;
  int* ptr = &first;
  *ptr = 3;
  ptr = &second;
}

// CHECK-LABEL: void readonly_reference{{[(][)]}}
// CHECK: const int{{ *& *}}ref {{=}} value;
void readonly_reference() {
  int value = 42;
  int& ref = value;
  int copy = ref;
  (void)copy;
}

// CHECK-LABEL: void changed_reference{{[(][)]}}
// CHECK: int{{ *& *}}ref {{=}} value;
void changed_reference() {
  int value = 42;
  int& ref = value;
  ++ref;
}

// CHECK-LABEL: void readonly_parameters{{[(]}}
// CHECK: void readonly_parameters(const int{{ *\* *}}const ptr, const int{{ *& *}}ref)
void readonly_parameters(int* ptr, int& ref) {
  int sum = *ptr + ref;
  (void)sum;
}

// CHECK-LABEL: void mixed_parameters{{[(]}}
// CHECK: void mixed_parameters(int{{ *\* *}}const out, const int{{ *\* *}}moving, int{{ *& *}}ref)
void mixed_parameters(int* out, int* moving, int& ref) {
  *out = ref;
  ++moving;
  ref = 5;
}

struct Box {
  void bump() { ++value; }
  int peek() const { return value; }
  int value;
};

// CHECK-LABEL: void member_write{{[(][)]}}
// CHECK: Box{{ *\* *}}const ptr {{=}} &box;
void member_write() {
  Box box{0};
  Box* ptr = &box;
  ptr->value = 7;
}

// CHECK-LABEL: void member_call_pointer{{[(][)]}}
// CHECK: Box{{ *\* *}}const ptr {{=}} &box;
void member_call_pointer() {
  Box box{0};
  Box* ptr = &box;
  ptr->bump();
}

// CHECK-LABEL: void member_call_reference{{[(][)]}}
// CHECK: Box{{ *& *}}ref {{=}} box;
void member_call_reference() {
  Box box{0};
  Box& ref = box;
  ref.bump();
}

// CHECK-LABEL: void const_member_call_pointer{{[(][)]}}
// CHECK: const Box{{ *\* *}}const ptr {{=}} &box;
void const_member_call_pointer() {
  Box box{0};
  Box* ptr = &box;
  int read = ptr->peek();
  (void)read;
}

// CHECK-LABEL: void const_member_call_reference{{[(][)]}}
// CHECK: const Box{{ *& *}}ref {{=}} box;
void const_member_call_reference() {
  Box box{0};
  Box& ref = box;
  int read = ref.peek();
  (void)read;
}

// CHECK-LABEL: void array_write{{[(][)]}}
// CHECK: int{{ *\* *}}const ptr {{=}} values;
void array_write() {
  int values[3] = {1, 2, 3};
  int* ptr = values;
  ptr[1] = 9;
}

// CHECK-LABEL: void call_arguments{{[(]}}
// CHECK: void call_arguments(int{{ *\* *}}const mutable_ptr, const int{{ *\* *}}const readonly_ptr, int{{ *& *}}ref)
void call_arguments(int* mutable_ptr, int* readonly_ptr, int& ref) {
  mutate_value(mutable_ptr);
  inspect_value(readonly_ptr);
  mutate_ref(ref);
}

// CHECK-LABEL: void call_pointer_reference{{[(][)]}}
// CHECK: int{{ *\* *}}ptr {{=}} &value;
void call_pointer_reference() {
  int value = 8;
  int* ptr = &value;
  mutate_pointer_ref(ptr);
}

// CHECK-LABEL: void double_pointer_read{{[(][)]}}
// CHECK: int{{ *\* *}}const{{ *\* *}}const both {{=}} &inner;
void double_pointer_read() {
  int value = 4;
  int* inner = &value;
  int** both = &inner;
  int read = **both;
  (void)read;
}

// CHECK-LABEL: void multi_declarator{{[(][)]}}
// CHECK: int{{ *\* *}}first {{=}} &value, {{\*}}second {{=}} &value;
void multi_declarator() {
  int value = 4;
  int* first = &value, *second = &value;
  *second = 7;
  int read = *first;
  (void)read;
}

// CHECK-LABEL: void const_pointee_pointer{{[(][)]}}
// CHECK: const int{{ *\* *}}const ptr {{=}} &value;
void const_pointee_pointer() {
  int value = 9;
  const int* ptr = &value;
  int read = *ptr;
  (void)read;
}

// CHECK-LABEL: void already_const{{[(]}}
// CHECK: void already_const(const int{{ *\* *}}const ptr, const int{{ *& *}}ref)
void already_const(const int* const ptr, const int& ref) {
  int read = *ptr + ref;
  (void)read;
}
