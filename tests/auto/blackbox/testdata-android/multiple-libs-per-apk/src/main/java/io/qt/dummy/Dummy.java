package io.qt.dummy;

import android.app.Activity;
import io.qbs.lib3.lib3;

public class Dummy extends Activity
{
    static {
        System.loadLibrary("lib1");
        System.loadLibrary("lib");
        lib3.foo();
    }
}
