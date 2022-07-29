package io.github.mpostaire.gbmulator;

import android.content.SharedPreferences;
import android.support.annotation.NonNull;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.TextView;

import java.util.Objects;

public class SettingsMenu extends AppCompatActivity {

    LinearLayout settingModeButton;
    TextView settingModeTextView;

    LinearLayout settingPaletteButton;
    TextView settingPaletteTextView;

    PopupMenu modePopup;
    PopupMenu palettePopup;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_settings_menu);
        Objects.requireNonNull(getSupportActionBar()).setDisplayHomeAsUpEnabled(true);

        SharedPreferences preferences = getSharedPreferences(UserSettings.PREFERENCES, MODE_PRIVATE);

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
                SharedPreferences.Editor preferencesEditor = preferences.edit();

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
                SharedPreferences.Editor preferencesEditor = preferences.edit();

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
}