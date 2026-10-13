package com.nfsu2.androidevolved;

import android.content.Context;
import android.content.res.Configuration;
import java.util.Locale;

/** Launcher locale is separate from the game's original language files. */
final class UiText {
    static final String PREF="launcher_locale";
    static Context wrap(Context context) {
        String tag=GameOptions.prefs(context).getString(PREF,"system");
        if (!tag.equals("pt-BR") && !tag.equals("en")) return context;
        Configuration config=new Configuration(context.getResources().getConfiguration());
        config.setLocales(new android.os.LocaleList(Locale.forLanguageTag(tag)));
        return context.createConfigurationContext(config);
    }
    static String t(Context context,int id) { return context.getString(id); }
    static String error(Context context,Throwable error) {
        if (error instanceof DriverArchive.Failure) return code(context,((DriverArchive.Failure)error).code);
        String message=error.getMessage();
        if(message!=null&&message.startsWith("Unavailable native dependency: "))return context.getString(R.string.v5_missing_dependency,message.substring(31));
        if(message!=null)switch(message){
            case "ELF range outside library": return t(context,R.string.v5_elf_error_0);
            case "Truncated ELF": return t(context,R.string.v5_elf_error_1);
            case "Library is not ELF64 ARM64": return t(context,R.string.v5_elf_error_2);
            case "Unsupported ELF program table": return t(context,R.string.v5_elf_error_3);
            case "Invalid ELF segment": return t(context,R.string.v5_elf_error_4);
            case "Missing ELF dynamic table": return t(context,R.string.v5_elf_error_5);
            case "Invalid ELF string table": return t(context,R.string.v5_elf_error_6);
            case "Unmapped ELF strings": return t(context,R.string.v5_elf_error_7);
            case "Invalid ELF dependency": return t(context,R.string.v5_elf_error_8);
            case "Invalid ELF dependency name": return t(context,R.string.v5_elf_error_9);
            case "Duplicate APK entry": return t(context,R.string.v5_elf_error_10);
            case "Invalid APK entry": return t(context,R.string.v5_elf_error_11);
            case "Unexpected ABI/path": return t(context,R.string.v5_elf_error_12);
            case "Incomplete port package": return t(context,R.string.v5_elf_error_13);
            case "Unexpected catalog origin": return t(context,R.string.v5_unexpected_catalog_origin);
            case "Untrusted download redirect": return t(context,R.string.v5_untrusted_download_redirect);
            case "Missing redirect": return t(context,R.string.v5_missing_redirect);
            case "Too many redirects": return t(context,R.string.v5_too_many_redirects);
            case "Catalog too large": return t(context,R.string.v5_catalog_too_large);
            case "Unsupported catalog": return t(context,R.string.v5_unsupported_catalog);
            case "Incompatible package identity": return t(context,R.string.v5_incompatible_package_identity);
            case "Unknown package source": return t(context,R.string.v5_unknown_package_source);
            case "Invalid package size/hash": return t(context,R.string.v5_invalid_package_size_hash);
            case "Unknown category": return t(context,R.string.v5_unknown_category);
            case "Package hash/size mismatch": return t(context,R.string.v5_package_hash_size_mismatch);
            case "Package manifest mismatch": return t(context,R.string.v5_package_manifest_mismatch);
            case "Signing certificate unavailable": return t(context,R.string.v5_signing_certificate_unavailable);
            case "Signing certificate does not match installed Velocity": return t(context,R.string.v5_signing_certificate_does_not_match_installed_velocity);
            case "Cancelled": return t(context,R.string.v5_cancelled);
            case "Download timed out": return t(context,R.string.v5_download_timed_out);
            case "Package exceeds declared size": return t(context,R.string.v5_package_exceeds_declared_size);
            case "Cannot atomically install download": return t(context,R.string.v5_cannot_atomically_install_download);
        }
        return context.getString(R.string.v5_generic_failure,error.getLocalizedMessage()==null?error.getClass().getSimpleName():error.getLocalizedMessage());
    }
    static String code(Context context,String code) {
        switch(code) {
            case "probe_driver": return t(context,R.string.probe_driver);
            case "probe_capabilities": return t(context,R.string.probe_capabilities);
            case "probe_title": return t(context,R.string.probe_title);
            case "boot_pending": return t(context,R.string.boot_pending);
            case "boot_running": return t(context,R.string.boot_running);
            case "exe_read_error": return t(context,R.string.exe_read_error);
            case "exe_mismatch": return t(context,R.string.exe_mismatch);
            case "probe_cancelled": return t(context,R.string.probe_cancelled);
            case "probe_timeout": return t(context,R.string.probe_timeout);
            case "probe_failed": return t(context,R.string.probe_failed);
            case "driver_open": return t(context,R.string.driver_open);
            case "driver_entry": return t(context,R.string.driver_entry);
            case "driver_instance": return t(context,R.string.driver_instance);
            case "driver_functions": return t(context,R.string.driver_functions);
            case "no_vulkan_gpu": return t(context,R.string.no_vulkan_gpu);
            case "zip_count": return t(context,R.string.zip_count);
            case "zip_paths": return t(context,R.string.zip_paths);
            case "mkdir_error": return t(context,R.string.mkdir_error);
            case "driver_size": return t(context,R.string.driver_size);
            case "driver_abi": return t(context,R.string.driver_abi);
            case "driver_link": return t(context,R.string.driver_link);
            case "delete_error": return t(context,R.string.delete_error);
            case "driver_mkdir": return t(context,R.string.driver_mkdir);
            case "zip_open": return t(context,R.string.zip_open);
            case "driver_meta": return t(context,R.string.driver_meta);
            case "driver_library": return t(context,R.string.driver_library);
            case "driver_android": return t(context,R.string.driver_android);
            case "driver_name": return t(context,R.string.driver_name);
            case "driver_install": return t(context,R.string.driver_install);
            case "package_install": return t(context,R.string.package_install);
            case "driver_ambiguous": return t(context,R.string.driver_ambiguous);
            case "driver_protect": return t(context,R.string.driver_protect);
            case "driver_path": return t(context,R.string.driver_path);
            default: return code;
        }
    }
    private static String runtimeDetail(Context context,String detail) {
        if(detail.startsWith("hle_missing:"))return t(context,R.string.runtime_call_missing)+detail.substring("hle_missing:".length());
        if(detail.startsWith("network_unimplemented:"))return t(context,R.string.network_unimplemented)+detail.substring("network_unimplemented:".length());
        if(detail.contains("DXVK pipeline compilation failed after retry"))return t(context,R.string.v5_pipeline_failure);
        return detail;
    }
    static String status(Context context,String value) {
        if(value.startsWith("guest_boot_")) {
            if(value.equals("guest_boot_running"))return t(context,R.string.boot_running);
            if(value.equals("guest_boot_pending"))return t(context,R.string.boot_pending);
            if(value.startsWith("guest_boot_finished:"))return t(context,R.string.boot_finished)+value.substring("guest_boot_finished:".length());
            if(value.startsWith("guest_boot_stopped:"))return t(context,R.string.boot_stopped)+runtimeDetail(context,value.substring("guest_boot_stopped:".length()));
        }
        if(value.startsWith("driver_custom_error:"))return t(context,R.string.custom_driver_failed)+value.substring("driver_custom_error:".length());
        if(value.startsWith("directory_read_error:"))return t(context,R.string.directory_read_error)+value.substring(21);
        if(value.startsWith("directory_empty:"))return t(context,R.string.directory_empty)+value.substring(16);
        return code(context,value);
    }
}
