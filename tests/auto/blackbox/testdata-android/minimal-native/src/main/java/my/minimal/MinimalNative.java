package minimalnative;

import android.app.Activity;
import android.widget.TextView;
import android.os.Bundle;

public class MinimalNative extends Activity
{
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        TextView  tv = new TextView(this);
        tv.setText(stringFromNative());
        setContentView(tv);
    }

    public native String stringFromNative();

    static {
        System.loadLibrary("minimal");
    }
}
