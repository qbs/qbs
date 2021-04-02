package org.qbs.example;

import org.qtproject.qt.android.bindings.QtActivity;
import android.os.*;
import android.content.*;
import android.app.*;
import android.util.Log;

import java.lang.String;
import android.content.Intent;

import org.qbs.example.*;


public class TestQt6 extends QtActivity
{
    public static native void testFunc(String test);

    @Override
    public void onCreate(Bundle savedInstanceState) {
      super.onCreate(savedInstanceState);
          Log.d("qbs", "onCreate Test");
          Intent theIntent = getIntent();
          if (theIntent != null) {
              String theAction = theIntent.getAction();
              if (theAction != null) {
                  Log.d("qbs onCreate ", theAction);
              }
          }
    }

    @Override
    public void onDestroy() {
        Log.d("qbs", "onDestroy");
        System.exit(0);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        Log.d("qbs onActivityResult", "requestCode: "+requestCode);
        if (resultCode == RESULT_OK) {
            Log.d("qbs onActivityResult - resultCode: ", "SUCCESS");
        } else {
            Log.d("qbs onActivityResult - resultCode: ", "CANCEL");
        }
    }

    @Override
    public void onNewIntent(Intent intent) {
      Log.d("qbs", "onNewIntent");
      super.onNewIntent(intent);
      setIntent(intent);
    }
}
