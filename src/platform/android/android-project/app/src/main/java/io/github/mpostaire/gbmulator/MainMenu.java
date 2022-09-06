package io.github.mpostaire.gbmulator;

import android.Manifest;
import android.annotation.SuppressLint;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothSocket;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import androidx.annotation.NonNull;
import androidx.core.app.ActivityCompat;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;


public class MainMenu extends AppCompatActivity {

    Intent filePickerIntent = null;
    Uri romUri = null;
    TextView loadedROMBottom;
    TextView loadedROMTop;
    Button resumeButton;
    Button resetButton;

    SharedPreferences preferences;
    int emu_mode = 0;

    BluetoothSocket socket = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        loadedROMBottom = findViewById(R.id.loadedROMBottom);
        loadedROMTop = findViewById(R.id.loadedROMTop);

        resumeButton = findViewById(R.id.resumeButton);
        resetButton = findViewById(R.id.resetButton);

        preferences = getSharedPreferences(UserSettings.PREFERENCES, MODE_PRIVATE);
    }

    public void errorToast(String msg) {
        Toast.makeText(this, msg, Toast.LENGTH_SHORT).show();
    }

    public void launchEmulator(boolean resume) {
        if (romUri == null)
            return;
        int new_emu_mode = preferences.getInt(UserSettings.EMULATION_MODE, UserSettings.EMULATION_MODE_DEFAULT);
        if (emu_mode != 0 && emu_mode != new_emu_mode)
            resume = false;
        emu_mode = new_emu_mode;
        Intent i = new Intent(MainMenu.this, Emulator.class);
        i.putExtra("rom_uri", romUri);
        i.putExtra("resume", resume);
        i.putExtra("palette", preferences.getInt(UserSettings.EMULATION_PALETTE, UserSettings.EMULATION_PALETTE_DEFAULT));
        i.putExtra("sound", preferences.getFloat(UserSettings.EMULATION_SOUND, UserSettings.EMULATION_SOUND_DEFAULT));
        i.putExtra("speed", preferences.getFloat(UserSettings.EMULATION_SPEED, UserSettings.EMULATION_SPEED_DEFAULT));
        i.putExtra("frame_skip", preferences.getInt(UserSettings.FRAME_SKIP, UserSettings.FRAME_SKIP_DEFAULT));
        i.putExtra("orientation", getRequestedOrientation());
        startActivity(i);
    }

    public void resumeROM(View view) {
        // TODO check if emulation mode is still the same
        launchEmulator(true);
    }

    public String getRomTitle(Uri uri) {
        try {
            InputStream in = getApplication().getContentResolver().openInputStream(uri);

            int bufferSize = 0x145;
            byte[] buffer = new byte[bufferSize];

            int len = in.read(buffer);
            if (len == -1)
                return null;

            in.close();

            if (buffer[0x143] == (byte) 0x80 || buffer[0x143] == (byte) 0xC0)
                return new String(Arrays.copyOfRange(buffer, 0x134, 0x143)).trim();
            else
                return new String(Arrays.copyOfRange(buffer, 0x134, 0x144)).trim();
        } catch (IOException e) {
            e.printStackTrace();
            return null;
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (requestCode == 200 && resultCode == RESULT_OK && data != null) {
            Uri uri = data.getData();

            String romTitle = getRomTitle(uri);
            if (romTitle == null) {
                errorToast("Error reading file");
                filePickerIntent = null;
            }

            String extension = uri.getLastPathSegment();
            int dotIndex = extension.lastIndexOf('.');
            if (dotIndex < 0) {
                errorToast("The selected file is not a valid ROM.");
                filePickerIntent = null;
                return;
            }
            extension = extension.substring(dotIndex);
            if (!(extension.equals(".gb") || extension.equals(".cgb") || extension.equals(".gbc"))) {
                errorToast("The selected file is not a valid ROM.");
                filePickerIntent = null;
                return;
            }

            romUri = uri;
            // allow new file picker intents to be sent
            filePickerIntent = null;

            loadedROMTop.setVisibility(View.VISIBLE);
            loadedROMBottom.setText(romTitle);

            resumeButton.setEnabled(true);
            resumeButton.setCompoundDrawablesWithIntrinsicBounds(R.drawable.round_play_arrow_black_24dp, 0, 0, 0);
            resetButton.setEnabled(true);
            resetButton.setCompoundDrawablesWithIntrinsicBounds(R.drawable.round_refresh_black_24dp, 0, 0, 0);

            launchEmulator(false);
        } else {
            filePickerIntent = null;
        }
    }

    public void loadROM(View view) {
        if (filePickerIntent != null) // don't send a file picker intent while another is already being processed
            return;
        filePickerIntent = new Intent();
        filePickerIntent.setType("*/*");
        filePickerIntent.setAction(Intent.ACTION_OPEN_DOCUMENT);
        filePickerIntent.addCategory(Intent.CATEGORY_OPENABLE);
        startActivityForResult(filePickerIntent, 200);
    }

    public void linkCable(View view) {
        boolean bluetooth = ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH) == PackageManager.PERMISSION_GRANTED;
        boolean bluetoothAdmin = ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_ADMIN) == PackageManager.PERMISSION_GRANTED;
        if (bluetooth && bluetoothAdmin) {
            goToLinkMenu();
        } else {
            /*if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                // TODO request new bluetooth permissions (and test in emulator if this works)
            } else */if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                requestPermissions(new String[] { Manifest.permission.BLUETOOTH, Manifest.permission.BLUETOOTH_ADMIN }, 111);
            } else {
                Toast.makeText(MainMenu.this, "Can't use link cable without bluetooth permissions", Toast.LENGTH_SHORT).show();
                finish();
            }
        }
    }

    public void resetROM(View view) {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle(R.string.reset_rom_button);
        builder.setMessage(R.string.reset_rom_dialog_message);
        builder.setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialogInterface, int i) {
                launchEmulator(false);
            }
        });
        builder.setNegativeButton(android.R.string.cancel, null);
        builder.show();
    }

    public void openSettings(View view) {
        startActivity(new Intent(MainMenu.this, SettingsMenu.class));
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        switch (requestCode) {
            case 111:
                // If request is cancelled, the result arrays are empty.
                if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED && grantResults[1] == PackageManager.PERMISSION_GRANTED) {
                    // Permission is granted. Continue the action or workflow
                    // in your app.
                    goToLinkMenu();
                }  else {
                    // Explain to the user that the feature is unavailable because
                    // the features requires a permission that the user has denied.
                    // At the same time, respect the user's decision. Don't link to
                    // system settings in an effort to convince the user to change
                    // their decision.
                    Toast.makeText(MainMenu.this, "Can't use link cable without bluetooth permissions", Toast.LENGTH_SHORT).show();
                    finish();
                }
                return;
        }
        // Other 'case' lines to check for other
        // permissions this app might request.
    }

    public void goToLinkMenu() {
        BluetoothAdapter bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
        if (bluetoothAdapter == null) {
            Toast.makeText(MainMenu.this, "Bluetooth is not available", Toast.LENGTH_SHORT).show();
            return;
        }

        if (bluetoothAdapter.isEnabled()) {
            startActivityForResult(new Intent(MainMenu.this, LinkMenu.class), 222);
            return;
        }

        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle(R.string.link_button);
        builder.setMessage(R.string.link_dialog_message);
        builder.setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
            @SuppressLint("MissingPermission")
            @Override
            public void onClick(DialogInterface dialogInterface, int i) {
                bluetoothAdapter.enable();
                startActivity(new Intent(MainMenu.this, LinkMenu.class));
            }
        });
        builder.setNegativeButton(android.R.string.cancel, null);
        builder.show();
    }
    
}
