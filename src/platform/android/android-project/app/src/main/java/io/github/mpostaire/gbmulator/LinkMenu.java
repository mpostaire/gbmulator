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
import android.support.annotation.NonNull;
import android.support.v7.app.AppCompatActivity;
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

    private BluetoothAdapter bluetoothAdapter;
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

        bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();

        BroadcastReceiver broadcastReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                String action = intent.getAction();

                if (BluetoothAdapter.ACTION_STATE_CHANGED.equals(action)) {
                    int state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, -1);

                    switch (state) {
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
        Set<BluetoothDevice> pairedDevices = bluetoothAdapter.getBondedDevices();
        if (pairedDevices.size() == 0) {
            status.setVisibility(View.VISIBLE);
            deviceListContainer.setVisibility(View.GONE);
            if (bluetoothAdapter.isEnabled())
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