package io.qt.qbs;

public class HelloWorld {
    public static void main(String[] args) {
        System.out.println("Tach.");
    }

    public class Internal {
        public native void something();
    }

    public class Other {
        public final int countOfThings = 0;
    }
}
