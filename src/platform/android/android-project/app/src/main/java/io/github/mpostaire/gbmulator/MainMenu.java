package io.github.mpostaire.gbmulator;

import android.content.Intent;
import android.net.Uri;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;


public class MainMenu extends AppCompatActivity {

    Intent filePickerIntent = null;
    Uri romUri = null;
    TextView loadedROMName;
    Button resumeButton;
    Button resetButton;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        loadedROMName = findViewById(R.id.loadedROMName);
        resumeButton = findViewById(R.id.resumeButton);
        resetButton = findViewById(R.id.resetButton);
    }

    public void errorToast(String msg) {
        Toast.makeText(this, msg, Toast.LENGTH_SHORT).show();
    }

    public void launchEmulator(boolean resume) {
        if (romUri == null)
            return;
        Intent i = new Intent(MainMenu.this, Emulator.class);
        i.putExtra("rom_uri", romUri);
        i.putExtra("resume", resume);
        startActivity(i);
    }

    public void resumeROM(View view) {
        launchEmulator(true);
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

            // TODO instead of getting filename, get actual rom title using a native function that also
            //      checks if the rom is valid (create emulator_is_valid_rom(byte_t *rom_data, size_t rom_size) function)
            //      used by the mmu parse_cartridge() function
            String fileName = uri.getLastPathSegment();
            fileName = fileName.substring(fileName.lastIndexOf('/') + 1);
            loadedROMName.setText(fileName);

            resumeButton.setEnabled(true);
            resetButton.setEnabled(true);

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

    public void resetROM(View view) {
        launchEmulator(false);
    }

    public void openSettings(View view) {
        startActivity(new Intent(MainMenu.this, SettingsMenu.class));
    }
}
