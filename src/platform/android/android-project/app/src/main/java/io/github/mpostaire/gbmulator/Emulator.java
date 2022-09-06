package io.github.mpostaire.gbmulator;

import android.bluetooth.BluetoothSocket;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.database.ContentObserver;
import android.net.Uri;
import java.io.InputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;

import android.os.Bundle;
import android.os.Handler;
import android.os.ParcelFileDescriptor;
import android.provider.Settings;
import android.util.Log;
import android.widget.Toast;
import org.libsdl.app.SDLActivity;

public class Emulator extends SDLActivity {

    SharedPreferences preferences;
    SharedPreferences.Editor preferencesEditor;

    BluetoothSocket socket;

    Uri romUri;
    boolean resume;
    int orientation;
    int emuMode;
    int palette;
    float speed;
    float sound;
    int frameSkip;
    int layoutEditor;
    float buttonOpacity;

    float portraitDpadX, portraitDpadY;
    float portraitAX, portraitAY;
    float portraitBX, portraitBY;
    float portraitStartX, portraitStartY;
    float portraitSelectX, portraitSelectY;

    float landscapeDpadX, landscapeDpadY;
    float landscapeAX, landscapeAY;
    float landscapeBX, landscapeBY;
    float landscapeStartX, landscapeStartY;
    float landscapeSelectX, landscapeSelectY;

    public native void receiveROMData(
        byte[] data, int size,
        boolean resume, boolean is_link_connected, int emu_mode, int palette, float emu_speed, float sound, int emu_frame_skip,
        float buttons_opacity,
        float portraitDpadX, float portraitDpadY,
        float portraitAX, float portraitAY,
        float portraitBX, float portraitBY,
        float portraitStartX, float portraitStartY,
        float portraitSelectX, float portraitSelectY,
        float landscapeDpadX, float landscapeDpadY,
        float landscapeAX, float landscapeAY,
        float landscapeBX, float landscapeBY,
        float landscapeStartX, float landscapeStartY,
        float landscapeSelectX, float landscapeSelectY);

    public native void enterLayoutEditor(float buttons_opacity,
        boolean is_landscape,
        float dpad_x, float dpad_y,
        float a_x, float a_y,
        float b_x, float b_y,
        float start_x, float start_y,
        float select_x, float select_y);

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

        socket = ((GBmulatorApp) getApplication()).socket;

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
        buttonOpacity = preferences.getFloat(UserSettings.BUTTONS_OPACITY, UserSettings.BUTTONS_OPACITY_DEFAULT);

        portraitDpadX = preferences.getFloat(UserSettings.PORTRAIT_DPAD_X, UserSettings.PORTRAIT_DPAD_X_DEFAULT);
        portraitDpadY = preferences.getFloat(UserSettings.PORTRAIT_DPAD_Y, UserSettings.PORTRAIT_DPAD_Y_DEFAULT);
        portraitAX = preferences.getFloat(UserSettings.PORTRAIT_A_X, UserSettings.PORTRAIT_A_X_DEFAULT);
        portraitAY = preferences.getFloat(UserSettings.PORTRAIT_A_Y, UserSettings.PORTRAIT_A_Y_DEFAULT);
        portraitBX = preferences.getFloat(UserSettings.PORTRAIT_B_X, UserSettings.PORTRAIT_B_X_DEFAULT);
        portraitBY = preferences.getFloat(UserSettings.PORTRAIT_B_Y, UserSettings.PORTRAIT_B_Y_DEFAULT);
        portraitStartX = preferences.getFloat(UserSettings.PORTRAIT_START_X, UserSettings.PORTRAIT_START_X_DEFAULT);
        portraitStartY = preferences.getFloat(UserSettings.PORTRAIT_START_Y, UserSettings.PORTRAIT_START_Y_DEFAULT);
        portraitSelectX = preferences.getFloat(UserSettings.PORTRAIT_SELECT_X, UserSettings.PORTRAIT_SELECT_X_DEFAULT);
        portraitSelectY = preferences.getFloat(UserSettings.PORTRAIT_SELECT_Y, UserSettings.PORTRAIT_SELECT_Y_DEFAULT);

