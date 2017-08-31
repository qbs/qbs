package io.qt.dummy1;

import android.app.Activity;
import android.os.Build;
import java.util.Arrays;
import java.util.List;

public class Dummy extends Activity
{
    static {
        List<String> abis = Arrays.asList(Build.SUPPORTED_ABIS);
        if (abis.contains("x86") || abis.contains("mips")) {
            System.loadLibrary("p1lib1");
        }
        if (abis.contains("armeabi")) {
            System.loadLibrary("p1lib2");
        }
    }
}
