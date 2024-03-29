package io.github.mpostaire.gbmulator;

public class UserSettings {

    public static final String PREFERENCES = "preferences";

    public static final String EMULATION_MODE = "emulation_mode";
    public static final int EMULATION_MODE_DMG = 1;
    public static final int EMULATION_MODE_CGB = 2;
    public static final int EMULATION_MODE_DEFAULT = EMULATION_MODE_CGB;

    public static final String EMULATION_PALETTE = "emulation_palette";
    public static final int EMULATION_PALETTE_GRAY = 0;
    public static final int EMULATION_PALETTE_ORIG = 1;
    public static final int EMULATION_PALETTE_DEFAULT = EMULATION_PALETTE_GRAY;

    public static final String EMULATION_SPEED = "emulation_speed";
    public static final float EMULATION_SPEED_DEFAULT = 1.0f;

    public static final String EMULATION_SOUND = "emulation_sound";
    public static final float EMULATION_SOUND_DEFAULT = 1.0f;

    public static final String FRAME_SKIP = "frame_skip";
    public static final int FRAME_SKIP_DEFAULT = 0;

    public static final String BUTTONS_OPACITY = "buttons_opacity";
    public static final float BUTTONS_OPACITY_DEFAULT = 1.0f;

    public static final String LAST_IP = "last_ip";
    public static final String LAST_IP_DEFAULT = "";

    public static final String LAST_PORT = "last_port";
    public static final String LAST_PORT_DEFAULT = "";

    // Portrait buttons positions
    public static final String PORTRAIT_DPAD_X = "portrait_dpad_x";
    public static final float PORTRAIT_DPAD_X_DEFAULT = 0.0f;

    public static final String PORTRAIT_DPAD_Y = "portrait_dpad_y";
    public static final float PORTRAIT_DPAD_Y_DEFAULT = 0.63f;

    public static final String PORTRAIT_A_X = "portrait_a_x";
    public static final float PORTRAIT_A_X_DEFAULT = 0.88f;

    public static final String PORTRAIT_A_Y = "portrait_a_y";
    public static final float PORTRAIT_A_Y_DEFAULT = 0.66f;

    public static final String PORTRAIT_B_X = "portrait_b_x";
    public static final float PORTRAIT_B_X_DEFAULT = 0.75f;

    public static final String PORTRAIT_B_Y = "portrait_b_y";
    public static final float PORTRAIT_B_Y_DEFAULT = 0.73f;

    public static final String PORTRAIT_START_X = "portrait_start_x";
    public static final float PORTRAIT_START_X_DEFAULT = 0.56f;

    public static final String PORTRAIT_START_Y = "portrait_start_y";
    public static final float PORTRAIT_START_Y_DEFAULT = 0.93f;

    public static final String PORTRAIT_SELECT_X = "portrait_select_x";
    public static final float PORTRAIT_SELECT_X_DEFAULT = 0.25f;

    public static final String PORTRAIT_SELECT_Y = "portrait_select_y";
    public static final float PORTRAIT_SELECT_Y_DEFAULT = 0.93f;

    public static final String PORTRAIT_LINK_X = "portrait_link_x";
    public static final float PORTRAIT_LINK_X_DEFAULT = 0.45f;

    public static final String PORTRAIT_LINK_Y = "portrait_link_y";
    public static final float PORTRAIT_LINK_Y_DEFAULT = 0.52f;

    // Landscape buttons positions
    public static final String LANDSCAPE_DPAD_X = "landscape_dpad_x";
    public static final float LANDSCAPE_DPAD_X_DEFAULT = 0.0f;

    public static final String LANDSCAPE_DPAD_Y = "landscape_dpad_y";
    public static final float LANDSCAPE_DPAD_Y_DEFAULT = 0.38f;

    public static final String LANDSCAPE_A_X = "landscape_a_x";
    public static final float LANDSCAPE_A_X_DEFAULT = 0.93f;

    public static final String LANDSCAPE_A_Y = "landscape_a_y";
    public static final float LANDSCAPE_A_Y_DEFAULT = 0.44f;

    public static final String LANDSCAPE_B_X = "landscape_b_x";
    public static final float LANDSCAPE_B_X_DEFAULT = 0.86f;

    public static final String LANDSCAPE_B_Y = "landscape_b_y";
    public static final float LANDSCAPE_B_Y_DEFAULT = 0.56f;

    public static final String LANDSCAPE_START_X = "landscape_start_x";
    public static final float LANDSCAPE_START_X_DEFAULT = 0.79f;

    public static final String LANDSCAPE_START_Y = "landscape_start_y";
    public static final float LANDSCAPE_START_Y_DEFAULT = 0.88f;

    public static final String LANDSCAPE_SELECT_X = "landscape_select_x";
    public static final float LANDSCAPE_SELECT_X_DEFAULT = 0.11f;

    public static final String LANDSCAPE_SELECT_Y = "landscape_select_y";
    public static final float LANDSCAPE_SELECT_Y_DEFAULT = 0.88f;

    public static final String LANDSCAPE_LINK_X = "landscape_link_x";
    public static final float LANDSCAPE_LINK_X_DEFAULT = 0.94f;

    public static final String LANDSCAPE_LINK_Y = "landscape_link_y";
    public static final float LANDSCAPE_LINK_Y_DEFAULT = 0.02f;

    public static final String CAMERA_IS_FRONT = "camera_is_front";
    public static final boolean CAMERA_IS_FRONT_DEFAULT = false;
}
