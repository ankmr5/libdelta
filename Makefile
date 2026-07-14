build:
    clang -lentropy -lcontinuityvm_ffi src/libdelta.c -o dist/libdelta.so -shared -fPIC

all: build
