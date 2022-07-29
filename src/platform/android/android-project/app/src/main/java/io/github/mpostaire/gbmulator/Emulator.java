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

public class Emulator extends SDLActivity {

    public native void receiveROMData(byte[] data, int size, boolean resume);

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
        Toast.makeText(this, msg, Toast.LENGTH_SHORT).show();
    }

    public void requestROM() {
        Bundle extras = getIntent().getExtras();
        if (extras == null) {
            receiveROMData(null, 0, false);
            return;
        }

        try {
            Uri rom_uri = extras.getParcelable("rom_uri");
            boolean resume = extras.getBoolean("resume");

            InputStream in = getApplication().getContentResolver().openInputStream(rom_uri);
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

            receiveROMData(rom_data, rom_data.length, resume);
        } catch (IOException e){
            errorToast("The selected file is not a valid ROM.");
            finish();
        }

    }

}
