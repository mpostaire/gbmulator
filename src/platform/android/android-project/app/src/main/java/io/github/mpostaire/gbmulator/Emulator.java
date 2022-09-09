package io.github.mpostaire.gbmulator;

import android.annotation.SuppressLint;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothClass;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.database.ContentObserver;
import android.net.Uri;
import java.io.InputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.Set;

import android.os.Bundle;
import android.os.Handler;
import android.os.ParcelFileDescriptor;
import android.provider.Settings;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.constraintlayout.widget.ConstraintLayout;

import com.google.api.services.drive.model.User;

import org.libsdl.app.SDLActivity;

import io.github.mpostaire.gbmulator.bluetooth.BluetoothDeviceAdapter;
import io.github.mpostaire.gbmulator.tcp.TCPClientConnectionThread;
import io.github.mpostaire.gbmulator.tcp.TCPConnectionCallback;
import io.github.mpostaire.gbmulator.tcp.TCPServerConnectionThread;

public class Emulator extends SDLActivity {

    SharedPreferences preferences;
    SharedPreferences.Editor preferencesEditor;

    BluetoothAdapter bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
    BluetoothSocket bluetoothSocket;

    AlertDialog selectlinkModeDialog;
    AlertDialog wifiServerDialog;
    AlertDialog wifiClientDialog;
    AlertDialog bluetoothClientDialog;
    AlertDialog bluetoothEnableDialog;
    ProgressDialog bluetoothEnablingDialog;
    ProgressDialog connectingDialog;

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
    float portraitLinkX, portraitLinkY;

    float landscapeDpadX, landscapeDpadY;
    float landscapeAX, landscapeAY;
    float landscapeBX, landscapeBY;
    float landscapeStartX, landscapeStartY;
    float landscapeSelectX, landscapeSelectY;
    float landscapeLinkX, landscapeLinkY;

    public native void receiveROMData(
        byte[] data, int size,
        boolean resume, int emu_mode, int palette, float emu_speed, float sound, int emu_frame_skip,
        float buttons_opacity,
        float portraitDpadX, float portraitDpadY,
        float portraitAX, float portraitAY,
        float portraitBX, float portraitBY,
        float portraitStartX, float portraitStartY,
        float portraitSelectX, float portraitSelectY,
        float portraitLinkX, float portraitLinkY,
        float landscapeDpadX, float landscapeDpadY,
        float landscapeAX, float landscapeAY,
        float landscapeBX, float landscapeBY,
        float landscapeStartX, float landscapeStartY,
        float landscapeSelectX, float landscapeSelectY,
        float landscapeLinkX, float landscapeLinkY);

    public native void enterLayoutEditor(float buttons_opacity,
        boolean is_landscape,
        float dpad_x, float dpad_y,
        float a_x, float a_y,
        float b_x, float b_y,
        float start_x, float start_y,
        float select_x, float select_y,
        float link_x, float link_y);

    public native void tcpLinkReady(int sfd);

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

        bluetoothSocket = ((GBmulatorApp) getApplication()).bluetoothSocket;

        preferences = getSharedPreferences(UserSettings.PREFERENCES, MODE_PRIVATE);
        preferencesEditor = preferences.edit();

        initDialogs();

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
        portraitLinkX = preferences.getFloat(UserSettings.PORTRAIT_LINK_X, UserSettings.PORTRAIT_LINK_X_DEFAULT);
        portraitLinkY = preferences.getFloat(UserSettings.PORTRAIT_LINK_Y, UserSettings.PORTRAIT_LINK_Y_DEFAULT);

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
        landscapeLinkX = preferences.getFloat(UserSettings.LANDSCAPE_LINK_X, UserSettings.LANDSCAPE_LINK_X_DEFAULT);
        landscapeLinkY = preferences.getFloat(UserSettings.LANDSCAPE_LINK_Y, UserSettings.LANDSCAPE_LINK_Y_DEFAULT);

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

