package io.github.mpostaire.gbmulator;

import android.annotation.SuppressLint;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothClass;
import android.bluetooth.BluetoothDevice;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import java.util.Objects;
import java.util.Set;

public class LinkMenu extends AppCompatActivity {

    private BluetoothAdapter bluetoothManager;
    BluetoothDeviceAdapter listAdapter;

    TextView status;
    LinearLayout deviceListContainer;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_link_menu);
        Objects.requireNonNull(getSupportActionBar()).setDisplayHomeAsUpEnabled(true);

        status = findViewById(R.id.status);
        deviceListContainer = findViewById(R.id.deviceListContainer);
        listAdapter = new BluetoothDeviceAdapter(this);

        ListView listView = findViewById(R.id.deviceList);
        listView.setAdapter(listAdapter);
        listView.setOnItemClickListener(new AdapterView.OnItemClickListener(){
            @SuppressLint("MissingPermission")
            @Override
            public void onItemClick(AdapterView<?> adapterView, View view, int i, long l) {
                BluetoothDevice device = listAdapter.getItem(i);
                Toast.makeText(LinkMenu.this, "TODO connect to: " + device.getName(), Toast.LENGTH_SHORT).show();
            }
        });

        // TODO ALL OF THIS MAY BE USELESS...
        //  we can let android do the connection logic an here we only get the current bluetooth connected device(s)
        //  to start the communication
        //  https://developer.android.com/guide/topics/connectivity/bluetooth.html
        //  https://developer.android.com/guide/topics/connectivity/bluetooth/permissions#availability
        //  https://stackoverflow.com/questions/33383891/currently-connected-bluetooth-device-android

        //bluetoothManager = (BluetoothManager) getSystemService(BLUETOOTH_SERVICE);
        bluetoothManager = BluetoothAdapter.getDefaultAdapter();

        // TODO show 2 listViews (paired and unpaired) to allow to pair directly in this activity
        //      --> need to handle sdk level >= S and its permissions
        BroadcastReceiver broadcastReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                String action = intent.getAction();

                // TODO also update list when new device is paired/unpaired
                if (action.equals(BluetoothAdapter.ACTION_STATE_CHANGED)) {
                    switch (intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, -1)) {
                        case BluetoothAdapter.STATE_OFF:
                        case BluetoothAdapter.STATE_ON:
                            populateBluetoothDevicesList();
                            break;
                    }
                }
            }
        };
        registerReceiver(broadcastReceiver, new IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED));

        populateBluetoothDevicesList();
    }

    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            this.finish();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @SuppressLint("MissingPermission")
    public void populateBluetoothDevicesList() {
        Set<BluetoothDevice> pairedDevices = bluetoothManager.getBondedDevices();
        if (pairedDevices.size() == 0) {
            status.setVisibility(View.VISIBLE);
            deviceListContainer.setVisibility(View.GONE);
            if (bluetoothManager.isEnabled())
                status.setText(R.string.link_menu_no_devices);
            else
                status.setText(R.string.link_menu_no_bluetooth);
        } else {
            status.setVisibility(View.GONE);
            deviceListContainer.setVisibility(View.VISIBLE);
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

}