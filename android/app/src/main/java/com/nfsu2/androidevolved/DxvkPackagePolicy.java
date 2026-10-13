package com.nfsu2.androidevolved;

import java.io.*;
import java.net.URI;
import java.nio.*;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.*;
import java.util.zip.*;

/** Pure-Java package validation, shared by launcher and host tests. */
final class DxvkPackagePolicy {
    static final String RELEASE_PREFIX="https://github.com/silasquercloud/nfsu2-recomp-project-velocity/releases/download/";
    static final long MAX_APK=160L*1024*1024;
    static final Set<String> SYSTEM=new HashSet<>(Arrays.asList("libandroid.so","liblog.so","libvulkan.so","libaaudio.so","libdl.so","libm.so","libc.so"));
    static void require(boolean ok,String message)throws IOException {if(!ok)throw new IOException(message);}
    static boolean releaseUrl(String text){
        try{URI u=URI.create(text);return text.startsWith(RELEASE_PREFIX)&&u.getUserInfo()==null&&u.getPort()==-1&&u.getFragment()==null&&u.getQuery()==null&&!u.getPath().contains("..");}catch(Exception e){return false;}
    }
    static boolean redirectUrl(String text){
        try{URI u=URI.create(text);return "https".equals(u.getScheme())&&u.getUserInfo()==null&&u.getPort()==-1&&Arrays.asList("github.com","release-assets.githubusercontent.com","objects.githubusercontent.com").contains(u.getHost());}catch(Exception e){return false;}
    }
    static String hex(byte[] b){StringBuilder s=new StringBuilder();for(byte v:b)s.append(String.format(Locale.ROOT,"%02x",v&255));return s.toString();}
    static String digest(File file)throws Exception {
        MessageDigest hash=MessageDigest.getInstance("SHA-256");byte[] block=new byte[65536];
        try(InputStream in=new FileInputStream(file)){int n;while((n=in.read(block))!=-1)hash.update(block,0,n);}return hex(hash.digest());
    }
    static byte[] range(ZipFile zip,ZipEntry entry,long offset,int length)throws IOException {
        require(offset>=0&&length>=0&&length<=4*1024*1024&&offset<=entry.getSize()&&length<=entry.getSize()-offset,"ELF range outside library");
        byte[] bytes=new byte[length];try(InputStream in=zip.getInputStream(entry)){
            long remaining=offset;while(remaining>0){long n=in.skip(remaining);if(n==0){require(in.read()!=-1,"Truncated ELF");n=1;}remaining-=n;}
            int pos=0;while(pos<length){int n=in.read(bytes,pos,length-pos);require(n>0,"Truncated ELF");pos+=n;}
        }return bytes;
    }
    static ByteBuffer buffer(byte[] b){return ByteBuffer.wrap(b).order(ByteOrder.LITTLE_ENDIAN);}
    static Set<String> dependencies(ZipFile zip,ZipEntry entry)throws IOException {
        byte[] head=range(zip,entry,0,64);ByteBuffer h=buffer(head);
        require(head[0]==127&&head[1]=='E'&&head[2]=='L'&&head[3]=='F'&&head[4]==2&&head[5]==1&&h.getShort(18)==183,"Library is not ELF64 ARM64");
        int count=h.getShort(56)&65535, stride=h.getShort(54)&65535;long table=h.getLong(32);
        require(count>0&&count<=128&&stride==56,"Unsupported ELF program table");
        ByteBuffer ph=buffer(range(zip,entry,table,count*stride));List<long[]> loads=new ArrayList<>();long dyn=-1,size=0;
        for(int i=0;i<count;i++){int p=i*stride,type=ph.getInt(p);long off=ph.getLong(p+8),va=ph.getLong(p+16),bytes=ph.getLong(p+32);
            require(off>=0&&bytes>=0&&off<=entry.getSize()&&bytes<=entry.getSize()-off,"Invalid ELF segment");
            if(type==1)loads.add(new long[]{off,va,bytes});if(type==2){dyn=off;size=bytes;}}
        require(dyn>=0&&size>0&&size<=1024*1024&&size%16==0,"Missing ELF dynamic table");
        ByteBuffer d=buffer(range(zip,entry,dyn,(int)size));long strings=-1,stringSize=0;List<Long> names=new ArrayList<>();
        for(int p=0;p<d.capacity();p+=16){long tag=d.getLong(p),v=d.getLong(p+8);if(tag==0)break;if(tag==5)strings=v;if(tag==10)stringSize=v;if(tag==1)names.add(v);}
        require(stringSize>0&&stringSize<=4*1024*1024,"Invalid ELF string table");long offset=-1;
        for(long[] load:loads)if(strings>=load[1]&&strings-load[1]<=load[2]&&stringSize<=load[2]-(strings-load[1]))offset=load[0]+strings-load[1];
        require(offset>=0,"Unmapped ELF strings");byte[] str=range(zip,entry,offset,(int)stringSize);Set<String> result=new HashSet<>();
        for(long index:names){require(index>=0&&index<str.length,"Invalid ELF dependency");int end=(int)index;while(end<str.length&&str[end]!=0&&end-index<=255)end++;require(end<str.length&&str[end]==0,"Invalid ELF dependency name");result.add(new String(str,(int)index,end-(int)index,StandardCharsets.US_ASCII));}
        return result;
    }
    static void verifyLibraries(File apk)throws IOException {
        try(ZipFile zip=new ZipFile(apk)){Set<String> entries=new HashSet<>(),libs=new HashSet<>();List<ZipEntry> nativeEntries=new ArrayList<>();
            for(Enumeration<? extends ZipEntry> e=zip.entries();e.hasMoreElements();){ZipEntry z=e.nextElement();String n=z.getName();require(entries.add(n),"Duplicate APK entry");
                require(!n.contains("..")&&!n.startsWith("/"),"Invalid APK entry");
                if(n.startsWith("lib/")&&n.endsWith(".so")){require(n.startsWith("lib/arm64-v8a/")&&n.substring(14).indexOf('/')<0,"Unexpected ABI/path");libs.add(n.substring(n.lastIndexOf('/')+1));nativeEntries.add(z);}}
            require(libs.containsAll(Arrays.asList("libnfsu2_android.so","libnfsu2_diagnostics.so","libmain_hook.so","libfile_redirect_hook.so","libgsl_alloc_hook.so","libhook_impl.so")),"Incomplete port package");
            for(ZipEntry z:nativeEntries)for(String dependency:dependencies(zip,z))require(libs.contains(dependency)||SYSTEM.contains(dependency),"Unavailable native dependency: "+dependency);
        }
    }
}
