package io.github.mpostaire.gbmulator;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.bluetooth.BluetoothClass;
import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import java.util.ArrayList;

public class BluetoothDeviceAdapter extends BaseAdapter {

    private final ArrayList<BluetoothDevice> listData;
    private final LayoutInflater layoutInflater;
    Context context;

    public BluetoothDeviceAdapter(Context context) {
        this.listData = new ArrayList<>();
        this.context = context;
        layoutInflater = LayoutInflater.from(context);
    }

    @Override
    public int getCount() {
        return listData.size();
    }

    @Override
    public BluetoothDevice getItem(int position) {
        return listData.get(position);
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    public void clear() {
        listData.clear();
    }

    @SuppressLint("MissingPermission")
    public void add(BluetoothDevice device) {
        listData.add(device);
    }

    @SuppressLint("MissingPermission")
    public View getView(int position, View convertView, ViewGroup parent) {
        ViewHolder holder;
        if (convertView == null) {
            convertView = layoutInflater.inflate(R.layout.list_item, null);
            holder = new ViewHolder();
            holder.deviceIcon = convertView.findViewById(R.id.icon);
            holder.deviceName = convertView.findViewById(R.id.topText);
            holder.deviceType = convertView.findViewById(R.id.bottomText);
            convertView.setTag(holder);
        } else {
            holder = (ViewHolder) convertView.getTag();
        }

        BluetoothDevice device = listData.get(position);
        holder.deviceIcon.setImageDrawable(context.getResources().getDrawable(deviceClassToDrawableRes(device.getBluetoothClass().getDeviceClass())));
        holder.deviceName.setText(device.getName());
        holder.deviceType.setText(device.getAddress());

        return convertView;
    }

    private String deviceClassToString(int deviceClass) {
        switch (deviceClass) {
            case BluetoothClass.Device.PHONE_SMART:
                return "Phone";
            case BluetoothClass.Device.COMPUTER_DESKTOP:
                return "Desktop";
            case BluetoothClass.Device.COMPUTER_LAPTOP:
                return "Laptop";
            default:
                return "Unknown";
        }
    }

    private int deviceClassToDrawableRes(int deviceClass) {
        switch (deviceClass) {
            case BluetoothClass.Device.PHONE_SMART:
                return R.drawable.round_smartphone_black_36dp;
            case BluetoothClass.Device.COMPUTER_DESKTOP:
                return R.drawable.round_desktop_windows_black_36dp;
            case BluetoothClass.Device.COMPUTER_LAPTOP:
                return R.drawable.round_laptop_black_36dp;
            default:
                return R.drawable.round_device_unknown_black_36dp;
        }
    }

    static class ViewHolder {
        ImageView deviceIcon;
        TextView deviceName;
        TextView deviceType;
    }

}