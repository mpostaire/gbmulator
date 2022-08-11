package io.github.mpostaire.gbmulator;

import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import android.view.MenuItem;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.SeekBar;
import android.widget.TextView;

import java.util.Objects;

public class SettingsMenu extends AppCompatActivity {

    LinearLayout settingModeButton;
    TextView settingModeTextView;

    LinearLayout settingPaletteButton;
    TextView settingPaletteTextView;

    SeekBar volumeSeekBar;
    TextView volumeLevelTextView;

    SeekBar speedSeekBar;
    TextView speedLevelTextView;

    SeekBar frameSkipSeekBar;
    TextView frameSkipTextView;

    PopupMenu modePopup;
    PopupMenu palettePopup;

    SharedPreferences preferences;
    SharedPreferences.Editor preferencesEditor;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_settings);
        Objects.requireNonNull(getSupportActionBar()).setDisplayHomeAsUpEnabled(true);

        preferences = getSharedPreferences(UserSettings.PREFERENCES, MODE_PRIVATE);
        preferencesEditor = preferences.edit();

        settingModeButton = findViewById(R.id.settingModeButton);
        settingModeTextView = findViewById(R.id.settingModeTextView);

        switch (preferences.getInt(UserSettings.EMULATION_MODE, UserSettings.EMULATION_MODE_DEFAULT)) {
            case UserSettings.EMULATION_MODE_DMG:
                settingModeTextView.setText(R.string.setting_mode_dmg);
                break;
            case UserSettings.EMULATION_MODE_CGB:
                settingModeTextView.setText(R.string.setting_mode_cgb);
                break;
        }

        settingPaletteButton = findViewById(R.id.settingColorPaletteButton);
        settingPaletteTextView = findViewById(R.id.settingPaletteTextView);

        switch (preferences.getInt(UserSettings.EMULATION_PALETTE, UserSettings.EMULATION_PALETTE_DEFAULT)) {
            case UserSettings.EMULATION_PALETTE_ORIG:
                settingPaletteTextView.setText(R.string.setting_palette_orig);
                break;
            case UserSettings.EMULATION_PALETTE_GRAY:
                settingPaletteTextView.setText(R.string.setting_palette_gray);
                break;
        }

        modePopup = new PopupMenu(this, settingModeTextView);
        modePopup.inflate(R.menu.setting_mode_popup_menu);
        modePopup.setOnMenuItemClickListener(new PopupMenu.OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem menuItem) {
                switch (menuItem.getItemId()) {
                    case R.id.popup_menu_mode_dmg:
                        settingModeTextView.setText(R.string.setting_mode_dmg);
                        preferencesEditor.putInt(UserSettings.EMULATION_MODE, UserSettings.EMULATION_MODE_DMG);
                        preferencesEditor.apply();
                        return true;
                    case R.id.popup_menu_mode_cgb:
                        settingModeTextView.setText(R.string.setting_mode_cgb);
                        preferencesEditor.putInt(UserSettings.EMULATION_MODE, UserSettings.EMULATION_MODE_CGB);
                        preferencesEditor.apply();
                        return true;
                    default:
                        return false;
                }
            }
        });

        palettePopup = new PopupMenu(this, settingPaletteTextView);
        palettePopup.inflate(R.menu.setting_palette_popup_menu);
        palettePopup.setOnMenuItemClickListener(new PopupMenu.OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem menuItem) {
                switch (menuItem.getItemId()) {
                    case R.id.popup_menu_palette_gray:
                        settingPaletteTextView.setText(R.string.setting_palette_gray);
                        preferencesEditor.putInt(UserSettings.EMULATION_PALETTE, UserSettings.EMULATION_PALETTE_GRAY);
                        preferencesEditor.apply();
                        return true;
                    case R.id.popup_menu_palette_orig:
                        settingPaletteTextView.setText(R.string.setting_palette_orig);
                        preferencesEditor.putInt(UserSettings.EMULATION_PALETTE, UserSettings.EMULATION_PALETTE_ORIG);
                        preferencesEditor.apply();
                        return true;
                    default:
                        return false;
                }
            }
        });

        int volumeProgress = (int) (preferences.getFloat(UserSettings.EMULATION_SOUND, UserSettings.EMULATION_SOUND_DEFAULT) * 100);
        volumeLevelTextView = findViewById(R.id.volumeLevelTextView);
        volumeLevelTextView.setText(getString(R.string.setting_volume_label, volumeProgress));
        volumeSeekBar = findViewById(R.id.volumeSeekBar);
        volumeSeekBar.setProgress(volumeProgress);
        volumeSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {
                preferencesEditor.putFloat(UserSettings.EMULATION_SOUND, i / 100.0f);
                preferencesEditor.apply();
                volumeLevelTextView.setText(getString(R.string.setting_volume_label, i));
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {

            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {

            }
        });

        float speed = preferences.getFloat(UserSettings.EMULATION_SPEED, UserSettings.EMULATION_SPEED_DEFAULT);
        int speedProgress = (int) ((speed - 1) / 0.5f);
        speedLevelTextView = findViewById(R.id.speedLevelTextView);
        speedLevelTextView.setText(getString(R.string.setting_speed_label, speed));
        speedSeekBar = findViewById(R.id.speedSeekBar);
        speedSeekBar.setProgress(speedProgress);
        speedSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {
                float value = i * 0.5f + 1;
                preferencesEditor.putFloat(UserSettings.EMULATION_SPEED, value);
                preferencesEditor.apply();
                speedLevelTextView.setText(getString(R.string.setting_speed_label, value));
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {

            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {

            }
        });

        int frameSkip = preferences.getInt(UserSettings.FRAME_SKIP, UserSettings.FRAME_SKIP_DEFAULT);
        frameSkipTextView = findViewById(R.id.frameSkipTextView);
        if (frameSkip == 0)
            frameSkipTextView.setText(getString(R.string.setting_frame_skip_value_off));
        else
            frameSkipTextView.setText(getString(R.string.setting_frame_skip_value, frameSkip));
        frameSkipSeekBar = findViewById(R.id.frameSkipSeekBar);
        frameSkipSeekBar.setProgress(frameSkip);
        frameSkipSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {
                preferencesEditor.putInt(UserSettings.FRAME_SKIP, i);
                preferencesEditor.apply();
                if (i == 0)
                    frameSkipTextView.setText(getString(R.string.setting_frame_skip_value_off));
                else
                    frameSkipTextView.setText(getString(R.string.setting_frame_skip_value, i));
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {

            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {

            }
        });
    }

    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            this.finish();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    public void showModePopup(View view) {
        modePopup.show();
    }

    public void showPalettePopup(View view) {
        palettePopup.show();
    }

    public void layoutEditor(View view) {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle(R.string.setting_layout_editor);
        builder.setMessage(R.string.setting_layout_editor_popup_label);
        builder.setPositiveButton(R.string.setting_layout_editor_popup_portrait_button, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialogInterface, int i) {
                Intent intent = new Intent(SettingsMenu.this, Emulator.class);
                intent.putExtra("layout_editor", 1);
                startActivity(intent);
            }
        });
        builder.setNegativeButton(R.string.setting_layout_editor_popup_landscape_button, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialogInterface, int i) {
                Intent intent = new Intent(SettingsMenu.this, Emulator.class);
                intent.putExtra("layout_editor", 2);
                startActivity(intent);
            }
        });
        builder.setNeutralButton(R.string.setting_layout_editor_popup_defaults_button, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialogInterface, int i) {
                preferencesEditor.putFloat(UserSettings.PORTRAIT_SCREEN_X, UserSettings.PORTRAIT_SCREEN_X_DEFAULT);
                preferencesEditor.putFloat(UserSettings.PORTRAIT_SCREEN_Y, UserSettings.PORTRAIT_SCREEN_X_DEFAULT);
                preferencesEditor.putFloat(UserSettings.PORTRAIT_SCREEN_SIZE, UserSettings.PORTRAIT_SCREEN_SIZE_DEFAULT);

                preferencesEditor.putFloat(UserSettings.LANDSCAPE_SCREEN_X, UserSettings.LANDSCAPE_SCREEN_X_DEFAULT);
                preferencesEditor.putFloat(UserSettings.LANDSCAPE_SCREEN_Y, UserSettings.LANDSCAPE_SCREEN_Y_DEFAULT);
                preferencesEditor.putFloat(UserSettings.LANDSCAPE_SCREEN_SIZE, UserSettings.LANDSCAPE_SCREEN_SIZE_DEFAULT);

                preferencesEditor.apply();
            }
        });
        builder.show();
    }

}