    @SuppressLint("MissingPermission")
    public void initDialogs() {
        final String[] ip = { preferences.getString(UserSettings.LAST_IP, UserSettings.LAST_IP_DEFAULT) };
        String portString = preferences.getString(UserSettings.LAST_PORT, UserSettings.LAST_PORT_DEFAULT);
        final int[] port = { portString.equals(UserSettings.LAST_PORT_DEFAULT) ? 0 : Integer.parseInt(portString) };

        connectingDialog = new ProgressDialog(Emulator.this);
        connectingDialog.setProgressStyle(ProgressDialog.STYLE_SPINNER);
        connectingDialog.setCancelable(false);
        connectingDialog.setCanceledOnTouchOutside(false);
        connectingDialog.setButton(DialogInterface.BUTTON_NEGATIVE, getText(android.R.string.cancel), (dialogInterface, i) -> dialogInterface.cancel());
        connectingDialog.setOnCancelListener(dialogInterface -> {
            errorToast("Connection cancelled");
            synchronized (messageboxSelection) {
                messageboxSelection.notify();
            }
        });

        TCPServerConnectionThread serverConnectionThread = new TCPServerConnectionThread(new TCPConnectionCallback() {
            @Override
            public void run() {
                GBmulatorApp app = (GBmulatorApp) getApplication();
                app.tcpSocket = ParcelFileDescriptor.fromSocket(getSocket()).getFd();
                Toast.makeText(Emulator.this, "Connected to client", Toast.LENGTH_SHORT).show();
                connectingDialog.dismiss();
            }
        });

        LinearLayout wifiServerLayout = (LinearLayout) getLayoutInflater().inflate(R.layout.wifi_server_dialog, null);
        EditText serverInputPort = wifiServerLayout.findViewById(R.id.editTextPort);
        serverInputPort.setText(portString);
        wifiServerDialog = new AlertDialog.Builder(Emulator.this)
                .setTitle("WiFi server")
                .setView(wifiServerLayout)
                .setCancelable(false)
                .setPositiveButton(android.R.string.ok, (dialogInterface, i) -> {
                    try {
                        port[0] = Integer.parseInt(serverInputPort.getText().toString());
                        if (port[0] > 65535) {
                            errorToast("Port number must be <= 65535");
                            dialogInterface.cancel();
                        } else {
                            dialogInterface.dismiss();
                            connectingDialog.setTitle("WiFi server");
                            connectingDialog.setMessage("Waiting for a client connection...");
                            connectingDialog.setOnDismissListener(dialogInterface1 -> {
                                synchronized (messageboxSelection) {
                                    messageboxSelection.notify();
                                    tcpLinkReady(((GBmulatorApp) getApplication()).tcpSocket);
                                }
                            });
                            connectingDialog.show();

                            preferencesEditor.putString(UserSettings.LAST_PORT, Integer.toString(port[0]));
                            preferencesEditor.apply();
                            serverConnectionThread.port = port[0];
                            serverConnectionThread.start();
                        }
                    } catch (NumberFormatException e) {
                        errorToast("Invalid port number");
                        dialogInterface.cancel();
                    }
                })
                .setNegativeButton(android.R.string.cancel, (dialogInterface, i) -> dialogInterface.cancel())
                .setOnCancelListener(dialogInterface -> {
                    serverConnectionThread.cancel();
                    synchronized (messageboxSelection) {
                        messageboxSelection.notify();
                    }
                })
                .create();

        TCPClientConnectionThread clientConnectionThread = new TCPClientConnectionThread(new TCPConnectionCallback() {
            @Override
            public void run() {
                GBmulatorApp app = (GBmulatorApp) getApplication();
                app.tcpSocket = ParcelFileDescriptor.fromSocket(getSocket()).getFd();
                Toast.makeText(Emulator.this, "Connected to server", Toast.LENGTH_SHORT).show();
                connectingDialog.dismiss();
            }
        });

        LinearLayout wifiClientLayout = (LinearLayout) getLayoutInflater().inflate(R.layout.wifi_client_dialog, null);
        EditText clientInputIP = wifiClientLayout.findViewById(R.id.editTextIP);
        EditText clientInputPort = wifiClientLayout.findViewById(R.id.editTextPort);
        clientInputIP.setText(ip[0]);
        clientInputPort.setText(portString);
        wifiClientDialog = new AlertDialog.Builder(Emulator.this)
                .setTitle("WiFi client")
                .setView(wifiClientLayout)
                .setCancelable(false)
                .setPositiveButton(android.R.string.ok, (dialogInterface, i) -> {
                    ip[0] = clientInputIP.getText().toString();
                    if (ip[0].equals("")) {
                        errorToast("Invalid IP address");
                        dialogInterface.cancel();
                        return;
                    }
                    try {
                        port[0] = Integer.parseInt(clientInputPort.getText().toString());
                        if (port[0] > 65535) {
                            errorToast("Port number must be <= 65535");
                            dialogInterface.cancel();
                        } else {
                            connectingDialog.setTitle("WiFi client");
                            connectingDialog.setMessage("Connecting to the server...");
                            connectingDialog.setOnDismissListener(dialogInterface1 -> {
                                synchronized (messageboxSelection) {
                                    messageboxSelection.notify();
                                    tcpLinkReady(((GBmulatorApp) getApplication()).tcpSocket);
                                }
                            });
                            connectingDialog.show();

                            clientConnectionThread.port = port[0];
                            clientConnectionThread.ipAddress = ip[0];
                            preferencesEditor.putString(UserSettings.LAST_PORT, Integer.toString(port[0]));
                            preferencesEditor.putString(UserSettings.LAST_IP, ip[0]);
                            preferencesEditor.apply();
                            clientConnectionThread.start();
                        }
                    } catch (NumberFormatException e) {
                        errorToast("Invalid port number");
                        dialogInterface.cancel();
                    }
                })
                .setNegativeButton(android.R.string.cancel, (dialogInterface, i) -> dialogInterface.cancel())
                .setOnCancelListener(dialogInterface -> {
                    synchronized (messageboxSelection) {
                        messageboxSelection.notify();
                    }
                })
                .create();

        final boolean[] showBluetoothClientDialog = { false };
        if (bluetoothAdapter != null) {
            ConstraintLayout deviceListContainer = (ConstraintLayout) getLayoutInflater().inflate(R.layout.bluetooth_client_dialog, null);
            BluetoothDeviceAdapter listAdapter = new BluetoothDeviceAdapter(Emulator.this);
            ListView deviceList = deviceListContainer.findViewById(R.id.deviceList);
            deviceList.setAdapter(listAdapter);
            deviceList.setOnItemClickListener((adapterView, view, i, l) -> {
                errorToast("TODO connect to " + listAdapter.getItem(i).getName());
                bluetoothClientDialog.cancel();
            });

            bluetoothClientDialog = new AlertDialog.Builder(Emulator.this)
                    .setTitle("Bluetooth client")
                    .setView(deviceListContainer)
                    .setCancelable(false)
                    .setNegativeButton(android.R.string.cancel, (dialogInterface, i) -> dialogInterface.cancel())
                    .setOnCancelListener(dialogInterface -> {
                        synchronized (messageboxSelection) {
                            messageboxSelection.notify();
                        }
                    })
                    .create();
            bluetoothClientDialog.setOnShowListener(dialogInterface -> populateBluetoothDevicesList(deviceListContainer, listAdapter));

            bluetoothEnablingDialog = new ProgressDialog(Emulator.this);
            bluetoothEnablingDialog.setMessage("Enabling Bluetooth...");
            bluetoothEnablingDialog.setProgressStyle(ProgressDialog.STYLE_SPINNER);
            bluetoothEnablingDialog.setCancelable(false);
            bluetoothEnablingDialog.setCanceledOnTouchOutside(false);

            bluetoothEnableDialog = new AlertDialog.Builder(Emulator.this)
                    .setTitle("Enable Bluetooth")
                    .setMessage(R.string.link_dialog_message)
                    .setCancelable(false)
                    .setPositiveButton(android.R.string.ok, (dialogInterface, i) -> {
                        bluetoothAdapter.enable();
                        if (showBluetoothClientDialog[0])
                            bluetoothEnablingDialog.setTitle("Bluetooth client");
                        else
                            bluetoothEnablingDialog.setTitle("Bluetooth server");

                        bluetoothEnablingDialog.show();
                        registerReceiver(new BroadcastReceiver() {
                            @Override
                            public void onReceive(Context context, Intent intent) {
                                String action = intent.getAction();

                                // TODO also update list when new device is paired/unpaired
                                if (action.equals(BluetoothAdapter.ACTION_STATE_CHANGED)) {
                                    switch (intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, -1)) {
                                        case BluetoothAdapter.STATE_ON:
                                            bluetoothEnablingDialog.cancel();
                                            if (showBluetoothClientDialog[0]) {
                                                bluetoothClientDialog.show();
                                            } else {
                                                connectingDialog.setTitle("Bluetooth server");
                                                connectingDialog.setMessage("Waiting for a client connection...");
                                                connectingDialog.show();
                                            }
                                            break;
                                    }
                                }
                            }
                        }, new IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED));
                    })
                    .setNegativeButton(android.R.string.cancel, (dialogInterface, i) -> dialogInterface.cancel())
                    .setOnCancelListener(dialogInterface -> {
                        errorToast("Bluetooth must be enabled to continue");
                        synchronized (messageboxSelection) {
                            messageboxSelection.notify();
                        }
                    })
                    .create();
        }

        String[] modes;
        if (bluetoothAdapter == null)
            modes = new String[] { "WiFi server", "WiFi client" };
        else
            modes = new String[] { "WiFi server", "WiFi client", "Bluetooth server", "Bluetooth client" };
        final int[] selected = { 0 };
        selectlinkModeDialog = new AlertDialog.Builder(Emulator.this)
                .setTitle("Link mode")
                .setCancelable(false)
                .setSingleChoiceItems(modes, selected[0], (dialogInterface, i) -> selected[0] = i)
                .setPositiveButton(android.R.string.ok, (dialogInterface, i) -> {
                    switch (selected[0]) {
                        case 0:
                            wifiServerDialog.show();
                            break;
                        case 1:
                            wifiClientDialog.show();
                            break;
                        case 2:
                            showBluetoothClientDialog[0] = false;
                            if (bluetoothAdapter.isEnabled()) {
                                connectingDialog.setTitle("Bluetooth server");
                                connectingDialog.setMessage("Waiting for a client connection...");
                                connectingDialog.show();
                            } else {
                                bluetoothEnableDialog.show();
                            }
                            break;
                        case 3:
                            // TODO listen bluetooth code
                            showBluetoothClientDialog[0] = true;
                            if (bluetoothAdapter.isEnabled())
                                bluetoothClientDialog.show();
                            else
                                bluetoothEnableDialog.show();
                            break;
                    }
                })
                .setNegativeButton(android.R.string.cancel, (dialogInterface, i) -> dialogInterface.cancel())
                .setOnCancelListener(dialogInterface -> {
                    synchronized (messageboxSelection) {
                        messageboxSelection.notify();
                    }
                })
                .create();
    }

    @SuppressLint("MissingPermission")
    public void populateBluetoothDevicesList(View container, BluetoothDeviceAdapter listAdapter) {
        Set<BluetoothDevice> pairedDevices = bluetoothAdapter.getBondedDevices();
        ListView deviceList = container.findViewById(R.id.deviceList);
        TextView status = container.findViewById(R.id.status);
        if (pairedDevices.size() == 0) {
            status.setVisibility(View.VISIBLE);
            deviceList.setVisibility(View.GONE);
            status.setText(R.string.link_menu_no_devices);
        } else {
            status.setVisibility(View.GONE);
            deviceList.setVisibility(View.VISIBLE);
        }

        listAdapter.clear();
        for (BluetoothDevice device : pairedDevices)
            if (isAllowedDeviceClass(device.getBluetoothClass().getDeviceClass()))
                listAdapter.add(device);
        listAdapter.notifyDataSetChanged();
    }

    private boolean isAllowedDeviceClass(int deviceClass) {
        switch (deviceClass) {
            case BluetoothClass.Device.PHONE_SMART:
            case BluetoothClass.Device.COMPUTER_DESKTOP:
            case BluetoothClass.Device.COMPUTER_LAPTOP:
                return true;
            default:
                return false;
        }
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
                        portraitSelectX, portraitSelectY,
                        portraitLinkX, portraitLinkY);
                break;
            case 2:
                enterLayoutEditor(buttonOpacity, true,
                        landscapeDpadX, landscapeDpadY,
                        landscapeAX, landscapeAY,
                        landscapeBX, landscapeBY,
                        landscapeStartX, landscapeStartY,
                        landscapeSelectX, landscapeSelectY,
                        landscapeLinkX, landscapeLinkY);
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
                    buttonOpacity,
                    portraitDpadX, portraitDpadY, portraitAX, portraitAY, portraitBX, portraitBY, portraitStartX, portraitStartY, portraitSelectX, portraitSelectY, portraitLinkX, portraitLinkY,
                    landscapeDpadX, landscapeDpadY, landscapeAX, landscapeAY, landscapeBX, landscapeBY, landscapeStartX, landscapeStartY, landscapeSelectX, landscapeSelectY, landscapeLinkX, landscapeLinkY);
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
            float startX, float startY,
            float linkX, float linkY) {
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
        preferencesEditor.putFloat(isLandscape ? UserSettings.LANDSCAPE_LINK_X : UserSettings.PORTRAIT_LINK_X, linkX);
        preferencesEditor.putFloat(isLandscape ? UserSettings.LANDSCAPE_LINK_Y : UserSettings.PORTRAIT_LINK_Y, linkY);

        preferencesEditor.apply();
    }

    public void linkMenu() {
        runOnUiThread(() -> selectlinkModeDialog.show());

        // block the calling thread
        synchronized (messageboxSelection) {
            try {
                messageboxSelection.wait();
            } catch (InterruptedException ex) {
                ex.printStackTrace();
            }
        }

    }

    public int linkSendData(byte[] data) {
        try {
            bluetoothSocket.getOutputStream().write(data);
            return data.length;
        } catch (IOException e) {
            e.printStackTrace();
            return -1;
        }
    }

    public byte[] linkReceiveData(int size) {
        byte[] buf = new byte[size];
        try {
            bluetoothSocket.getInputStream().read(buf);
        } catch (IOException e) {
            e.printStackTrace();
            return null;
        }
        return buf;
    }

}
