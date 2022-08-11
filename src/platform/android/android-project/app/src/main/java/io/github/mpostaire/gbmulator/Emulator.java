package io.github.mpostaire.gbmulator;

import android.content.SharedPreferences;
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

    SharedPreferences preferences;
    SharedPreferences.Editor preferencesEditor;

    Uri romUri;
    boolean resume;
    int orientation;
    int emuMode;
    int palette;
    float speed;
    float sound;
    int frameSkip;
    int layoutEditor;

    float portraitScreenX;
    float portraitScreenY;
    float portraitSreenSize;

    float landscapeScreenX;
    float landscapeScreenY;
    float landscapeScreenSize;

    public native void receiveROMData(
            byte[] data, int size,
            boolean resume, int emu_mode, int palette, float emu_speed, float sound, int emu_frame_skip,
            float portraitScreenX, float portraitScreenY, float portraitSreenSize,
            float landscapeScreenX, float landscapeScreenY, float landscapeScreenSize);

    public native void enterLayoutEditor(boolean is_landscape, float screenX, float screenY, float screenSize);

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

        Bundle extras = getIntent().getExtras();
        if (extras == null) {
            errorToast("Oops... Something bad happened!");
            finish();
            return;
        }

        preferences = getSharedPreferences(UserSettings.PREFERENCES, MODE_PRIVATE);
        preferencesEditor = preferences.edit();

        romUri = extras.getParcelable("rom_uri");
        resume = extras.getBoolean("resume");
        layoutEditor = extras.getInt("layout_editor");
        if (layoutEditor == 1)
            orientation = ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
        else if (orientation == 2)
            orientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE;
        else
            orientation = extras.getInt("orientation");

        emuMode = preferences.getInt(UserSettings.EMULATION_MODE, UserSettings.EMULATION_MODE_DEFAULT);
        palette = preferences.getInt(UserSettings.EMULATION_PALETTE, UserSettings.EMULATION_PALETTE_DEFAULT);
        speed = preferences.getFloat(UserSettings.EMULATION_SPEED, UserSettings.EMULATION_SPEED_DEFAULT);
        sound = preferences.getFloat(UserSettings.EMULATION_SOUND, UserSettings.EMULATION_SOUND_DEFAULT);
        frameSkip = preferences.getInt(UserSettings.FRAME_SKIP, UserSettings.FRAME_SKIP_DEFAULT);

        portraitScreenX = preferences.getFloat(UserSettings.PORTRAIT_SCREEN_X, UserSettings.PORTRAIT_SCREEN_X_DEFAULT);
        portraitScreenY = preferences.getFloat(UserSettings.PORTRAIT_SCREEN_Y, UserSettings.PORTRAIT_SCREEN_Y_DEFAULT);
        portraitSreenSize = preferences.getFloat(UserSettings.PORTRAIT_SCREEN_SIZE, UserSettings.PORTRAIT_SCREEN_SIZE_DEFAULT);

        landscapeScreenX = preferences.getFloat(UserSettings.LANDSCAPE_SCREEN_X, UserSettings.LANDSCAPE_SCREEN_X_DEFAULT);
        landscapeScreenY = preferences.getFloat(UserSettings.LANDSCAPE_SCREEN_Y, UserSettings.LANDSCAPE_SCREEN_Y_DEFAULT);
        landscapeScreenSize = preferences.getFloat(UserSettings.LANDSCAPE_SCREEN_SIZE, UserSettings.LANDSCAPE_SCREEN_SIZE_DEFAULT);

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

    @Override
    public void setOrientationBis(int w, int h, boolean resizable, String hint) {
        Log.v("SDL", "setOrientation() requestedOrientation=" + orientation + " width=" + w +" height="+ h +" resizable=" + resizable + " hint=" + hint);
        mSingleton.setRequestedOrientation(orientation);
    }

    public void errorToast(String msg) {
        Toast.makeText(this, msg, Toast.LENGTH_SHORT).show();
    }

    public void onNativeAppReady() {
        switch (layoutEditor) {
            case 0:
                requestROM();
                break;
            case 1:
                enterLayoutEditor(false, portraitScreenX, portraitScreenY, portraitSreenSize);
                break;
            case 2:
                enterLayoutEditor(true, landscapeScreenX, landscapeScreenY, landscapeScreenSize);
                break;
        }
    }

    public void requestROM() {
        try {
            InputStream in = getApplication().getContentResolver().openInputStream(romUri);
            ByteArrayOutputStream byteBuffer = new ByteArrayOutputStream();

            int bufferSize = 1024;
            byte[] buffer = new byte[bufferSize];

            int len;
            while ((len = in.read(buffer)) != -1)
                byteBuffer.write(buffer, 0, len);

            byte[] rom = byteBuffer.toByteArray();
            byteBuffer.close();
            in.close();

            receiveROMData(
                    rom, rom.length, resume, emuMode, palette, speed, sound, frameSkip,
                    portraitScreenX, portraitScreenY, portraitSreenSize,
                    landscapeScreenX, landscapeScreenY, landscapeScreenSize);
        } catch (IOException e) {
            errorToast("The selected file is not a valid ROM.");
            finish();
        }

    }

    public void applyLayoutPreferences(boolean isLandscape, float screenX, float screenY, float screenSize) {
        preferencesEditor.putFloat(isLandscape ? UserSettings.LANDSCAPE_SCREEN_X : UserSettings.PORTRAIT_SCREEN_X, screenX);
        preferencesEditor.putFloat(isLandscape ? UserSettings.LANDSCAPE_SCREEN_Y : UserSettings.PORTRAIT_SCREEN_Y, screenY);
        preferencesEditor.putFloat(isLandscape ? UserSettings.LANDSCAPE_SCREEN_SIZE : UserSettings.PORTRAIT_SCREEN_SIZE, screenSize);

        preferencesEditor.apply();
    }

}
