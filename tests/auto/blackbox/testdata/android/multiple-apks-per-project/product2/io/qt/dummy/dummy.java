package io.qt.dummy;

import android.app.Activity;

public class Dummy extends Activity
{
    static {
        System.loadLibrary("lib1");
        System.loadLibrary("lib");
    }
}
