package com.nfsu2.androidevolved;
public final class ScreenPolicyCheck {
    public static void main(String[] args){
        if(ScreenPolicy.orientation("auto")!=11)throw new AssertionError("Must respect user rotation lock");
        if(ScreenPolicy.orientation("normal")!=0)throw new AssertionError("Normal landscape");
        if(ScreenPolicy.orientation("reverse")!=8)throw new AssertionError("Reverse landscape");
        if(ScreenPolicy.orientation("invalid")!=11||ScreenPolicy.orientation(null)!=11)throw new AssertionError("Safe landscape fallback");
        System.out.println("Android orientation policy passed: user-landscape automatic, fixed normal/reverse, safe fallback; no physical rotation claimed");
    }
}
