package com.nfsu2.androidevolved;
import java.io.*;
import java.nio.file.*;
import java.util.zip.*;
public final class DxvkPackagePolicyCheck {
    static void check(boolean value){if(!value)throw new AssertionError("Package verification contract");}
    public static void main(String[] args)throws Exception {
        check(DxvkPackagePolicy.releaseUrl(DxvkPackagePolicy.RELEASE_PREFIX+"v1/app.apk"));
        for(String u:new String[]{"http://github.com/test.apk","https://github.com.evil.test/test.apk","https://evil.test/app.apk",DxvkPackagePolicy.RELEASE_PREFIX+"../other.apk",DxvkPackagePolicy.RELEASE_PREFIX+"v1/app.apk?redirect=elsewhere"})check(!DxvkPackagePolicy.releaseUrl(u));
        check(DxvkPackagePolicy.redirectUrl("https://release-assets.githubusercontent.com/path?token=download"));
        check(!DxvkPackagePolicy.redirectUrl("https://github.com@evil.test/x"));
        check(!DxvkPackagePolicy.redirectUrl("http://release-assets.githubusercontent.com/x"));
        File malformed=File.createTempFile("velocity-package-test",".apk");
        try{try(ZipOutputStream out=new ZipOutputStream(new FileOutputStream(malformed))){out.putNextEntry(new ZipEntry("lib/x86/libnfsu2_android.so"));out.write(new byte[64]);out.closeEntry();}
            boolean rejected=false;try{DxvkPackagePolicy.verifyLibraries(malformed);}catch(IOException e){rejected=true;}check(rejected);
            Files.write(malformed.toPath(),"abc".getBytes("US-ASCII"));check(DxvkPackagePolicy.digest(malformed).equals("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
        }finally{malformed.delete();}
        if(args.length!=1)throw new IllegalArgumentException("Provide a real verified ARM64 port APK");
        DxvkPackagePolicy.verifyLibraries(new File(args[0]));
        System.out.println("HTTPS origin/redirect policy, SHA-256, malformed ABI rejection, real ELF64 ARM64 dependency validation passed");
    }
}