        landscapeDpadX = preferences.getFloat(UserSettings.LANDSCAPE_DPAD_X, UserSettings.LANDSCAPE_DPAD_X_DEFAULT);
        landscapeDpadY = preferences.getFloat(UserSettings.LANDSCAPE_DPAD_Y, UserSettings.LANDSCAPE_DPAD_Y_DEFAULT);
        landscapeAX = preferences.getFloat(UserSettings.LANDSCAPE_A_X, UserSettings.LANDSCAPE_A_X_DEFAULT);
        landscapeAY = preferences.getFloat(UserSettings.LANDSCAPE_A_Y, UserSettings.LANDSCAPE_A_Y_DEFAULT);
        landscapeBX = preferences.getFloat(UserSettings.LANDSCAPE_B_X, UserSettings.LANDSCAPE_B_X_DEFAULT);
        landscapeBY = preferences.getFloat(UserSettings.LANDSCAPE_B_Y, UserSettings.LANDSCAPE_B_Y_DEFAULT);
        landscapeStartX = preferences.getFloat(UserSettings.LANDSCAPE_START_X, UserSettings.LANDSCAPE_START_X_DEFAULT);
        landscapeStartY = preferences.getFloat(UserSettings.LANDSCAPE_START_Y, UserSettings.LANDSCAPE_START_Y_DEFAULT);
        landscapeSelectX = preferences.getFloat(UserSettings.LANDSCAPE_SELECT_X, UserSettings.LANDSCAPE_SELECT_X_DEFAULT);
        landscapeSelectY = preferences.getFloat(UserSettings.LANDSCAPE_SELECT_Y, UserSettings.LANDSCAPE_SELECT_Y_DEFAULT);

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
                enterLayoutEditor(buttonOpacity, false,
                        portraitDpadX, portraitDpadY,
                        portraitAX, portraitAY,
                        portraitBX, portraitBY,
                        portraitStartX, portraitStartY,
                        portraitSelectX, portraitSelectY);
                break;
            case 2:
                enterLayoutEditor(buttonOpacity, true,
                        landscapeDpadX, landscapeDpadY,
                        landscapeAX, landscapeAY,
                        landscapeBX, landscapeBY,
                        landscapeStartX, landscapeStartY,
                        landscapeSelectX, landscapeSelectY);
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
                    rom, rom.length, resume, ((GBmulatorApp) getApplication()).isLinkConnected, emuMode, palette, speed, sound, frameSkip,
                    buttonOpacity,
                    portraitDpadX, portraitDpadY, portraitAX, portraitAY, portraitBX, portraitBY, portraitStartX, portraitStartY, portraitSelectX, portraitSelectY,
                    landscapeDpadX, landscapeDpadY, landscapeAX, landscapeAY, landscapeBX, landscapeBY, landscapeStartX, landscapeStartY, landscapeSelectX, landscapeSelectY);
        } catch (IOException e) {
            errorToast("The selected file is not a valid ROM.");
            finish();
        }

    }

    public void applyLayoutPreferences(
            boolean isLandscape,
            float dpadX, float dpadY,
            float aX, float aY,
            float bX, float bY,
            float selectX, float selectY,
            float startX, float startY) {
        preferencesEditor.putFloat(isLandscape ? UserSettings.LANDSCAPE_DPAD_X : UserSettings.PORTRAIT_DPAD_X, dpadX);
        preferencesEditor.putFloat(isLandscape ? UserSettings.LANDSCAPE_DPAD_Y : UserSettings.PORTRAIT_DPAD_Y, dpadY);
        preferencesEditor.putFloat(isLandscape ? UserSettings.LANDSCAPE_A_X : UserSettings.PORTRAIT_A_X, aX);
        preferencesEditor.putFloat(isLandscape ? UserSettings.LANDSCAPE_A_Y : UserSettings.PORTRAIT_A_Y, aY);
        preferencesEditor.putFloat(isLandscape ? UserSettings.LANDSCAPE_B_X : UserSettings.PORTRAIT_B_X, bX);
        preferencesEditor.putFloat(isLandscape ? UserSettings.LANDSCAPE_B_Y : UserSettings.PORTRAIT_B_Y, bY);
        preferencesEditor.putFloat(isLandscape ? UserSettings.LANDSCAPE_SELECT_X : UserSettings.PORTRAIT_SELECT_X, selectX);
        preferencesEditor.putFloat(isLandscape ? UserSettings.LANDSCAPE_SELECT_Y : UserSettings.PORTRAIT_SELECT_Y, selectY);
        preferencesEditor.putFloat(isLandscape ? UserSettings.LANDSCAPE_START_X : UserSettings.PORTRAIT_START_X, startX);
        preferencesEditor.putFloat(isLandscape ? UserSettings.LANDSCAPE_START_Y : UserSettings.PORTRAIT_START_Y, startY);

        preferencesEditor.apply();
    }

    public int linkSendData(byte[] data) {
        try {
            socket.getOutputStream().write(data);
            return data.length;
        } catch (IOException e) {
            e.printStackTrace();
            return -1;
        }
    }

    public byte[] linkReceiveData(int size) {
        byte[] buf = new byte[size];
        try {
            socket.getInputStream().read(buf);
        } catch (IOException e) {
            e.printStackTrace();
            return null;
        }
        return buf;
    }

}
