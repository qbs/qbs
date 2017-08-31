package io.qt.dummy2;

import android.app.Activity;
import android.os.Build;
import java.util.Arrays;
import java.util.List;

public class Dummy extends Activity
{
    static {
        List<String> abis = Arrays.asList(Build.SUPPORTED_ABIS);
        if (abis.contains("armeabi")) {
            System.loadLibrary("p2lib1");
            System.loadLibrary("p2lib2");
        }
    }
}
