package com.nfsu2.androidevolved;
import android.content.*;
import android.database.*;
import android.net.Uri;
import android.os.*;
import android.provider.OpenableColumns;
import java.io.*;
/** Grants the Android package installer read-only access to a verified package. */
public final class VelocityApkProvider extends ContentProvider {
    public boolean onCreate(){return true;}
    private File file(Uri uri)throws FileNotFoundException {
        String name=uri.getLastPathSegment();
        if(name==null||!name.matches("[a-f0-9]{64}\\.apk")||uri.getPathSegments().size()!=1)throw new FileNotFoundException();
        File file=new File(new File(getContext().getCacheDir(),"dxvk-packages"),name);
        if(!file.isFile())throw new FileNotFoundException();return file;
    }
    public String getType(Uri uri){return "application/vnd.android.package-archive";}
    public ParcelFileDescriptor openFile(Uri uri,String mode)throws FileNotFoundException {if(!"r".equals(mode))throw new FileNotFoundException("Read only");return ParcelFileDescriptor.open(file(uri),ParcelFileDescriptor.MODE_READ_ONLY);}
    public Cursor query(Uri uri,String[] projection,String selection,String[] args,String order){
        try{File f=file(uri);String[] cols=projection==null?new String[]{OpenableColumns.DISPLAY_NAME,OpenableColumns.SIZE}:projection;MatrixCursor c=new MatrixCursor(cols);Object[] values=new Object[cols.length];for(int i=0;i<cols.length;i++)values[i]=OpenableColumns.SIZE.equals(cols[i])?f.length():OpenableColumns.DISPLAY_NAME.equals(cols[i])?"Project-Velocity.apk":null;c.addRow(values);return c;}catch(FileNotFoundException e){return null;}
    }
    public Uri insert(Uri uri,ContentValues values){throw new UnsupportedOperationException();}
    public int update(Uri uri,ContentValues values,String selection,String[] args){throw new UnsupportedOperationException();}
    public int delete(Uri uri,String selection,String[] args){throw new UnsupportedOperationException();}
}
