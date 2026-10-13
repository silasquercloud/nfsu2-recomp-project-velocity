package com.nfsu2.androidevolved;

import android.app.*;
import android.content.*;
import android.content.pm.*;
import android.net.Uri;
import android.provider.Settings;
import android.widget.*;
import org.json.*;
import java.io.*;
import java.net.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.atomic.AtomicBoolean;

/** Static DXVK requires a matched whole-port APK, not a Windows DLL download. */
final class DxvkManager {
    static final String CATALOG="https://raw.githubusercontent.com/silasquercloud/nfsu2-recomp-project-velocity/feature/project-velocity-arm64/releases/dxvk-catalog-v1.json";
    static final AtomicBoolean downloading=new AtomicBoolean(false);
    final Activity activity;
    final Runnable changed;
    DxvkManager(Activity a,Runnable changed){activity=a;this.changed=changed;}
    void ui(Runnable action){activity.runOnUiThread(()->{if(!activity.isFinishing()&&!activity.isDestroyed())action.run();});}
    void message(String title,String body){new AlertDialog.Builder(activity).setTitle(title).setMessage(body).setPositiveButton(android.R.string.ok,null).show();}
    void show(){
        new AlertDialog.Builder(activity).setTitle(activity.getString(R.string.velocity4_dxvk_manager_title))
          .setMessage(activity.getString(R.string.velocity4_dxvk_manager_body))
          .setPositiveButton("Published packages",(d,w)->catalog())
          .setNeutralButton("Delete downloads",(d,w)->clearDownloads())
          .setNegativeButton("Recovery",(d,w)->{
              GameOptions.set(activity,GameOptions.ASYNC,"off");GameOptions.set(activity,GameOptions.PREWARM,"off");GameOptions.set(activity,GameOptions.PROFILE,"compatibility");changed.run();
              message("Recovery selected","The next game launch uses the bundled DXVK with synchronous compilation and conservative settings. Android cannot silently downgrade an installed APK; failed/cancelled installations retain the previous application.");
          }).show();
    }
    void clearDownloads(){if(downloading.get()){message("Download running","Cancel or finish the download first.");return;}File[] files=directory().listFiles();int count=0;if(files!=null)for(File f:files)if(f.getName().matches("[a-f0-9]{64}\\.apk(?:\\.part)?")&&f.delete())count++;message("Downloads deleted",Integer.toString(count));}
    File directory(){File dir=new File(activity.getCacheDir(),"dxvk-packages");dir.mkdirs();return dir;}
    static HttpURLConnection connect(String address,boolean catalog)throws IOException {
        String current=address;
        for(int redirect=0;redirect<5;redirect++){
            if(catalog){DxvkPackagePolicy.require(current.equals(CATALOG),"Unexpected catalog origin");}
            else DxvkPackagePolicy.require(DxvkPackagePolicy.redirectUrl(current),"Untrusted download redirect");
            HttpURLConnection c=(HttpURLConnection)new URL(current).openConnection();c.setInstanceFollowRedirects(false);c.setConnectTimeout(15000);c.setReadTimeout(15000);c.setRequestProperty("User-Agent","ProjectVelocity/4");
            int status=c.getResponseCode();if(status==200)return c;
            if(status==301||status==302||status==303||status==307||status==308){String next=c.getHeaderField("Location");c.disconnect();DxvkPackagePolicy.require(next!=null,"Missing redirect");current=new URL(new URL(current),next).toString();continue;}
            c.disconnect();throw new IOException("HTTP "+status);
        }throw new IOException("Too many redirects");
    }
    void catalog(){
        Toast.makeText(activity,"Loading verified package catalog…",Toast.LENGTH_SHORT).show();
        new Thread(()->{try{
            HttpURLConnection c=connect(CATALOG,true);ByteArrayOutputStream data=new ByteArrayOutputStream();
            try(InputStream in=c.getInputStream()){byte[] block=new byte[8192];int n;while((n=in.read(block))!=-1){DxvkPackagePolicy.require(data.size()+n<=65536,"Catalog too large");data.write(block,0,n);}}finally{c.disconnect();}
            JSONObject catalog=new JSONObject(data.toString("UTF-8"));DxvkPackagePolicy.require(catalog.getInt("schema")==1,"Unsupported catalog");JSONArray array=catalog.getJSONArray("packages");DxvkPackagePolicy.require(array.length()<=20,"Catalog too large");
            List<JSONObject> choices=new ArrayList<>();List<String> labels=new ArrayList<>();
            for(int i=0;i<array.length();i++){JSONObject p=array.getJSONObject(i);validateEntry(p);choices.add(p);labels.add(p.getString("category")+" · "+p.getString("version")+"\n"+p.getString("backend")+" · "+p.getString("validation"));}
            ui(()->new AlertDialog.Builder(activity).setTitle(activity.getString(R.string.velocity4_dxvk_packages_title)).setItems(labels.toArray(new String[0]),(d,w)->choose(choices.get(w))).setNegativeButton(android.R.string.cancel,null).show());
        }catch(Exception e){ui(()->message("Catalog unavailable",e.getMessage()));}},"dxvk-catalog").start();
    }
    static void validateEntry(JSONObject p)throws Exception {
        DxvkPackagePolicy.require(p.getString("abi").equals("arm64-v8a")&&p.getString("package").equals("com.nfsu2.androidevolved"),"Incompatible package identity");
        DxvkPackagePolicy.require(DxvkPackagePolicy.releaseUrl(p.getString("url")),"Unknown package source");
        DxvkPackagePolicy.require(p.getString("sha256").matches("[a-f0-9]{64}")&&p.getLong("bytes")>0&&p.getLong("bytes")<=DxvkPackagePolicy.MAX_APK,"Invalid package size/hash");
        DxvkPackagePolicy.require(Arrays.asList("Current","Stable","Legacy","Experimental").contains(p.getString("category")),"Unknown category");
    }
    void choose(JSONObject p){
        String version=p.optString("version");long code=p.optLong("versionCode");
        new AlertDialog.Builder(activity).setTitle(version).setMessage(activity.getString(R.string.velocity4_dxvk_package_verify_body)+(code<BuildConfig.VERSION_CODE?"Older version: download only. Android does not permit normal downgrade installation.":"Installation takes effect after Android restarts the application. Save and exit the game before installing."))
            .setPositiveButton("Download",(d,w)->download(p)).setNegativeButton(android.R.string.cancel,null).show();
    }
    void verify(File apk,JSONObject p)throws Exception {
        DxvkPackagePolicy.require(apk.length()==p.getLong("bytes")&&DxvkPackagePolicy.digest(apk).equals(p.getString("sha256")),"Package hash/size mismatch");
        DxvkPackagePolicy.verifyLibraries(apk);
        PackageManager pm=activity.getPackageManager();PackageInfo candidate=pm.getPackageArchiveInfo(apk.getPath(),PackageManager.GET_SIGNING_CERTIFICATES);
        PackageInfo installed=pm.getPackageInfo(activity.getPackageName(),PackageManager.GET_SIGNING_CERTIFICATES);
        DxvkPackagePolicy.require(candidate!=null&&candidate.packageName.equals(activity.getPackageName())&&candidate.getLongVersionCode()==p.getLong("versionCode")&&candidate.versionName.equals(p.getString("version")),"Package manifest mismatch");
        DxvkPackagePolicy.require(candidate.signingInfo!=null&&installed.signingInfo!=null,"Signing certificate unavailable");
        Set<String> a=new HashSet<>(),b=new HashSet<>();for(Signature s:candidate.signingInfo.getApkContentsSigners())a.add(DxvkPackagePolicy.hex(MessageDigest.getInstance("SHA-256").digest(s.toByteArray())));for(Signature s:installed.signingInfo.getApkContentsSigners())b.add(DxvkPackagePolicy.hex(MessageDigest.getInstance("SHA-256").digest(s.toByteArray())));
        DxvkPackagePolicy.require(!a.isEmpty()&&a.equals(b),"Signing certificate does not match installed Velocity");
    }
    void download(JSONObject p){
        if(!downloading.compareAndSet(false,true)){message("Download running","One package at a time.");return;}
        AtomicBoolean cancel=new AtomicBoolean(false);ProgressBar bar=new ProgressBar(activity,null,android.R.attr.progressBarStyleHorizontal);bar.setMax(100);
        AlertDialog progress=new AlertDialog.Builder(activity).setTitle(activity.getString(R.string.velocity4_dxvk_download_title)).setView(bar).setNegativeButton("Cancel",(d,w)->cancel.set(true)).create();progress.setOnCancelListener(d->cancel.set(true));progress.show();
        new Thread(()->{File part=null;try{
            String hash=p.getString("sha256");File apk=new File(directory(),hash+".apk");part=new File(directory(),hash+".apk.part");long expected=p.getLong("bytes");
            if(!apk.isFile()){
                HttpURLConnection c=connect(p.getString("url"),false);long size=0,last=0,start=System.nanoTime();
                try(InputStream in=c.getInputStream();FileOutputStream out=new FileOutputStream(part)){byte[] block=new byte[65536];int n;while((n=in.read(block))!=-1){DxvkPackagePolicy.require(!cancel.get(),"Cancelled");DxvkPackagePolicy.require(System.nanoTime()-start<300000000000L,"Download timed out");size+=n;DxvkPackagePolicy.require(size<=expected,"Package exceeds declared size");out.write(block,0,n);long now=System.nanoTime();if(now-last>250000000){last=now;int percent=(int)(size*100/expected);ui(()->bar.setProgress(percent));}}out.getFD().sync();}finally{c.disconnect();}
                verify(part,p);DxvkPackagePolicy.require(!cancel.get(),"Cancelled");DxvkPackagePolicy.require(part.renameTo(apk),"Cannot atomically install download");
            }else verify(apk,p);
            DxvkPackagePolicy.require(!cancel.get(),"Cancelled");ui(()->{progress.dismiss();if(p.optLong("versionCode")<BuildConfig.VERSION_CODE)message("Verified legacy download","Stored in private package cache. Normal Android installation cannot downgrade this version.");else new AlertDialog.Builder(activity).setTitle(activity.getString(R.string.velocity4_dxvk_verified_title)).setMessage(activity.getString(R.string.velocity4_dxvk_verified_body)).setPositiveButton("Install",(d,w)->install(apk)).setNegativeButton(android.R.string.cancel,null).show();});
        }catch(Exception e){if(part!=null)part.delete();ui(()->{progress.dismiss();message("Package not installed",e.getMessage());});}finally{downloading.set(false);}},"dxvk-download").start();
    }
    void install(File apk){
        if(!activity.getPackageManager().canRequestPackageInstalls()){
            message("Allow installation","Enable installation for Project Velocity in the next Android screen, then select the downloaded package again.");
            activity.startActivity(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,Uri.parse("package:"+activity.getPackageName())));return;
        }
        Uri uri=Uri.parse("content://"+activity.getPackageName()+".velocity.apks/"+apk.getName());
        try{activity.startActivity(new Intent(Intent.ACTION_VIEW).setDataAndType(uri,"application/vnd.android.package-archive").addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION));}catch(Exception e){message("Installer unavailable",e.getMessage());}
    }
}
