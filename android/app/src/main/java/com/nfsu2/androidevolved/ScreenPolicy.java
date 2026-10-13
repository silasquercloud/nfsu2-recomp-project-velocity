package com.nfsu2.androidevolved;
import android.content.pm.ActivityInfo;
/** Automatic respects Android's rotation lock and never requests portrait. */
final class ScreenPolicy {
    static int orientation(String mode){
        if("normal".equals(mode))return ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
        if("reverse".equals(mode))return ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE;
        return ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE;
    }
}
