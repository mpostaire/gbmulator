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

    Uri romUri;
    boolean resume;
    int orientation;
    int emuMode;
    int palette;
    float speed;
    float sound;
    int frameSkip;

    public native void receiveROMData(byte[] data, int size, boolean resume, int emu_mode, int palette, float emu_speed, float sound, int emu_frame_skip);

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

        romUri = extras.getParcelable("rom_uri");
        resume = extras.getBoolean("resume");
        orientation = extras.getInt("orientation");

        SharedPreferences preferences = getSharedPreferences(UserSettings.PREFERENCES, MODE_PRIVATE);
        emuMode = preferences.getInt(UserSettings.EMULATION_MODE, UserSettings.EMULATION_MODE_DEFAULT);
        palette = preferences.getInt(UserSettings.EMULATION_PALETTE, UserSettings.EMULATION_PALETTE_DEFAULT);
        speed = preferences.getFloat(UserSettings.EMULATION_SPEED, UserSettings.EMULATION_SPEED_DEFAULT);
        sound = preferences.getFloat(UserSettings.EMULATION_SOUND, UserSettings.EMULATION_SOUND_DEFAULT);
        frameSkip = preferences.getInt(UserSettings.FRAME_SKIP, UserSettings.FRAME_SKIP_DEFAULT);

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

            receiveROMData(rom, rom.length, resume, emuMode, palette, speed, sound, frameSkip);
        } catch (IOException e) {
            errorToast("The selected file is not a valid ROM.");
            finish();
        }

    }

}
