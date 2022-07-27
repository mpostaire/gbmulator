package io.github.mpostaire.gbmulator;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.database.ContentObserver;
import android.net.Uri;
import java.io.InputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;

import android.os.Bundle;
import android.os.Handler;
import android.provider.Settings;
import android.util.Log;
import android.widget.Toast;
import org.libsdl.app.SDLActivity;

public class GBmulator extends SDLActivity {

    Intent intent = null;

    public native void receiveROMData(byte[] data, int size);

    public boolean canRotate() {
        try {
            return Settings.System.getInt(getContentResolver(), Settings.System.ACCELEROMETER_ROTATION) == 1;
        } catch (Settings.SettingNotFoundException e) {
            return false;
        }
    }

    public void setAllowedOrientations() {
        if (canRotate()) {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR);
        } else {
            if (mCurrentOrientation == SDL_ORIENTATION_PORTRAIT || mCurrentOrientation == SDL_ORIENTATION_PORTRAIT_FLIPPED || mCurrentOrientation == SDL_ORIENTATION_UNKNOWN)
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
            else
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Uri uri = Settings.System.getUriFor(Settings.System.ACCELEROMETER_ROTATION);
        ContentObserver rotationObserver = new ContentObserver(new Handler()) {
            @Override
            public void onChange(boolean selfChange) {
                super.onChange(selfChange);
                setAllowedOrientations();
            }
        };
        getContentResolver().registerContentObserver(uri,true, rotationObserver);
    }

    public void errorToast(String msg) {
        Toast.makeText(
                this,
                msg,
                Toast.LENGTH_SHORT
        ).show();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
    
        if (requestCode == 200 && resultCode == RESULT_OK && data != null) {
            Uri uri = data.getData();
            String extension = uri.getLastPathSegment();
            int dotIndex = extension.lastIndexOf('.');
            if (dotIndex < 0) {
                errorToast("The selected file is not a valid ROM.");
                intent = null;
                return;
            }
            extension = extension.substring(dotIndex);
            if (!(extension.equals(".gb") || extension.equals(".cgb") || extension.equals(".gbc"))) {
                errorToast("The selected file is not a valid ROM.");
                intent = null;
                return;
            }
            try {
                InputStream in = getApplication().getContentResolver().openInputStream(uri);
                ByteArrayOutputStream byteBuffer = new ByteArrayOutputStream();

                int bufferSize = 1024;
                byte[] buffer = new byte[bufferSize];

                int len = 0;
                while ((len = in.read(buffer)) != -1)
                    byteBuffer.write(buffer, 0, len);

                // and then we can return your byte array.
                byte[] rom_data = byteBuffer.toByteArray();
                byteBuffer.close();
                in.close();

                receiveROMData(rom_data, rom_data.length);
            } catch (IOException e) {
                errorToast("The selected file is not a valid ROM.");
            } finally {
                // allow new file picker intents to be sent
                intent = null;
            }
        } else {
            intent = null;
            errorToast("Something went wrong...");
        }
    }

    public void filePicker() {
        if (intent != null) // don't send a file picker intent while another is already being processed
            return;
        intent = new Intent();
        intent.setType("*/*");
        intent.setAction(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        startActivityForResult(intent, 200);
    }

}
