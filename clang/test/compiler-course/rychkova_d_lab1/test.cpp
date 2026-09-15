// RUN: %clang_cc1 -load %llvmshlibdir/noexcept.so -plugin noexcept_analyzer -fsyntax-only -fcxx-exceptions %s 2>&1 | FileCheck %s

// CHECK-DAG: FunctionDecl {{.*}} safeSimpleFunction 'int () noexcept'
int safeSimpleFunction() {
    return 42;
}

// CHECK-DAG: FunctionDecl {{.*}} calculatorFunction 'int () noexcept'
int calculatorFunction() {
    int result = 0;
    for (int i = 1; i <= 5; ++i) {
        result += i * 2;
    }
    return result;
}

// CHECK-DAG: FunctionDecl {{.*}} selectorFunction 'int (int) noexcept'
int selectorFunction(int value) {
    if (value > 0) {
        if (value > 100) return 100;
        return value;
    }
    return 0;
}

// CHECK-DAG: FunctionDecl {{.*}} processArray 'void (int *, int) noexcept'
void processArray(int* arr, int size) {
    for (int i = 0; i < size; ++i) {
        arr[i] = arr[i] * 2;
    }
}

// CHECK-DAG: FunctionDecl {{.*}} safeRecursive 'int (int) noexcept'
int safeRecursive(int n) {
    if (n <= 1) return 1;
    return n * safeRecursive(n - 1);
}

namespace Math {
    namespace Operations {
        // CHECK-DAG: FunctionDecl {{.*}} multiply 'int (int, int) noexcept'
        int multiply(int a, int b) { return a * b; }
    }
    
    // CHECK-DAG: FunctionDecl {{.*}} compute 'int (int, int) noexcept'
    int compute(int x, int y) { 
        return Operations::multiply(x, y) + x; 
    }
}

// CHECK-DAG: FunctionDecl {{.*}} chainCallA 'int () noexcept'
int chainCallA() { return 5; }

// CHECK-DAG: FunctionDecl {{.*}} chainCallB 'int () noexcept'
int chainCallB() { return chainCallA() + 3; }

// CHECK-DAG: FunctionDecl {{.*}} chainCallC 'int () noexcept'
int chainCallC() { return chainCallB() * 2; }

// CHECK-DAG: FunctionDecl {{.*}} main 'int () noexcept'
int main() {
    int data[10];
    processArray(data, 10);
    return Math::compute(5, 3) + safeRecursive(5);
}

// CHECK-NOT: FunctionDecl {{.*}} throwException 'int () noexcept'
int throwException() {
    throw 1;
    return 0;
}

// CHECK-NOT: FunctionDecl {{.*}} callThrower 'int () noexcept'
int callThrower() {
    return throwException();
}

// CHECK-NOT: FunctionDecl {{.*}} tryBlockFunction 'int () noexcept'
int tryBlockFunction() {
    try {
        return 42;
    } catch (...) {
        return 0;
    }
}

// CHECK-NOT: FunctionDecl {{.*}} allocateMemory 'int* () noexcept'
int* allocateMemory() {
    return new int[100];
}

// CHECK-NOT: FunctionDecl {{.*}} unsafeConstructor 'int () noexcept'
class UnsafeClass {
public:
    UnsafeClass() noexcept(false) {}
    ~UnsafeClass() noexcept(false) {}
};
int unsafeConstructor() {
    return throwException();
}

class SafeClass {
public:
    // CHECK-DAG: CXXMethodDecl {{.*}} trivialMethod 'void () noexcept'
    void trivialMethod() {}
    
    // CHECK-DAG: CXXMethodDecl {{.*}} constMethod 'int () const noexcept'
    int constMethod() const { return 42; }
};
