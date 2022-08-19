package io.github.mpostaire.gbmulator;

import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import android.view.MenuItem;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import com.google.android.gms.auth.api.signin.GoogleSignIn;
import com.google.android.gms.auth.api.signin.GoogleSignInAccount;
import com.google.android.gms.auth.api.signin.GoogleSignInClient;
import com.google.android.gms.auth.api.signin.GoogleSignInOptions;
import com.google.android.gms.common.api.Scope;
import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;
import com.google.api.client.extensions.android.http.AndroidHttp;
import com.google.api.client.googleapis.extensions.android.gms.auth.GoogleAccountCredential;
import com.google.api.client.json.gson.GsonFactory;
import com.google.api.services.drive.Drive;
import com.google.api.services.drive.DriveScopes;
import com.google.api.services.drive.model.File;

import java.io.ByteArrayOutputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
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

    SeekBar opacitySeekBar;
    TextView opacityTextView;

    PopupMenu modePopup;
    PopupMenu palettePopup;

    SharedPreferences preferences;
    SharedPreferences.Editor preferencesEditor;

    DriveServiceHelper driveServiceHelper;
    String driveSaveDirId;
    ProgressDialog driveSyncProgressDialog;

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

        int opacity = (int) (preferences.getFloat(UserSettings.BUTTONS_OPACITY, UserSettings.BUTTONS_OPACITY_DEFAULT) * 100);
        opacityTextView = findViewById(R.id.opacityTextView);
        opacityTextView.setText(getString(R.string.setting_opacity_value, opacity));
        opacitySeekBar = findViewById(R.id.opacitySeekBar);
        opacitySeekBar.setProgress(opacity - 10);
        opacitySeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {
                preferencesEditor.putFloat(UserSettings.BUTTONS_OPACITY, (i + 10) / 100.0f);
                preferencesEditor.apply();
                opacityTextView.setText(getString(R.string.setting_opacity_value, i + 10));
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
                preferencesEditor.putFloat(UserSettings.PORTRAIT_DPAD_X, UserSettings.PORTRAIT_DPAD_X_DEFAULT);
                preferencesEditor.putFloat(UserSettings.PORTRAIT_DPAD_Y, UserSettings.PORTRAIT_DPAD_Y_DEFAULT);
                preferencesEditor.putFloat(UserSettings.PORTRAIT_A_X, UserSettings.PORTRAIT_A_X_DEFAULT);
                preferencesEditor.putFloat(UserSettings.PORTRAIT_A_Y, UserSettings.PORTRAIT_A_Y_DEFAULT);
                preferencesEditor.putFloat(UserSettings.PORTRAIT_B_X, UserSettings.PORTRAIT_B_X_DEFAULT);
                preferencesEditor.putFloat(UserSettings.PORTRAIT_B_Y, UserSettings.PORTRAIT_B_Y_DEFAULT);
                preferencesEditor.putFloat(UserSettings.PORTRAIT_START_X, UserSettings.PORTRAIT_START_X_DEFAULT);
                preferencesEditor.putFloat(UserSettings.PORTRAIT_START_Y, UserSettings.PORTRAIT_START_Y_DEFAULT);
                preferencesEditor.putFloat(UserSettings.PORTRAIT_SELECT_X, UserSettings.PORTRAIT_SELECT_X_DEFAULT);
                preferencesEditor.putFloat(UserSettings.PORTRAIT_SELECT_Y, UserSettings.PORTRAIT_SELECT_Y_DEFAULT);

                preferencesEditor.putFloat(UserSettings.LANDSCAPE_DPAD_X, UserSettings.LANDSCAPE_DPAD_X_DEFAULT);
                preferencesEditor.putFloat(UserSettings.LANDSCAPE_DPAD_Y, UserSettings.LANDSCAPE_DPAD_Y_DEFAULT);
                preferencesEditor.putFloat(UserSettings.LANDSCAPE_A_X, UserSettings.LANDSCAPE_A_X_DEFAULT);
                preferencesEditor.putFloat(UserSettings.LANDSCAPE_A_Y, UserSettings.LANDSCAPE_A_Y_DEFAULT);
                preferencesEditor.putFloat(UserSettings.LANDSCAPE_B_X, UserSettings.LANDSCAPE_B_X_DEFAULT);
                preferencesEditor.putFloat(UserSettings.LANDSCAPE_B_Y, UserSettings.LANDSCAPE_B_Y_DEFAULT);
                preferencesEditor.putFloat(UserSettings.LANDSCAPE_START_X, UserSettings.LANDSCAPE_START_X_DEFAULT);
                preferencesEditor.putFloat(UserSettings.LANDSCAPE_START_Y, UserSettings.LANDSCAPE_START_Y_DEFAULT);
                preferencesEditor.putFloat(UserSettings.LANDSCAPE_SELECT_X, UserSettings.LANDSCAPE_SELECT_X_DEFAULT);
                preferencesEditor.putFloat(UserSettings.LANDSCAPE_SELECT_Y, UserSettings.LANDSCAPE_SELECT_Y_DEFAULT);

                preferencesEditor.apply();
            }
        });
        builder.show();
    }

    public void signInDrive(View view) {
        driveSyncProgressDialog = new ProgressDialog(this);
        driveSyncProgressDialog.setTitle(getString(R.string.setting_drive_dialog_title));
        driveSyncProgressDialog.setMessage("Signing in your Google Drive account...");
        driveSyncProgressDialog.setProgressStyle(ProgressDialog.STYLE_SPINNER);
        driveSyncProgressDialog.setMax(1);
        driveSyncProgressDialog.setCancelable(false);
        driveSyncProgressDialog.setCanceledOnTouchOutside(false);
        driveSyncProgressDialog.show();

        GoogleSignInOptions signInOptions = new GoogleSignInOptions.Builder(GoogleSignInOptions.DEFAULT_SIGN_IN)
                .requestEmail()
                .requestScopes(new Scope(DriveScopes.DRIVE_FILE))
                .build();

        GoogleSignInClient client = GoogleSignIn.getClient(this, signInOptions);

        startActivityForResult(client.getSignInIntent(), 400);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        switch (requestCode) {
            case 400:
                if (resultCode == RESULT_OK) {
                    GoogleSignIn.getSignedInAccountFromIntent(data)
                            .addOnSuccessListener(new OnSuccessListener<GoogleSignInAccount>() {
                                @Override
                                public void onSuccess(GoogleSignInAccount googleSignInAccount) {
                                    GoogleAccountCredential credential = GoogleAccountCredential
                                            .usingOAuth2(SettingsMenu.this, Collections.singleton(DriveScopes.DRIVE_FILE));
                                    credential.setSelectedAccount(googleSignInAccount.getAccount());

                                    Drive driveService = new Drive.Builder(AndroidHttp.newCompatibleTransport(), new GsonFactory(), credential)
                                            .setApplicationName("GBmulator")
                                            .build();

                                    driveSyncSaves(driveService);
                                }
                            })
                            .addOnFailureListener(new OnFailureListener() {
                                @Override
                                public void onFailure(@NonNull Exception e) {
                                    e.printStackTrace();
                                    Toast.makeText(SettingsMenu.this, "Failed to connect to Google Drive", Toast.LENGTH_SHORT).show();
                                }
                            });
                }
                break;
        }
    }

    public void incrementProgressDialog() {
        driveSyncProgressDialog.incrementProgressBy(1);
        if (driveSyncProgressDialog.getProgress() >= driveSyncProgressDialog.getMax())
            driveSyncProgressDialog.dismiss();
    }

    public void decrementMaxProgressDialog() {
        driveSyncProgressDialog.setMax(driveSyncProgressDialog.getMax() - 1);
        if (driveSyncProgressDialog.getMax() <= 0 || driveSyncProgressDialog.getProgress() >= driveSyncProgressDialog.getMax())
            driveSyncProgressDialog.dismiss();
    }

    private void driveSyncSaves(Drive driveService) {
        driveSyncProgressDialog.setMessage("Fetching remote saves...");

        driveServiceHelper = new DriveServiceHelper(driveService);
        driveServiceHelper.getSaveDirId().addOnSuccessListener(new OnSuccessListener<String>() {
            @Override
            public void onSuccess(String dirId) {
                driveSaveDirId = dirId;
                if (driveSaveDirId == null) {
                    driveCompare(new File[] {});
                    return;
                }
                driveServiceHelper.listSaveFiles(driveSaveDirId).addOnSuccessListener(new OnSuccessListener<List<File>>() {
                    @Override
                    public void onSuccess(List<File> files) {
                        driveCompare(files.toArray(new File[] {}));
                    }
                }).addOnFailureListener(new OnFailureListener() {
                    @Override
                    public void onFailure(@NonNull Exception e) {
                        driveCompare(new File[] {});
                    }
                });
            }
        }).addOnFailureListener(new OnFailureListener() {
            @Override
            public void onFailure(@NonNull Exception e) {
                driveCompare(new File[] {});
            }
        });
    }

    public void driveCompare(File[] distantFiles) {
        java.io.File localSaveDir = new java.io.File(getFilesDir().getAbsolutePath());
        java.io.File[] localSaveFiles = localSaveDir.listFiles();
        ArrayList<java.io.File> toUpload = new ArrayList<>();
        ArrayList<File> toUpdate = new ArrayList<>();
        ArrayList<String> toUpdateLocalPath = new ArrayList<>();
        ArrayList<File> toDownload = new ArrayList<>();
        if (localSaveFiles != null) {
            for (java.io.File localFile : localSaveFiles) {
                if (localFile.getName().equals("resume"))
                    continue;
                boolean found = false;
                for (File distantFile : distantFiles) {
                    if (localFile.getName().equals(distantFile.getName())) {
                        found = true;
                        if (localFile.lastModified() > distantFile.getModifiedTime().getValue()) {
                            toUpdate.add(distantFile);
                            toUpdateLocalPath.add(localFile.getAbsolutePath());
                        } else {
                            toDownload.add(distantFile);
                        }
                        break;
                    }
                }

                // add to upload list the local save files that are not present in the distant save directory
                if (!found)
                    toUpload.add(localFile);
            }
        }

        // add to download list the distant save files that are not present in the local save directory
        for (File distantFile : distantFiles) {
            boolean found = false;
            if (localSaveFiles != null) {
                for (java.io.File localFile : localSaveFiles) {
                    if (localFile.getName().equals("resume"))
                        continue;
                    if (distantFile.getName().equals(localFile.getName())) {
                        found = true;
                        break;
                    }
                }
            }

            if (!found)
                toDownload.add(distantFile);
        }

        /*for (File f : toDownload)
            Log.i("GBmulator", "TO DOWNLOAD: " + f.getName());
        for (java.io.File f : toUpload)
            Log.i("GBmulator", "TO UPLOAD: " + f.getName());
        for (File f : toUpdate)
            Log.i("GBmulator", "TO UPDATE: " + f.getName());

        if (toDownload.isEmpty())
            Log.i("GBmulator", "TO DOWNLOAD EMPTY");
        if (toUpload.isEmpty())
            Log.i("GBmulator", "TO UPLOAD EMPTY");
        if (toUpdate.isEmpty())
            Log.i("GBmulator", "TO UPDATE EMPTY");*/

        driveSyncProgressDialog.setMessage("Syncing saves...");
        driveSyncProgressDialog.setMax(toDownload.size() + toUpload.size() + toUpdate.size());

        // Todo download, then upload, then update (and do it all at once in a single call to drive service helper)

        driveDownloadSaveFiles(toDownload);
        driveUploadSaveFiles(toUpload);
        driveUpdateSaveFiles(toUpdateLocalPath, toUpdate);
    }

    public void uploadSaveFilesHelper(ArrayList<java.io.File> toUpload, String parentDirId) {
        for (java.io.File f : toUpload) {
            driveServiceHelper.uploadSaveFile(f.getAbsolutePath(), parentDirId).addOnSuccessListener(new OnSuccessListener<String>() {
                @Override
                public void onSuccess(String s) {
                    incrementProgressDialog();
                }
            }).addOnFailureListener(new OnFailureListener() {
                @Override
                public void onFailure(@NonNull Exception e) {
                    e.printStackTrace();
                    decrementMaxProgressDialog();
                    Toast.makeText(SettingsMenu.this, "Failed to upload the save file: " + f.getName(), Toast.LENGTH_SHORT).show();
                }
            });
        }
    }

    public void driveUploadSaveFiles(ArrayList<java.io.File> toUpload) {
        if (driveSaveDirId == null) {
            driveServiceHelper.createSaveDir().addOnSuccessListener(new OnSuccessListener<String>() {
                @Override
                public void onSuccess(String parentDirId) {
                    uploadSaveFilesHelper(toUpload, parentDirId);
                }
            }).addOnFailureListener(new OnFailureListener() {
                @Override
                public void onFailure(@NonNull Exception e) {
                    e.printStackTrace();
                    decrementMaxProgressDialog();
                    Toast.makeText(SettingsMenu.this, "Failed to create the save directory", Toast.LENGTH_SHORT).show();
                }
            });
        } else {
            uploadSaveFilesHelper(toUpload, driveSaveDirId);
        }
    }

    public void driveUpdateSaveFiles(ArrayList<String> toUpdateLocalPath, ArrayList<File> toUpdate) {
        for (int i = 0; i < toUpdate.size(); i++) {
            int finalI = i;
            driveServiceHelper.updateSaveFile(toUpdateLocalPath.get(i), toUpdate.get(i).getId()).addOnSuccessListener(new OnSuccessListener<String>() {
                @Override
                public void onSuccess(String s) {
                    incrementProgressDialog();
                }
            }).addOnFailureListener(new OnFailureListener() {
                @Override
                public void onFailure(@NonNull Exception e) {
                    e.printStackTrace();
                    decrementMaxProgressDialog();
                    Toast.makeText(SettingsMenu.this, "Failed to update the save file: " + toUpdate.get(finalI).getName(), Toast.LENGTH_SHORT).show();
                }
            });
        }
    }

    public void driveDownloadSaveFiles(ArrayList<File> toDownload) {
        for (File f : toDownload) {
            driveServiceHelper.downloadSaveFile(f.getId()).addOnSuccessListener(new OnSuccessListener<ByteArrayOutputStream>() {
                @Override
                public void onSuccess(ByteArrayOutputStream byteArrayOutputStream) {
                    FileOutputStream fileOutputStream = null;
                    try {
                        fileOutputStream = new FileOutputStream(getFilesDir() + "/" + f.getName());
                    } catch (FileNotFoundException e) {
                        e.printStackTrace();
                        decrementMaxProgressDialog();
                        Toast.makeText(SettingsMenu.this, "Failed to download the save file: " + f.getName(), Toast.LENGTH_SHORT).show();
                        return;
                    }
                    try {
                        byteArrayOutputStream.writeTo(fileOutputStream);
                    } catch (IOException e) {
                        e.printStackTrace();
                        decrementMaxProgressDialog();
                        Toast.makeText(SettingsMenu.this, "Failed to download the save file: " + f.getName(), Toast.LENGTH_SHORT).show();
                        return;
                    }
                    incrementProgressDialog();
                }
            }).addOnFailureListener(new OnFailureListener() {
                @Override
                public void onFailure(@NonNull Exception e) {
                    e.printStackTrace();
                    decrementMaxProgressDialog();
                    Toast.makeText(SettingsMenu.this, "Failed to download the save file: " + f.getName(), Toast.LENGTH_SHORT).show();
                }
            });
        }
    }

}
